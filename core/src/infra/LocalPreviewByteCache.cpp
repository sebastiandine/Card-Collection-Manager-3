#include "ccm/infra/LocalPreviewByteCache.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace ccm {

namespace fs = std::filesystem;

namespace {

// FNV-1a 64-bit hash, hex-encoded. We don't need cryptographic strength
// here: the `.idx` sidecar file holds the original key and load() rejects
// any mismatch, so a hash collision degrades to a cache miss instead of a
// wrong-image return. FNV-1a was picked to keep this dependency-free
// (no openssl, no extra link).
std::string fnv1a64Hex(std::string_view in) {
    constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ULL;
    constexpr std::uint64_t kPrime       = 0x100000001b3ULL;
    std::uint64_t h = kOffsetBasis;
    for (unsigned char c : in) {
        h ^= c;
        h *= kPrime;
    }
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

// Best-effort mtime; returns the epoch on any error so callers can still
// sort consistently (oldest-first eviction stays well-defined).
fs::file_time_type mtimeOrEpoch(const fs::path& p) {
    std::error_code ec;
    auto t = fs::last_write_time(p, ec);
    if (ec) return fs::file_time_type{};
    return t;
}

std::uintmax_t fileSizeOrZero(const fs::path& p) {
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    return ec ? 0u : sz;
}

void touchMtime(const fs::path& p) {
    std::error_code ec;
    fs::last_write_time(p, fs::file_time_type::clock::now(), ec);
    // Ignored: touch is a best-effort hint to the LRU policy.
}

}  // namespace

LocalPreviewByteCache::LocalPreviewByteCache(IFileSystem& fs,
                                             fs::path cacheDir,
                                             std::size_t maxBytes)
    : fs_(fs), cacheDir_(std::move(cacheDir)), maxBytes_(maxBytes) {}

std::string LocalPreviewByteCache::hashKey(std::string_view key) {
    return fnv1a64Hex(key);
}

fs::path LocalPreviewByteCache::payloadPath(const std::string& hash) const {
    return cacheDir_ / (hash + ".bin");
}

fs::path LocalPreviewByteCache::negativePath(const std::string& hash) const {
    return cacheDir_ / (hash + ".neg");
}

fs::path LocalPreviewByteCache::indexPath(const std::string& hash) const {
    return cacheDir_ / (hash + ".idx");
}

IPreviewByteCache::LoadResult LocalPreviewByteCache::load(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    const std::string hash = hashKey(key);
    const auto bin = payloadPath(hash);
    const auto neg = negativePath(hash);
    const auto idx = indexPath(hash);

    const bool hasBin = fs_.exists(bin);
    const bool hasNeg = fs_.exists(neg);
    if (!hasBin && !hasNeg) return {HitKind::Miss, {}};

    // Sidecar must exist and match exactly. Anything else - missing,
    // mismatched, empty - is treated as a miss so the next store() / 
    // storeNegative() will overwrite cleanly. This is what guarantees that
    // a hash collision can never serve another card's bytes or stale
    // "no image" verdict.
    if (!fs_.exists(idx)) return {HitKind::Miss, {}};
    auto idxRead = fs_.readText(idx);
    if (!idxRead) return {HitKind::Miss, {}};
    if (idxRead.value() != key) return {HitKind::Miss, {}};

    if (hasBin) {
        auto payload = fs_.readText(bin);
        if (!payload) return {HitKind::Miss, {}};
        // Touch mtime so this hit moves to the front of the LRU.
        touchMtime(bin);
        return {HitKind::Hit, std::move(payload).value()};
    }
    // Negative-only entry. Touch its mtime as well so frequently-checked
    // negatives don't get aged out by an arbitrary directory sweep.
    touchMtime(neg);
    return {HitKind::NegativeHit, {}};
}

void LocalPreviewByteCache::store(std::string_view key, const std::string& payload) {
    if (payload.empty()) return;

    std::lock_guard<std::mutex> lock(mutex_);

    auto ensure = fs_.ensureDirectory(cacheDir_);
    if (!ensure) return;

    const std::string hash = hashKey(key);
    const auto bin = payloadPath(hash);
    const auto neg = negativePath(hash);
    const auto idx = indexPath(hash);

    // If a negative entry exists for this exact key, drop it before writing
    // the positive payload so the two are never co-resident on disk.
    if (fs_.exists(neg)) (void)fs_.remove(neg);

    // Eviction runs against the *new* payload size, not the post-write
    // total, so we make room before writing. If the same key is being
    // overwritten the existing payload's bytes are released first.
    evictIfNeededLocked(payload.size());

    auto wrote = fs_.writeText(bin, payload);
    if (!wrote) return;
    auto wroteIdx = fs_.writeText(idx, std::string(key));
    if (!wroteIdx) {
        // Sidecar failure leaves us with bytes we can't safely serve later.
        // Roll back the payload write so a future load() doesn't see it.
        (void)fs_.remove(bin);
        return;
    }
}

void LocalPreviewByteCache::storeNegative(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto ensure = fs_.ensureDirectory(cacheDir_);
    if (!ensure) return;

    const std::string hash = hashKey(key);
    const auto bin = payloadPath(hash);
    const auto neg = negativePath(hash);
    const auto idx = indexPath(hash);

    // Replace any existing positive entry: storeNegative is the upstream
    // saying "the previous bytes are no longer the correct answer for this
    // record". Free the bytes from the size cap immediately.
    if (fs_.exists(bin)) (void)fs_.remove(bin);

    // Order matters: write the marker first, then the sidecar. If the
    // sidecar write fails we delete the marker to avoid a half-written
    // entry that load() would treat as a miss anyway but that contributes
    // a stray file to the directory listing.
    auto wroteNeg = fs_.writeText(neg, std::string{});
    if (!wroteNeg) return;
    auto wroteIdx = fs_.writeText(idx, std::string(key));
    if (!wroteIdx) {
        (void)fs_.remove(neg);
    }
}

std::size_t LocalPreviewByteCache::currentSizeBytes() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto entries = fs_.listDirectory(cacheDir_);
    if (!entries) return 0;
    std::size_t total = 0;
    for (const auto& p : entries.value()) {
        if (p.extension() == ".bin") total += static_cast<std::size_t>(fileSizeOrZero(p));
    }
    return total;
}

void LocalPreviewByteCache::evictIfNeededLocked(std::size_t incomingBytes) {
    auto entries = fs_.listDirectory(cacheDir_);
    if (!entries) return;

    struct Entry {
        fs::path bin;
        fs::path idx;
        std::uintmax_t size;
        fs::file_time_type mtime;
    };
    std::vector<Entry> bins;
    bins.reserve(entries.value().size());
    std::size_t total = 0;
    for (const auto& p : entries.value()) {
        if (p.extension() != ".bin") continue;
        Entry e;
        e.bin = p;
        e.idx = p;
        e.idx.replace_extension(".idx");
        e.size = fileSizeOrZero(p);
        e.mtime = mtimeOrEpoch(p);
        total += static_cast<std::size_t>(e.size);
        bins.push_back(std::move(e));
    }

    if (total + incomingBytes <= maxBytes_) return;

    std::sort(bins.begin(), bins.end(),
              [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });

    for (const auto& e : bins) {
        if (total + incomingBytes <= maxBytes_) break;
        // remove() is best-effort; if it fails we still drop our accounting
        // for the entry so we don't loop forever on a stuck file.
        (void)fs_.remove(e.bin);
        (void)fs_.remove(e.idx);
        total -= std::min<std::size_t>(static_cast<std::size_t>(e.size), total);
    }
}

}  // namespace ccm
