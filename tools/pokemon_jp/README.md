# Japanese Pokémon EN catalog ETL

Offline pipeline that builds `ui_wx/assets/pokemon_jp_en_catalog.json` for the
CCM3 Japanese Pokémon module. The C++ app loads this file at startup; it does
**not** scrape Bulbapedia or PokéAPI at runtime.

## Output schema

```json
{
  "sets": {
    "PMCG1": {
      "name_en": "Expansion Pack",
      "name_ja": "拡張パック",
      "releaseDate": "1996/10/20"
    }
  },
  "prints": [
    {
      "set_id": "PMCG1",
      "local_id": "073",
      "name_en": "Switch",
      "name_ja": "ポケモンいれかえ",
      "name_en_source": "trainer-table",
      "tcgplayer_id": "575596"
    },
    {
      "set_id": "PMCG4",
      "local_id": "017",
      "name_en": "Dark Charizard",
      "name_ja": "わるいリザードン",
      "name_en_source": "species-table-variant",
      "tcgplayer_id": "575744"
    }
  ]
}
```

`name_en_source` is one of `bulbapedia` | `species-table` | `species-table-variant` |
`trainer-table` | `energy-table` | `manual` | `tcgdex-thirdparty`.

Optional gap-fill fields (classic JA when TCGdex has no CDN scan):

- `tcgplayer_id` — TCGPlayer product id from data-asia `thirdParty.tcgplayer`
  (PMCG and other classic sets). Runtime builds
  `https://product-images.tcgplayer.com/fit-in/437x437/{id}.jpg`
- `image_url` — explicit HTTPS URL (wins over `tcgplayer_id` when both set).
  Sources: City Gym bundled `asset:pokemon_jp_classic/...` paths, or neo1–neo4
  Japanese CardIndex scans written by `enrich_neo_image_urls.py` (JP only;
  empty `image_url` → card-back when no JP scan exists).

## Suggested steps

1. Snapshot TCGdex `GET /v2/ja/sets` into `_tcgdex_sets.json`.
2. Maintain curated English display names in `set_en_names.json` (set id → EN).
3. Run `merge_set_en_catalog.py` to emit set EN names into the catalog (also
   refreshes classic TCGdex-missing products from `classic_missing_sets.json`).
4. For Original-era products TCGdex omits (City Gym theme decks, Expansion
   Sheets, Southern Islands), maintain `classic_missing_sets.json` +
   `classic_missing_prints.json` and run `merge_classic_missing.py`. Print
   `local_id`s are sequential `001`… within each product. Owner Pokémon use
   full English titles (e.g. `Erika's Oddish`), not bare species names.
   For Bulbapedia **Unnumbered Promotional cards**, run
   `harvest_unnumbered_promos.py` to refresh the `UnnumberedPromo` set + prints,
   then `enrich_unnumbered_promo_images.py` to write Bulbagarden Archives
   `image_url` values that prefer Japanese / Unnumbered Promotional scans
   (reprint/gallery) over English Wizards primary `|image=` files. Binding is
   print-identity-aware (set/page tokens, no bare-species page fallback) so
   unrelated Mewtwo promos do not share one WHF scan. EN-only Bulbapedia pages
   leave `image_url` empty (card-back) — never store Wizards/Base Set EN
   scans for Pokemon (Japan). Also fills `name_ja` when present. Then run
   `merge_classic_missing.py`.
   Cardmarket labels some Expansion Sheet / Vending Pokémon as EXP/EXS; those
   may still be filed under `UnnumberedPromo` here (e.g. `Mewtwo (Vending S1)` /
   `Mewtwo (Vending S3)`). Auto-detect matches bare species names as whole
   tokens (`Mewtwo` → `Team GR's Mewtwo`, `Mewtwo Strikes Back (…)`, not `Mew`).
   For printing-accurate City Gym deck scans, run
   `fetch_classic_gym_images.py` and store deck-specific `image_url` values as
   `asset:pokemon_jp_classic/<setId>/<localId>.jpg`. Do **not** reuse PMCG
   donor `tcgplayer_id`s for City Gym deck exclusives; that shows the wrong
   Leaders' Stadium art.
