#pragma once

// CardPreviewService: high-level operation for resolving and downloading a
// card's preview image from the active game's external API. Mirrors the
// `EntryPanelTemplate.tsx::useEffect([entry])` flow:
//   1. Per-game `ICardPreviewSource` resolves the card -> an image URL.
//   2. Service issues a GET against that URL through `IHttpClient`.
//   3. Raw bytes are returned to the caller (UI decodes them with whichever
//      image lib it prefers - we use wxImage in the wx adapter).
//
// Sources are registered through `IGameModule::cardPreviewSource()`. Modules
// that return nullptr are silently skipped; games without a registered source
// produce an explicit error result on lookup rather than silently doing nothing.

#include "ccm/domain/Enums.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"
#include "ccm/ports/IPreviewByteCache.hpp"
#include "ccm/util/Result.hpp"

#include <cstddef>
#include <list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace ccm {

class CardPreviewService {
public:
    explicit CardPreviewService(IHttpClient& http,
                                IPreviewByteCache* persistentCache = nullptr);

    // Register a game module's preview source. Calling this with a module
    // whose `cardPreviewSource()` returns nullptr is a no-op (the game has
    // no remote preview API). The module reference must remain valid for
    // the lifetime of the service.
    void registerModule(IGameModule& module);

    // Resolve and download the preview image bytes for a single card.
    // The returned `std::string` is a raw byte buffer (PNG/JPEG payload) -
    // it is NOT decoded text. Use std::string::data()/size() with whatever
    // image-decoding facility your UI provides.
    //
    // Successful results are cached in two tiers, both keyed by
    // (game, name, setId, setNo):
    //   1. In-memory LRU (bounded by `kCacheCapacity`) for instant hits
    //      while the app is running.
    //   2. Optional persistent byte cache (passed at construction) so
    //      previews survive app restarts.
    // Re-selecting the same row is then a memcpy away from the wxImage
    // decoder, no HTTP at all - this is the common user-facing case
    // (clicking around the table).
    //
    // Failures are split into two policies based on
    // `PreviewLookupError::Kind`:
    //   * `NotFound` (the upstream answered cleanly that this record has
    //     no preview) is *negative-cached* in both tiers, so subsequent
    //     selections short-circuit without touching the network. The
    //     cache key is invalidated automatically when the user edits a
    //     lookup-relevant field of the record.
    //   * `Transient` (HTTP / network / parse failure) is NEVER cached, so
    //     the next selection retries cleanly once connectivity is back.
    Result<std::string> fetchPreviewBytes(Game game,
                                          std::string_view name,
                                          std::string_view setId,
                                          std::string_view setNo);

    Result<AutoDetectedPrint> detectFirstPrint(Game game,
                                               std::string_view name,
                                               std::string_view setId);

    // Download image bytes from a fully-qualified URL without going through
    // per-game preview-source resolution. Cached by URL (same LRU bound).
    Result<std::string> fetchImageBytesByUrl(std::string_view url);

    // Maximum number of cached preview entries kept in memory. Picked so a
    // typical Yu-Gi-Oh! collection page can scroll up and down without
    // re-hitting the network, while keeping a hard upper bound on RSS for
    // very large collections (each entry is roughly one PNG, <100 KiB).
    static constexpr std::size_t kCacheCapacity = 128;

private:
    enum class CacheLookupKind {
        Miss,         // not in the in-memory tier
        Hit,          // positive entry; bytes returned via outPayload
        NegativeHit,  // negative entry; outPayload is empty
    };

    Result<std::string> fetchAndCache(const std::string& cacheKey,
                                      std::string_view url);

    // Returns the kind of in-memory cache entry for `key`. On Hit the
    // payload is copied into `outPayload`; on NegativeHit `outPayload` is
    // cleared. Both Hit and NegativeHit move the entry to the front of
    // the LRU.
    CacheLookupKind cacheLookup(const std::string& key, std::string& outPayload);
    void cacheStore(const std::string& key, std::string payload);
    void cacheStoreNegative(const std::string& key);

    IHttpClient&        http_;
    IPreviewByteCache*  persistentCache_{nullptr};
    std::unordered_map<Game, ICardPreviewSource*> sources_;

    // LRU: list holds entries in MRU-first order; map points at list nodes
    // for O(1) move-to-front. Mutex covers both list and map - lookups
    // happen on a worker thread spawned by BaseSelectedCardPanel.
    //
    // A `negative` entry has an empty payload by convention; we keep the
    // flag explicit (rather than abusing emptiness) so future invariants
    // around eviction or stats stay easy to reason about.
    struct CacheEntry {
        std::string key;
        std::string payload;
        bool        negative{false};
    };
    using CacheList = std::list<CacheEntry>;
    CacheList cacheList_;
    std::unordered_map<std::string, CacheList::iterator> cacheIndex_;
    std::mutex cacheMutex_;
};

}  // namespace ccm
