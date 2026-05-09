#include "ccm/services/CardPreviewService.hpp"

#include <string>
#include <utility>

namespace ccm {

namespace {

// Compose a stable cache key from the four lookup coordinates. Using NUL as
// a separator keeps the key unambiguous even if a card's name happens to
// contain `|` or other punctuation.
std::string makePreviewKey(Game game,
                           std::string_view name,
                           std::string_view setId,
                           std::string_view setNo) {
    std::string k;
    k.reserve(2 + name.size() + setId.size() + setNo.size() + 3);
    k.push_back('p');
    k.push_back(static_cast<char>(static_cast<int>(game)));
    k.push_back('\0');
    k.append(name);
    k.push_back('\0');
    k.append(setId);
    k.push_back('\0');
    k.append(setNo);
    return k;
}

std::string makeUrlKey(std::string_view url) {
    std::string k;
    k.reserve(url.size() + 1);
    k.push_back('u');
    k.append(url);
    return k;
}

}  // namespace

CardPreviewService::CardPreviewService(IHttpClient& http,
                                       IPreviewByteCache* persistentCache)
    : http_(http), persistentCache_(persistentCache) {}

void CardPreviewService::registerModule(IGameModule& module) {
    if (auto* src = module.cardPreviewSource(); src != nullptr) {
        sources_[module.id()] = src;
    }
}

CardPreviewService::CacheLookupKind CardPreviewService::cacheLookup(
    const std::string& key, std::string& outPayload) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cacheIndex_.find(key);
    if (it == cacheIndex_.end()) {
        outPayload.clear();
        return CacheLookupKind::Miss;
    }
    // Move-to-front to mark as most-recently-used.
    cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
    if (it->second->negative) {
        outPayload.clear();
        return CacheLookupKind::NegativeHit;
    }
    outPayload = it->second->payload;
    return CacheLookupKind::Hit;
}

void CardPreviewService::cacheStore(const std::string& key, std::string payload) {
    if (payload.empty()) return;
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cacheIndex_.find(key);
    if (it != cacheIndex_.end()) {
        // Overwrite existing entry (positive or negative) and bump it to
        // the front. Replacing a negative entry is the "we got a real
        // image after a previous NotFound" path - rare but valid.
        it->second->payload  = std::move(payload);
        it->second->negative = false;
        cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
        return;
    }
    cacheList_.push_front({key, std::move(payload), /*negative=*/false});
    cacheIndex_.emplace(key, cacheList_.begin());
    while (cacheList_.size() > kCacheCapacity) {
        cacheIndex_.erase(cacheList_.back().key);
        cacheList_.pop_back();
    }
}

void CardPreviewService::cacheStoreNegative(const std::string& key) {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    auto it = cacheIndex_.find(key);
    if (it != cacheIndex_.end()) {
        it->second->payload.clear();
        it->second->negative = true;
        cacheList_.splice(cacheList_.begin(), cacheList_, it->second);
        return;
    }
    cacheList_.push_front({key, std::string{}, /*negative=*/true});
    cacheIndex_.emplace(key, cacheList_.begin());
    while (cacheList_.size() > kCacheCapacity) {
        cacheIndex_.erase(cacheList_.back().key);
        cacheList_.pop_back();
    }
}

Result<std::string> CardPreviewService::fetchAndCache(const std::string& cacheKey,
                                                      std::string_view url) {
    auto bytes = http_.get(url);
    if (!bytes) return Result<std::string>::err(bytes.error());
    std::string payload = std::move(bytes).value();
    cacheStore(cacheKey, payload);
    // Best-effort persist to disk so the next app launch starts warm.
    // The persistent tier is fire-and-forget: any I/O error is swallowed
    // by the adapter, the in-memory tier still holds the bytes.
    if (persistentCache_ != nullptr) {
        persistentCache_->store(cacheKey, payload);
    }
    return Result<std::string>::ok(std::move(payload));
}

