#documentation #performance #network #ccm3

# Caching In CCM3

This document describes **what CCM3 caches**, **how lookups are ordered**, and **what is deliberately not cached**. It focuses on the card **preview image** path (remote APIs → raw bytes → UI decode), which is where most explicit caching lives. For upstream URL shapes and API roles, see [assets-and-info-apis.md](assets-and-info-apis.md).

## Scope

| Mechanism | What it stores | Survives restart? |
|-----------|----------------|-------------------|
| In-memory preview LRU (`CardPreviewService`) | Successful preview / fallback-image **bytes** + negative markers for "no upstream image" | No |
| Disk preview byte cache (`LocalPreviewByteCache`) | Same bytes + negative markers, persisted under `<exeDir>/.cache/preview-cache/` | Yes |
| HTTP session reuse (`CprHttpClient`) | libcurl **connections** (TLS + TCP keep-alive), not response bodies | No (process lifetime only) |

Other persistence (for example `JsonSetRepository` after “Update sets”, `JsonCollectionRepository` for card JSON, `LocalImageStore` for **user-attached** scan files) is normal app data storage, not preview caching. Those layers are documented elsewhere via domain services; this page stays centered on **preview latency** and **repeat lookups**.

### Cache directory layout

The umbrella cache root is `<exeDir>/.cache/`, where `exeDir` is the directory containing the running `ccm3` executable — the same scope as `config.json`. It is reserved for any future computed-from-network caches (set-list response snapshots, etc.); the leading dot keeps it out of the way for users poking around the install folder. Today it contains a single subdirectory:

- `<exeDir>/.cache/preview-cache/` — owned by `LocalPreviewByteCache`. Files inside are `<hash>.bin` / `<hash>.neg` / `<hash>.idx` triples (see below).

The cache is deliberately **not** under the user-configurable `Configuration.dataStorage` path. The data-storage path is meant for the user's own data — collection JSON, attached scans, set lists — and is meant to be relocatable, syncable, and backup-friendly. Previews are downloaded artifacts from upstream APIs:

- They must **not** follow the user's collection when the data-storage path is reconfigured at runtime (the cache would otherwise either rebuild from cold every time the user moves the dir, or pollute every chosen target with a recurring `.cache/` directory).
- They must **not** be uploaded together with the user's collection if the user backs up / syncs / version-controls the data dir.
- They are install-scoped, not collection-scoped: a fresh install elsewhere on disk should start cold, and uninstalling / moving the executable should leave nothing stale behind.

Pinning the cache to `exeDir` is what gives all three properties without per-flow plumbing. The trade-off is that the cache is **not** wiped by the in-app data-storage reset / relocation flow; if you genuinely want a clean slate (e.g. you suspect cache corruption), delete `<exeDir>/.cache/` manually.

## Preview lookup order

For `CardPreviewService::fetchPreviewBytes(game, name, setId, setNo)`:

1. **In-memory LRU** — O(1) lookup. A positive entry returns the bytes immediately; a **negative** entry returns an error immediately (no network call). Either kind of hit moves the entry to the most-recently-used position.
2. **Disk cache** (`IPreviewByteCache`, production: `LocalPreviewByteCache`) — on memory miss, look up on disk. A positive disk hit is **promoted** into the in-memory LRU and returned; a negative disk hit is likewise promoted into memory as a negative entry and returned as an error. So the next selection of the same card stays in-memory only.
3. **Network** — ask `ICardPreviewSource` for an image URL, then `IHttpClient::get(url)` for bytes. On success, write through to **both** memory and disk tiers as a positive entry. On failure, the **error classification** decides what to do (see below).

For `fetchImageBytesByUrl(url)` (used for per-game **card-back fallbacks** when preview lookup fails):

- Same three-tier pattern, but the cache key is derived only from the URL. There is no negative-cache analogue here: the URL is a fixed constant, so any failure is by definition transient.

## Negative caching: transient vs. permanent failures

`fetchPreviewBytes` distinguishes two failure kinds via `PreviewLookupError::Kind`:

