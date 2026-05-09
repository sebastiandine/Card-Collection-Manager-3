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

## Runtime Flow In CCM3

The app uses the same flow for both games:

- `SetService` asks the game's `ISetSource` (info API) for the latest set list.
- `CardPreviewService` asks the game's `ICardPreviewSource` (asset API) for a preview image URL.
- `CardPreviewService` performs a second HTTP GET to that URL and returns raw bytes to the UI layer.
- If preview lookup fails (or returns empty bytes), the UI fetches a per-game fallback card-back image URL through `CardPreviewService::fetchImageBytesByUrl(...)` and shows that image in the selected-card preview panel.

Current fallback image URLs (kept in `BaseSelectedCardPanel` for CCM2 parity):

- Magic: `https://gamepedia.cursecdn.com/mtgsalvation_gamepedia/f/f8/Magic_card_back.jpg`
- Pokemon: `https://archives.bulbagarden.net/media/upload/1/17/Cardback.jpg`

If a game module does not provide a preview source (`cardPreviewSource() == nullptr`), preview registration is skipped and the UI behaves as "no remote preview API available."

## Error Surface And Debugging Intent

Both source types return `Result<T, std::string>` errors so failures cross boundaries without exceptions. In practice, this keeps failures debuggable by separating:

- info API failures (bad set payload, schema mismatch, endpoint/network failure), and
- asset API failures (query mismatch, no matching card, missing image fields, image download failure).

When previews fail, verify request construction first (name sanitization, number normalization, percent encoding), then verify response shape assumptions (`data`, `image_uris`, `images.large`/`images.small`). If the fallback fetch succeeds, the panel intentionally shows the card-back image and the inline label `(image preview unavailable)`.
