#documentation #apis #integrations #ccm3

# Asset And Info APIs

This document explains which external APIs Card Collection Manager 3 uses, and what each API is responsible for in the app. Use this page with [adding-a-new-game.md](adding-a-new-game.md) when you are wiring a new game module or debugging API behavior.

## API Roles

The code separates remote APIs into two roles: **info APIs** and **asset APIs**. Info APIs provide set metadata used to populate local set lists (ID, name, release date). Asset APIs resolve a card lookup into an image URL, then `CardPreviewService` downloads the raw preview image bytes for the UI.

## Magic: The Gathering APIs

**Info API:** `https://api.scryfall.com/sets`  
Used by `MagicSetSource` to fetch all sets. The parser drops digital-only sets, maps Scryfall fields to the internal `Set` type, rewrites `released_at` from `YYYY-MM-DD` to `YYYY/MM/DD`, and sorts ascending by release date.

**Asset API:** `https://api.scryfall.com/cards/search?q=...`  
Used by `MagicCardPreviewSource` to find a card printing from `name` + `setId`, then extract `data[0].image_uris.normal` as the preview URL. The search query is percent-encoded and card names apply `&` -> `and` normalization before lookup.

## Pokemon APIs

**Info API:** `https://api.pokemontcg.io/v2/sets`  
Used by `PokemonSetSource` to fetch all sets. The parser maps `id`, `name`, and `releaseDate` directly into `Set`, then sorts ascending by release date.

**Asset API:** `https://api.pokemontcg.io/v2/cards?q=...`  
Used by `PokemonCardPreviewSource` to search by `name` plus optional `set.id` and collector number. It extracts `data[0].images.large` first and falls back to `images.small` if needed.

The Pokemon source also normalizes collector numbers before request build. For example, `4/102` is reduced to `4` because the remote query expects only the printed number component.

## Yu-Gi-Oh! APIs (Yugipedia + YGOPRODeck)

Yu-Gi-Oh! splits its remote calls across two upstreams. **Yugipedia** is the primary preview source because it hosts actual per-printing card scans; **YGOPRODeck** continues to drive set listings and the auto-detect-first-print helper, plus a last-resort image fallback.