- **`NotFound`** — the upstream answered cleanly that this exact record has no preview image. Examples:
  - Scryfall search returned `data: []`, or the matched card has no top-level `image_uris`.
  - Pokémon TCG search returned `data: []`, or the card has no `images` object / no `large` / `small` URL.
  - Yu-Gi-Oh!: **both** Yugipedia and YGOPRODeck answered cleanly with no match (Yugipedia tagged every candidate filename `missing`, *and* YGOPRODeck returned an empty `data` array or no usable image variants).

  These are **negative-cached** in both tiers. Subsequent selections of the same record return an error instantly, without any HTTP call. The user-visible effect is that the per-game card-back fallback shows up immediately on every click.

  The cache key is `(game, name, setId, setNo)` (with Yu-Gi-Oh! also packing rarity and edition into `setNo`). Any **edit to a lookup-relevant field** of the record changes the key automatically, which means the negative entry no longer matches and a fresh network resolution attempt runs the next time the user clicks the row. So if the user fixes a typo, changes the set, switches a YGO printing's edition or rarity, etc., the new fingerprint guarantees a re-fetch — no manual cache clear needed.

- **`Transient`** — we couldn't tell whether the record has an image because the upstream couldn't speak. Examples:
  - HTTP / network / TLS / DNS failure.
  - Malformed JSON, missing top-level fields (schema deviation that suggests an outage page rather than a real "no match" response).
  - Yu-Gi-Oh!: **either** Yugipedia or YGOPRODeck failed at the network/parse layer. The cautious rule is that as soon as one upstream couldn't speak, the overall outcome is transient — we cannot conclude the record has no image, only that we couldn't reach the place that would tell us.

  These are **never cached** (positive or negative). The next selection of the same row retries cleanly. This is the property that keeps a temporary connection drop from semi-permanently breaking previews.

This split is the reason the preview path doesn't keep retrying every click for cards whose printing genuinely has no upstream scan, *and* the reason a brief loss of connectivity doesn't poison the cache with bogus "no image" markers.

## Updating cached entries

There is no explicit "refresh" or "invalidate" API on `CardPreviewService` — by design. Every way an entry's state can change is driven by **what already happens** in the system, so contributors don't have to reason about a side-channel mutation API. The full set of transitions is:

### 1. Edit-driven invalidation (record changed → fresh lookup, automatic)

The cache key for the preview path is `(game, name, setId, setNo)`. For Yu-Gi-Oh! the third slot also encodes rarity and edition, packed by `YuGiOhSelectedCardPanel::previewKey()` as `<setNo>||<rarity>||<1E|UE>`. The user editing **any** lookup-relevant field of a card record produces a **different cache key** for the resulting selection, which means:

- Memory and disk lookups for the new key **miss** the old entry (positive or negative).
- A fresh `ICardPreviewSource::fetchImageUrl` call runs.
- The result is cached under the new key, leaving the old key's entry untouched but unreachable from the UI (it ages out via LRU / mtime eviction).

Concretely: fix a typo in the card name → re-fetch. Switch the printing's set → re-fetch. Toggle `1E` ↔ `UE` on a YGO card → re-fetch the per-printing scan. **No manual cache clear needed**; the test `editing a lookup-relevant field invalidates the negative entry automatically` pins this behavior.

If you add a new disambiguator (say a future "art treatment" flag), the rule is to pack it into one of the existing key slots (`setNo`'s `||`-separated tuple is the established hook) so this auto-invalidation continues to apply. Adding it as a side parameter that the cache key *doesn't* see would silently break the update story.

### 2. Same-key positive ↔ negative state transitions

If a previously cached entry's verdict flips upstream (Yugipedia uploads a missing scan, a Scryfall printing's `image_uris` get fixed, etc.) **and** the user re-encounters it under the same key, the next fetch decides:

- **Positive → negative.** Source returns `NotFound` for a key that previously cached a positive entry: `cacheStoreNegative(key)` overwrites the in-memory entry's bytes with an empty payload and flips `negative=true`; on disk, `LocalPreviewByteCache::storeNegative` removes the existing `<hash>.bin` (releasing its bytes from the size cap) and writes a `<hash>.neg` marker.
- **Negative → positive.** Source returns a real URL, `IHttpClient` returns bytes, `cacheStore(key, payload)` overwrites the existing in-memory entry with the new bytes and flips `negative=false`; on disk, `LocalPreviewByteCache::store` removes the existing `<hash>.neg` and writes the new `<hash>.bin`.

Both paths preserve a key invariant: **`.bin` and `.neg` for the same hash are never co-resident**. `LocalPreviewByteCache` tests pin this down (`a later positive store overwrites an earlier negative entry`, `storeNegative for an existing positive entry replaces the bytes`).

The "trigger" for these transitions in production is one of: the user edits the record back to a previous key (so the still-cached old entry surfaces and a network attempt then re-reaches the upstream), or the in-memory tier was cleared by an app restart and the disk-tier verdict is now stale. There is no time-based revalidation today; the design relies on the upstream answer being stable enough that a stale verdict only hurts until the natural transitions above kick in.

