#pragma once

// LocalPreviewByteCache - on-disk byte cache for CardPreviewService.
//
// Layout under the configured cache directory (composition root passes
// `<exeDir>/.cache/preview-cache/` - next to the executable, NOT under
// the user-configurable `dataStorage` path; see `docs/caching.md` and
// `app/AGENTS.md` for the rationale):
//   <hash>.bin   raw image bytes (PNG/JPEG payload), positive entries only
//   <hash>.neg   zero-byte marker file, negative entries only
//   <hash>.idx   one-line text sidecar holding the original cache key,
//                used to detect (and reject) hash collisions so we never
//                serve the wrong card's image and never honor a stale
//                negative entry across collisions
//
// Positive vs. negative entries are mutually exclusive for a given hash:
// `store` removes any existing `.neg`, `storeNegative` removes any existing
// `.bin`, and `load` prefers `.bin` on the off chance both somehow co-exist.
//
// The cache is bounded by total payload bytes (sum of `.bin` sizes). When
// `store` would push it past the cap we evict by file mtime (oldest first)
// until back under the cap; the `.idx` sidecar of an evicted entry is
// removed too. Negative entries are tiny (effectively `.idx` only) and are
// not subject to the byte cap directly - their count is naturally bounded
// by the user's collection size since a negative entry only ever exists
// for a card the user has actually looked at and the upstream answered
// "no image" for. Reads update mtime via a touch on hit so frequently-
// viewed cards survive eviction.
//
// All filesystem mutations go through `IFileSystem` (so the in-memory
// fake works in tests). Size and mtime queries - which the port does not
// expose - use `std::filesystem` directly inside this adapter. Tests that
// need to drive eviction stay easy to write: just call `store` past the cap
// and check the survivors.

#include "ccm/ports/IFileSystem.hpp"
#include "ccm/ports/IPreviewByteCache.hpp"

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>

namespace ccm {

class LocalPreviewByteCache final : public IPreviewByteCache {
public:
    // Default soft cap: ~64 MiB. A typical preview is 80-200 KiB, so this
    // holds several hundred cards comfortably while keeping disk usage
    // bounded for users with very large collections.
    static constexpr std::size_t kDefaultMaxBytes = 64ull * 1024 * 1024;

    LocalPreviewByteCache(IFileSystem& fs,
                          std::filesystem::path cacheDir,
                          std::size_t maxBytes = kDefaultMaxBytes);

    [[nodiscard]] LoadResult load(std::string_view key) override;
    void store(std::string_view key, const std::string& payload) override;
    void storeNegative(std::string_view key) override;

    // Test-visible knob: total payload bytes currently on disk (recomputed
    // from the directory listing so it stays accurate after external
    // tampering). Negative-entry markers do not count toward the total.
    [[nodiscard]] std::size_t currentSizeBytes();

private:
    std::filesystem::path payloadPath(const std::string& hash) const;
    std::filesystem::path negativePath(const std::string& hash) const;
    std::filesystem::path indexPath(const std::string& hash) const;

    // Hex-encoded FNV-1a 64-bit hash of the key. We don't need cryptographic
    // strength; the sidecar `.idx` file rejects collisions on load so the
    // worst case is a one-time cache miss.
    static std::string hashKey(std::string_view key);

    void evictIfNeededLocked(std::size_t incomingBytes);

    IFileSystem& fs_;
    std::filesystem::path cacheDir_;
    std::size_t maxBytes_;
    std::mutex mutex_;
};

}  // namespace ccm