Result<std::string> CardPreviewService::fetchPreviewBytes(Game game,
                                                          std::string_view name,
                                                          std::string_view setId,
                                                          std::string_view setNo) {
    auto it = sources_.find(game);
    if (it == sources_.end() || it->second == nullptr) {
        return Result<std::string>::err("No preview source registered for this game.");
    }

    // Cache check before any HTTP call. The (game, name, setId, setNo) tuple
    // uniquely identifies a printing for our purposes - the resolved image
    // URL is always a deterministic function of those four inputs, and any
    // edit to a lookup-relevant field changes the key automatically.
    const std::string key = makePreviewKey(game, name, setId, setNo);
    std::string cached;
    switch (cacheLookup(key, cached)) {
        case CacheLookupKind::Hit:
            return Result<std::string>::ok(std::move(cached));
        case CacheLookupKind::NegativeHit:
            return Result<std::string>::err("No preview available for this card.");
        case CacheLookupKind::Miss:
            break;
    }
    // Disk-backed second tier: previews persisted by an earlier app run
    // get promoted into the in-memory LRU on first access this session, so
    // subsequent re-selections stay fast without re-touching the network.
    // Negative entries on disk are likewise promoted - the user already
    // knows from a previous session that this record has no upstream image.
    if (persistentCache_ != nullptr) {
        const auto disk = persistentCache_->load(key);
        switch (disk.kind) {
            case IPreviewByteCache::HitKind::Hit:
                cacheStore(key, disk.payload);
                return Result<std::string>::ok(disk.payload);
            case IPreviewByteCache::HitKind::NegativeHit:
                cacheStoreNegative(key);
                return Result<std::string>::err("No preview available for this card.");
            case IPreviewByteCache::HitKind::Miss:
                break;
        }
    }

    auto url = it->second->fetchImageUrl(name, setId, setNo);
    if (!url) {
        // The two error kinds split here:
        //   - NotFound: upstream answered cleanly that this record has no
        //     image. Persist the verdict so we don't keep retrying.
        //   - Transient: network/HTTP/parse failure. Surface the error
        //     unchanged and DO NOT cache anything; the next selection
        //     retries from scratch.
        const auto err = std::move(url).error();
        if (err.kind == PreviewLookupError::Kind::NotFound) {
            cacheStoreNegative(key);
            if (persistentCache_ != nullptr) persistentCache_->storeNegative(key);
        }
        return Result<std::string>::err(err.message);
    }
    return fetchAndCache(key, url.value());
}

Result<AutoDetectedPrint> CardPreviewService::detectFirstPrint(Game game,
                                                               std::string_view name,
                                                               std::string_view setId) {
    auto it = sources_.find(game);
    if (it == sources_.end() || it->second == nullptr) {
        return Result<AutoDetectedPrint>::err("No preview source registered for this game.");
    }
    if (!it->second->supportsAutoDetectPrint()) {
        return Result<AutoDetectedPrint>::err("Auto-detect not enabled for this game.");
    }
    return it->second->detectFirstPrint(name, setId);
}

Result<std::string> CardPreviewService::fetchImageBytesByUrl(std::string_view url) {
    // The by-URL path is used for fixed per-game card-back fallback images.
    // A failure there is always transient (the URL itself is constant), so
    // there is no negative-cache analogue to worry about; we just look up
    // and, if needed, fetch+store.
    const std::string key = makeUrlKey(url);
    std::string cached;
    switch (cacheLookup(key, cached)) {
        case CacheLookupKind::Hit:
            return Result<std::string>::ok(std::move(cached));
        case CacheLookupKind::NegativeHit:
            // Defensive: nothing in this code path ever stores a negative
            // entry under a URL key, but if one ever ends up here (cache
            // file tampering, future code paths) treat it as a miss so the
            // fallback fetch can still run.
            break;
        case CacheLookupKind::Miss:
            break;
    }
    if (persistentCache_ != nullptr) {
        const auto disk = persistentCache_->load(key);
        if (disk.kind == IPreviewByteCache::HitKind::Hit) {
            cacheStore(key, disk.payload);
            return Result<std::string>::ok(disk.payload);
        }
    }
    return fetchAndCache(key, url);
}

}  // namespace ccm