### 3. Eviction-based aging (passive)

- **In-memory LRU.** Capacity is hard-capped at `CardPreviewService::kCacheCapacity = 128` entries (positive and negative share the count). When a new entry is inserted past the cap, the **least-recently-used** entry — the back of the list — is dropped. Any access (positive hit, negative hit, or store) moves the entry to the front, so heavily clicked cards are the last to go.
- **Disk cache.** Capacity is hard-capped by total `.bin` payload bytes (`LocalPreviewByteCache::kDefaultMaxBytes = 64 MiB`). When a `store` would push the total past the cap, oldest-by-mtime `.bin` files (with their `.idx` sidecars) are deleted until the new write fits. A successful `load` touches the entry's mtime, so frequently viewed cards rarely become eviction victims. `.neg` markers are not counted against the cap and are not actively evicted; their count is naturally bounded by the number of records the user has viewed whose upstream cleanly reported "no image".

Eviction is the only way an entry "ages out" without an explicit user action.

### 4. Manual / external invalidation

- **Delete the cache directory.** Removing `<exeDir>/.cache/preview-cache/` (or the umbrella `<exeDir>/.cache/`) is safe: `LocalPreviewByteCache` recreates the directory on the next store. The in-memory tier is unaffected by the disk delete during a running session, but a subsequent app restart starts cold.
- **Reinstall / move the executable.** Because the cache is rooted at `<exeDir>`, a fresh install elsewhere on disk starts cold by construction, and uninstalling / moving the exe leaves no stray cache in the user's data directory. (Note: **resetting or moving the user's data-storage directory does NOT clear the preview cache** — that's intentional; the cache is install-scoped, not collection-scoped.)
- **Tampering with sidecar files.** If a `<hash>.idx` is ever rewritten with a key that doesn't match the requested cache key (corruption, hash collision, filesystem hiccup), `LocalPreviewByteCache::load` reports `Miss` rather than serving the entry. A subsequent `store` / `storeNegative` overwrites the corrupted record cleanly. This is what makes FNV-1a (non-cryptographic) safe to use as the hash: the worst case is a one-time miss, never a wrong answer.

### What does *not* trigger an update

To keep the mental model crisp, the following cases **do not** invalidate or refresh anything:

- **Re-selecting the same row repeatedly.** That's a hit by design — the whole point of the cache. The only thing that changes is the entry's LRU position / mtime.
- **Transient errors on a row that is already negatively cached.** The negative entry is consulted first and short-circuits the call; the network is never touched, so a flaky network can't accidentally turn a `NotFound` verdict into a `Transient` outcome.
- **Restart with a populated disk cache.** This is a *warm start*, not an update. Both positive and negative entries flow back into memory on first re-access via the disk tier. No upstream is consulted, no entries are rewritten.

## In-memory LRU (`CardPreviewService`)

- **Implementation:** Doubly linked list + hash map, guarded by a mutex. Each entry is `{key, payload, negative}`; positive entries hold the bytes, negative entries hold an empty payload and a `negative=true` flag. Hits move the entry to the front regardless of kind.
- **Capacity:** `CardPreviewService::kCacheCapacity` (128 entries — positive and negative entries share this count).
- **Threading:** Preview work can run on a worker thread from the UI layer; all cache access goes through the mutex.
- **Keys:** Internal strings built in `CardPreviewService.cpp`:
  - Preview path: prefix `'p'`, then NUL-separated fields: enum `game`, `name`, `setId`, `setNo`. The `setNo` string may embed game-specific disambiguators (for example Yu-Gi-Oh! packs rarity and edition into `setNo` before it reaches the service — see `YuGiOhSelectedCardPanel::previewKey()`).
  - URL path: prefix `'u'` plus the full URL string.

Callers should treat the key as opaque; **correctness** depends on passing stable `(game, name, setId, setNo)` (and stable URL for fallback fetches) so the same printing always maps to the same cache entry.

## Disk byte cache (`LocalPreviewByteCache`)

- **Root directory:** `<exeDir>/.cache/preview-cache/` (created on first store). Pinned next to the executable, **not** under the user-configurable `Configuration.dataStorage` path — see "Cache directory layout" above for the rationale.
- **Files per logical entry:** mutually-exclusive `.bin` / `.neg`, plus an always-present `.idx` sidecar:
  - `<hash>.bin` — raw image bytes (PNG/JPEG payload). **Positive** entry.
  - `<hash>.neg` — zero-byte marker file. **Negative** entry (the upstream cleanly said "no image").
  - `<hash>.idx` — text sidecar holding the **exact** cache key string used by `CardPreviewService`. Used to reject hash collisions on load — if `.idx` does not match the requested key, the entry is treated as a miss regardless of which marker file is present.

  `store()` removes any existing `.neg` for the same hash; `storeNegative()` removes any existing `.bin`. The two states never co-exist. If they ever somehow did, `load()` prefers the `.bin` (more useful answer).