Upstream documentation:
- [Yugipedia MediaWiki API help](https://yugipedia.com/api.php?action=help) (standard MediaWiki action API; we only need `prop=imageinfo`).
- [Yu-Gi-Oh! API Guide — YGOPRODeck](https://ygoprodeck.com/api-guide/). CCM3 uses **v7** endpoints only.

### Info API: YGOPRODeck `cardsets.php`

`https://db.ygoprodeck.com/api/v7/cardsets.php`  
Used by `YuGiOhSetSource`. The response is a top-level JSON array. Each object maps `set_code` → internal `Set.id`, `set_name` → `Set.name`, and `tcg_date` → `Set.releaseDate` with `-` rewritten to `/` for consistency with other games’ date strings. Results are sorted ascending by `releaseDate`.

### Asset API: Yugipedia `api.php` (primary)

`https://yugipedia.com/api.php?action=query&prop=imageinfo&iiprop=url&titles=...`  
Used by `YuGiOhCardPreviewSource::fetchImageUrl` for the actual per-printing card scan. Yugipedia is the only public source we have found that distinguishes art between same-passcode reprints (LOB Blue-Eyes vs SDK Blue-Eyes, for example), and uses a **deterministic file-name convention** of the shape `<Slug>-<SET>-<REGION>-<RARITY>-<EDITION>[-Misc].<png|jpg>` per [Yugipedia’s image policy](https://yugipedia.com/wiki/Yugipedia:Image_policy).

The UI passes a positional tuple in `setNo` of the form `set_code||rarity||edition` (for example `SDK-001||Ultra Rare||UE`); the source splits on `||` before building filenames. Field meanings:

- `set_code` — full code as printed (`LOB-005`, `SDK-001`, `RA04-EN001`). Everything before the first `-` becomes the Yugipedia `<SET>` slot (`LOB`, `SDK`, `RA04`).
- `rarity` — full English rarity name from the edit dialog (`Ultra Rare` → `UR`, `Quarter Century Secret Rare` → `QCScR`, …). The canonical short-form mapping lives in `ygoRarityShortCode(...)` (`core/include/ccm/util/YuGiOhPrintingSlot.hpp`) and is reused by both the Yu-Gi-Oh overview-table rarity rendering and preview filename construction (`rarityCodeFor(...)`). Unknown values fall back to the rarity-less filename pattern.
- `edition` — `1E` when the user marked the card as 1st Edition, otherwise `UE` (Unlimited).

`buildCandidateFilenames(...)` then produces a priority-ordered list:

1. Printed edition first (`1E` then `UE`, or `UE` then `1E` for non-first), with `LE` last for promo-style prints.
2. English regions only — `EN`, then `NA`, then `EU`, then `AU`. **Yugipedia is queried with English regions regardless of the card’s stored Language**, so a German-language card still shows the English scan; this matches the user-visible policy in the edit dialog and avoids querying region-specific scans that are sparser on Yugipedia.
3. Both `.png` and `.jpg` extensions per combo (older LOB-era uploads are `.jpg`, modern reprints are `.png`).
4. A rarity-less fallback round so cards with unknown rarities still resolve in single-rarity sets.

`buildYugipediaQueryUrl(...)` joins all candidates into one MediaWiki **batch query** (`titles=File:A|File:B|...` URL-encoded), so the entire list resolves in a single HTTP call. `parseYugipediaResponse(...)` walks the candidate list in order and returns the URL of the first filename that came back with `imageinfo[0].url`; missing files come back with `"missing": ""` and are skipped.

### Asset API: YGOPRODeck `cardinfo.php` (fallback + auto-detect)

`https://db.ygoprodeck.com/api/v7/cardinfo.php?fname=...`  
Used in two situations:

1. **Last-resort preview fallback.** If Yugipedia returns no candidate match (cards without an English scan yet, transient API errors), `fetchImageUrl` falls through to `parseFallbackImageUrl(...)`, which prefers an exact-name match in YGOPRODeck’s `data[]`, otherwise the first row, and returns the first entry from `card_images[0]`. This is intentionally **not** filtered by `cardset=`: when YGOPRODeck applies that filter, it reorders `card_images` so alt-art passcodes are promoted ahead of the standard art, which would re-introduce the “wrong artwork” bug we fixed by switching to Yugipedia.

2. **Auto-detect print (`detectFirstPrint` / `detectPrintVariants`, Yu-Gi-Oh! edit dialog).** Uses `fname=` plus **`cardset=`** set to the **display set name** from the picker (must match `card_sets[].set_name` in the payload). If that request fails (for example unknown set label), it retries with **`fname=` only** and still filters prints by preferred `set_name`. `YuGiOhCardPreviewSource::parsePrintVariants(...)` walks every `(set_code, set_rarity)` pair for rows whose **card name matches exactly** (case-insensitive) so the dialog can offer ring-buffer **Next** controls: one cycles distinct `set_code` values for that name+set (and resets rarity to the first upstream rarity for the newly selected code); another cycles distinct `set_rarity` values for the **current** `set_code` without changing the collector number. Shared HTTP and parsing rules live beside `parseFirstPrint`. When the dialog passes both an exact card name and a display `set_name`, an upstream miss on that label returns an error instead of falling back to unfiltered `card_sets[]` rows — otherwise unrelated products (same card name, different `set_name` on each printing) could be blended into one bogus variant list. The Yu-Gi-Oh! edit dialog additionally drops European alternate `set_code` rows that use the `-E###` pattern (single `E` before digits, e.g. `LOB-E003`) when the card language is **English**, because YGOPRODeck keeps those alongside NA numbering (`LOB-005`) under the same English `set_name`; it also collapses `LOB-005`-style and `LOB-EN005`-style codes to one **Next** slot via digit-tail matching (`ccm/util/YuGiOhPrintingSlot.hpp`). No image data is needed for this path, so Yugipedia is not consulted.

YGOPRODeck publishes rate limits and asks clients to cache responses and avoid abusive hotlinking; treat failures after burst traffic as an upstream policy signal, not an app bug. Yugipedia’s MediaWiki API is similarly polite — one batched call per preview lookup keeps us well under any normal threshold.

## Runtime Flow In CCM3

The app uses the same flow for every game that registers a module:

- `SetService` asks the game's `ISetSource` (info API) for the latest set list.
- `CardPreviewService` asks the game's `ICardPreviewSource` (asset API) for a preview image URL.
- `CardPreviewService` performs a second HTTP GET to that URL and returns raw bytes to the UI layer.
- If preview lookup fails (or returns empty bytes), the UI loads a **per-game card-back fallback** in `BaseSelectedCardPanel`: Magic / Pokémon call `CardPreviewService::fetchImageBytesByUrl(...)` against fixed HTTPS URLs. Yu-Gi-Oh! tries two Yugipedia URLs (thumbnail then full `Back-EN.png`), then reads **`assets/ygo_card_back.png`** next to the executable if both downloads fail (bundled asset; see `app/CMakeLists.txt`).

### Caching And Connection Reuse

See [caching.md](caching.md) for a dedicated reference on preview cache tiers, internal keys, eviction, clearing, and HTTP session reuse.

Three mechanisms reduce preview latency for **all** games (Magic, Pokemon, Yu-Gi-Oh!). In addition, the shared HTTP session speeds **every** `IHttpClient::get` call (including set-list fetches), not only previews:

- **In-memory preview LRU** (`CardPreviewService`). Successful `fetchPreviewBytes` results are cached keyed by `(game, name, setId, setNo)`; successful `fetchImageBytesByUrl` results are cached keyed by URL (used for the per-game card-back fallback). Re-selecting a previously viewed row is decode-only — no HTTP at all. The cache is bounded by `CardPreviewService::kCacheCapacity` (currently 128 entries) and uses a list+map LRU under a mutex (the preview pipeline is invoked from a worker thread in `BaseSelectedCardPanel`). **Source errors are split** by `PreviewLookupError::Kind`: `NotFound` (the upstream answered cleanly that the record has no image) is *negative-cached* in this tier so subsequent selections short-circuit without HTTP, while `Transient` (HTTP/network/parse failures) is **never** cached so a brief outage cannot permanently disable a card's preview.
- **Persistent disk byte cache** (`LocalPreviewByteCache`, port `IPreviewByteCache`). Wraps the in-memory tier with an on-disk store under `<exeDir>/.cache/preview-cache/` — pinned **next to the executable**, in the same scope as `config.json`, **not** under the user-configurable `Configuration.dataStorage` path. The cache stays put when the user reconfigures or relocates their collection data, and it is not part of the user's data directory backups; it is install-scoped, not collection-scoped. Both positive previews and `NotFound` verdicts survive an app restart. Each entry is a mutually-exclusive `<hash>.bin` (positive payload) or `<hash>.neg` (negative marker) plus a `<hash>.idx` sidecar containing the original key — load-time mismatch on the sidecar treats the entry as a miss, so a hash collision degrades to a one-time HTTP refetch instead of serving the wrong card's bytes (or the wrong card's "no image" verdict). Hashing is FNV-1a 64-bit (no crypto dependency). The cache is bounded by total `.bin` payload bytes (default `kDefaultMaxBytes = 64 MiB`) and evicts oldest entries by mtime when a new write would exceed the cap; reading an entry touches its mtime so frequently-viewed cards survive eviction. Negative `.neg` markers are tiny and not counted against the cap — their count is naturally bounded by the user's actively-viewed records. Filesystem mutations route through `IFileSystem`; size and mtime queries (which the port does not expose) use `std::filesystem` directly inside the adapter. The persistent tier is **fire-and-forget on the way down** — every adapter operation swallows I/O errors so a flaky or full disk never breaks the preview path.
- **Persistent HTTP session** (`CprHttpClient`). The adapter owns one long-lived `cpr::Session` (libcurl easy handle) for the lifetime of the app. Per-request configuration is limited to `SetUrl(...)`; headers, timeout, and redirect policy are configured once in the constructor. Default **`Accept: */*`** keeps JSON responses and raw image bodies working on the same session (avoid tying every GET to `application/json`). libcurl's connection pool keeps the TLS connection to each host warm, so repeat calls to `api.scryfall.com`, `api.pokemontcg.io`, `db.ygoprodeck.com`, `yugipedia.com`, and `ms.yugipedia.com` skip the TLS handshake. A `std::mutex` serializes callers — libcurl easy handles are not thread-safe, and the preview pipeline is single-flight per panel anyway.

`CardPreviewService` consults the tiers in order **memory → disk → source/HTTP**. On a disk hit (positive *or* negative) the entry is promoted into the in-memory LRU so the next click on the same row never re-touches the disk cache. On HTTP success the bytes are written through to both tiers in one shot. On a `NotFound` source error the **negative** marker is written through to both tiers; on `Transient` source errors nothing is written, so the next selection retries cleanly.

The combined effect on the preview path: first selection of a previously-unseen card pays one TLS handshake per *new* host this session (typically two hops for Yu-Gi-Oh!: `yugipedia.com` for the API, `ms.yugipedia.com` for the image), each subsequent fresh card on the same host skips the handshake, any re-selection of an already-viewed card is instant, after the first run with the disk cache populated **even a fresh app launch is decode-only for previously-seen cards** until eviction or a manual cache clear, and **records the upstream cleanly has no image for** stay "instant card-back" across restarts instead of re-paying the lookup every launch. Editing a lookup-relevant field of a record (name, set, setNo, or for Yu-Gi-Oh! the rarity / edition packed into setNo) changes the cache key automatically, so a fresh resolution attempt happens on the next click.

To clear the persistent cache (for example to recover from a bad upstream image), delete the `<exeDir>/.cache/preview-cache/` subdirectory or the umbrella `<exeDir>/.cache/` folder. Note: the in-app "Reset" / data-storage-relocation flow does **not** touch this directory — the cache is install-scoped, not collection-scoped, so it is preserved across data-dir moves and only cleared by deleting the directory above explicitly (or by reinstalling / relocating the executable).

For the full caching design and contributor rules see the dedicated [caching.md](caching.md).

Fallback card-back sources (`BaseSelectedCardPanel`; Magic/Pokémon URLs match CCM2):

- Magic: `https://gamepedia.cursecdn.com/mtgsalvation_gamepedia/f/f8/Magic_card_back.jpg`
- Pokémon: `https://archives.bulbagarden.net/media/upload/1/17/Cardback.jpg`
- Yu-Gi-Oh!: Yugipedia English TCG back — try `https://ms.yugipedia.com/thumb/e/e5/Back-EN.png/250px-Back-EN.png`, then `https://ms.yugipedia.com/e/e5/Back-EN.png`; if both fail, load `<exeDir>/assets/ygo_card_back.png` (shipped from `ui_wx/assets/ygo_card_back.png` at link time). `fallbackImageUrlForGame(Game::YuGiOh)` returns the thumbnail URL for helpers that only consult a single string.

If a game module does not provide a preview source (`cardPreviewSource() == nullptr`), preview registration is skipped and the UI behaves as "no remote preview API available."

## Error Surface And Debugging Intent

All source types return `Result<T, std::string>` errors so failures cross boundaries without exceptions. In practice, this keeps failures debuggable by separating:

- info API failures (bad set payload, schema mismatch, endpoint/network failure), and
- asset API failures (query mismatch, no matching card, missing image fields, image download failure).

When previews fail, verify request construction first (name sanitization, number normalization, percent encoding), then verify response shape assumptions: Scryfall (`data`, `image_uris`), Pokemon (`data`, `images.large`/`images.small`), Yu-Gi-Oh! Yugipedia (`query.pages.<id>.imageinfo[0].url` per filename, missing files tagged `"missing": ""`), Yu-Gi-Oh! YGOPRODeck fallback (`data`, `name`, `card_images`). If the UI fallback path succeeds (network card-back and/or bundled PNG), the panel shows the card-back image and the inline label `(image preview unavailable)`; only if every fallback fails does the preview stay empty with status text.

For Yu-Gi-Oh! specifically, when a printing shows the wrong art compared with Yugipedia’s gallery, debug in this order: (1) verify the candidate list via `YuGiOhCardPreviewSource::buildCandidateFilenames(...)` against the actual file names on Yugipedia’s `Card_Gallery:<Card>` page; (2) confirm the dialog rarity name maps to the expected short code in `ygoRarityShortCode(...)` / `rarityCodeFor(...)` (extend the mapping when a new rarity surfaces); (3) confirm the `firstEdition` flag matches the printed edition stamp — the candidate ordering puts the printed edition first.
