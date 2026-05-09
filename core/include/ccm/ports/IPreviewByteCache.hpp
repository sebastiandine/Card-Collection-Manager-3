#pragma once

// IPreviewByteCache - persistent byte cache used by CardPreviewService to
// keep preview images alive across app restarts.
//
// The cache is keyed by an opaque string. CardPreviewService composes the
// key from `(game, name, setId, setNo)` (preview lookups) or directly from
// the URL (per-game card-back fallback fetches); the cache itself does not
// interpret the key, only stores the byte payload behind it.
//
// Two kinds of entries are persisted:
//
//   * Positive entries hold raw image bytes. Stored via `store(key, payload)`,
//     returned as `LoadResult{HitKind::Hit, payload}`.
//   * Negative entries record "we tried to resolve this exact card and the
//     upstream answered cleanly that it has no preview image" - i.e. the
//     `NotFound` half of `PreviewLookupError`. Stored via
//     `storeNegative(key)`, returned as `LoadResult{HitKind::NegativeHit, {}}`.
//     `Transient` errors (HTTP / network / parse failures) must NEVER reach
//     this cache: we cannot tell whether the record genuinely has no image
//     or just couldn't be reached, and persisting the miss would leave the
//     user staring at the card-back placeholder until they edit the card.
//
// A negative entry is implicitly invalidated when the cache key changes -
// since the key includes `(game, name, setId, setNo)` (with game-specific
// disambiguators packed into setNo), any edit that affects a lookup-relevant
// field will hit a fresh key and re-attempt the network lookup automatically.
//
// Implementations must be thread-safe with respect to concurrent load/store
// calls because CardPreviewService is invoked from a worker thread spawned
// by `BaseSelectedCardPanel`.
//
// Errors are intentionally swallowed (load returns Miss; store and
// storeNegative are fire-and-forget). A flaky or full disk must never break
// the preview path - in the worst case the user sees the same speed as a
// fresh app install.

#include <string>
#include <string_view>

namespace ccm {

class IPreviewByteCache {
public:
    enum class HitKind {
        Miss,         // no entry for this key (or unrecoverable I/O error)
        Hit,          // positive entry; bytes are in `payload`
        NegativeHit,  // negative entry; `payload` is empty by contract
    };

    struct LoadResult {
        HitKind     kind{HitKind::Miss};
        std::string payload;  // only meaningful when kind == Hit
    };

    virtual ~IPreviewByteCache() = default;

    // Returns the cached entry for `key`. On any error - missing files,
    // sidecar mismatch, malformed metadata, I/O failure - implementations
    // must report `HitKind::Miss` rather than surfacing the error.
    [[nodiscard]] virtual LoadResult load(std::string_view key) = 0;

    // Best-effort persist of `payload` under `key`. Empty payloads are not
    // stored as positive entries. If a negative entry already exists for
    // this key it is replaced. Errors are swallowed.
    virtual void store(std::string_view key, const std::string& payload) = 0;

    // Best-effort persist of "we tried, upstream cleanly said no image".
    // If a positive entry already exists for this key it is replaced.
    // Errors are swallowed. Must be invoked ONLY for `NotFound`-class
    // outcomes; never for transient failures.
    virtual void storeNegative(std::string_view key) = 0;
};

}  // namespace ccm