5. Extend `non_pokemon_en_by_ja.json` when new Trainer/Energy English aliases
   are needed (JA name → EN display name; covers reprints of the same JA name).
   Baseline coverage: **first 15 chronological TCGdex JA main sets**
   (`PMCG1`–`PMCG6`, `neo1`–`neo4`, `VS1`, `web1`, `E1`–`E3`). See
   `docs/assets-and-info-apis.md` → **Extending Trainer/Energy English aliases**.
6. Run `enrich_preview_images.py` to merge from TCGdex cards-database
   `data-asia`:
   - `tcgplayer_id` for classic-image gap-fill (where data-asia exposes it)
   - `name_ja` from card sources
   - `name_en` via National Dex → English species table (`species_en.json`)
     for ordinary Pokémon with `dexId` (e.g. Blastoise → `032`, Mewtwo → `050`)
   - **Variant full titles** when `name_ja` matches a known prefix + `dexId`
     (owner gym leaders, Rocket's, Dark, Light, Shining — e.g. `Erika's Oddish`,
     `Dark Charizard`, `Shining Celebi`). Upgrades existing bare `species-table`
     rows on re-enrich. Tagged `species-table-variant`.
   - `name_en` via `non_pokemon_en_by_ja.json` for Trainer/Energy
     (e.g. Switch ← `ポケモンいれかえ` → localId `073`)
7. Run `enrich_neo_image_urls.py` after enrich when neo1–neo4 previews need
   gap-fill. Writes `image_url` from **Japanese** CardIndex set scans (never
   English pokemontcg.io). Matching is by English card name within the JA neo
   set. Misses clear `image_url` (card-back). Use `--overwrite` to replace
   stale EN URLs. See `docs/assets-and-info-apis.md`.
8. Optionally refine `prints[]` name fields via Bulbapedia joins.

At runtime, `JapanesePokemonSetSource` prefers catalog `name_en` and **never**
leaves Japanese TCGdex names in `Set.name` (falls back to the set id). It also
injects the classic missing products listed above. `JapanesePokemonCardPreviewSource`
uses catalog `tcgplayer_id` / `image_url` only for the exact `setId`+`localId`
when TCGdex has no scan, and falls back to catalog-only Auto-detect/preview
when TCGdex has no set detail for a curated classic product.

## Commands

```bash
# After refreshing tools/pokemon_jp/_tcgdex_sets.json and set_en_names.json:
python tools/pokemon_jp/merge_set_en_catalog.py

# After editing classic_missing_sets.json / classic_missing_prints.json:
python tools/pokemon_jp/merge_classic_missing.py

# Refresh UnnumberedPromo prints from Bulbapedia, enrich JP images, then merge:
python tools/pokemon_jp/harvest_unnumbered_promos.py
python tools/pokemon_jp/enrich_unnumbered_promo_images.py --force
python tools/pokemon_jp/merge_classic_missing.py

# After updating bundled City Gym deck scans:
python tools/pokemon_jp/fetch_classic_gym_images.py

# Harvest TCGPlayer ids + species/trainer/variant English aliases:
python tools/pokemon_jp/enrich_preview_images.py

# Fill neo1–neo4 image_url from Japanese CardIndex scans (run after enrich):
python tools/pokemon_jp/enrich_neo_image_urls.py
# Replace / clear previously written EN pokemontcg.io neo URLs:
python tools/pokemon_jp/enrich_neo_image_urls.py --overwrite
```

Seed-only catalog writer (minimal rows):

```bash
python tools/pokemon_jp/build_catalog.py \
  --out ui_wx/assets/pokemon_jp_en_catalog.json
```