- **Hash:** FNV-1a 64-bit over the key, rendered as 16 hex digits. Not cryptographic; the `.idx` sidecar is the safety net that prevents collisions from serving the wrong card's bytes or the wrong card's "no image" verdict.
- **Size bound:** Default total payload cap `LocalPreviewByteCache::kDefaultMaxBytes` (64 MiB). Eviction removes **oldest by modification time** among `.bin` files (with their `.idx` sidecars) until the new write fits. **Negative entries** (`.neg` markers) are tiny and are not counted against the cap — their count is naturally bounded by the user's actively-viewed records.
- **Recency on read:** A successful `load` updates the corresponding `.bin` or `.neg` file's mtime (“touch”) so frequently viewed cards are less likely to be evicted.
- **Failure policy:** All adapter I/O failures are swallowed (miss on read, no-op on failed write). Preview still works from network; worst case is “cold” performance.

### Clearing the disk preview cache

- Delete the `<exeDir>/.cache/` folder (or just the `preview-cache/` subfolder inside it). Both options are safe: `LocalPreviewByteCache` recreates the directory on the next store.
- The in-app data-storage reset/relocation flow does **not** touch this directory — the cache is install-scoped (sits next to the exe), not collection-scoped. If you need a clean slate for the cache, delete the directory above explicitly.

## HTTP connection reuse (`CprHttpClient`)

The app constructs **one** `CprHttpClient` and shares it across set sources, preview sources, and image downloads. It owns a single long-lived `cpr::Session` (one libcurl easy handle per process).

- **Benefit:** Repeated HTTPS requests to the **same host** reuse TLS sessions / TCP connections where the server allows keep-alive, which materially reduces latency vs. a fresh session per GET (especially for Yu-Gi-Oh!, where preview resolution and the actual image often hit different hosts).
- **Thread safety:** All `get()` calls are serialized with a mutex because libcurl easy handles are not thread-safe.

This is **not** a response-body cache; it only amortizes connection setup.

## Design constraints (for contributors)

- **Classify source errors honestly.** A new game module's `ICardPreviewSource::fetchImageUrl` must return `PreviewLookupError::Kind::NotFound` only when the upstream answered cleanly (parsed response, no match / no image variants). Anything that could be the network — HTTP error, malformed body, schema deviation, timeout — is `Transient`.
- **Do not cache transient errors.** That's the rule that keeps a flaky connection from permanently disabling previews. If you ever need to record a failure, route it through `IPreviewByteCache::storeNegative` only on a confirmed `NotFound`.
- **Updates flow through the cache key, not a side channel.** Don't add a `clearCache(...)` / `invalidate(...)` API to `CardPreviewService` to "fix" a stale entry. The supported update mechanic is: edit-driven invalidation (key changes), same-key positive/negative replacement on the next successful resolution, and LRU/mtime eviction (see "Updating cached entries" above). A side-channel invalidation API would just be another way for callers to forget to keep the disk tier in sync with the memory tier.
- **Extend cache keys** by packing new disambiguators into existing coordinates (typically `setNo` / tuple encoding) rather than bypassing `CardPreviewService`, so memory and disk tiers stay aligned and editing the record continues to invalidate the negative entry automatically.
- **Tests:**
  - `card_preview_service_tests.cpp` pins tier ordering and write-through using an in-memory `IPreviewByteCache` fake; it also exercises the negative-caching behavior end-to-end (NotFound is remembered, Transient is retried, edits invalidate the entry, warm-restart honors the disk negative entry, a later positive overwrites a previous negative).
  - `local_preview_byte_cache_tests.cpp` exercises the real-disk adapter in isolated temp directories, including the `.bin`/`.neg`/`.idx` interactions (round-trip, restart, mutual replacement, sidecar collision rejection, eviction).

## Related reading

- [assets-and-info-apis.md](assets-and-info-apis.md) — external APIs and the same preview tiers in **runtime flow** context.
- Root `AGENTS.md` — UI performance guardrails summary.
- `core/AGENTS.md` — conventions for preview caching, source-error classification, and `CprHttpClient` session ownership.
