#!/usr/bin/env python3
"""Merge classic TCGdex-missing JA products into pokemon_jp_en_catalog.json.

Reads:
  tools/pokemon_jp/classic_missing_sets.json
  tools/pokemon_jp/classic_missing_prints.json
  tools/pokemon_jp/set_en_names.json (updated with EN display names)

Writes set metadata + prints into ui_wx/assets/pokemon_jp_en_catalog.json
without dropping existing TCGdex-backed entries. Replaces prior prints for
the same (set_id, local_id) keys from classic_missing_prints.json.
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SETS = HERE / "classic_missing_sets.json"
PRINTS = HERE / "classic_missing_prints.json"
EN_MAP = HERE / "set_en_names.json"
OUT = ROOT / "ui_wx" / "assets" / "pokemon_jp_en_catalog.json"

# Classic gym-deck / sheet products use synthetic set ids (not on TCGdex).
CLASSIC_SET_IDS = frozenset(
    {
        "UnnumberedPromo",
        "ExpSheet1",
        "ExpSheet2",
        "ExpSheet3",
        "NiviCG",
        "HanadaCG",
        "KuchibaCG",
        "TamamushiCG",
        "YamabukiCG",
        "GurenTG",
        "SouthernIslands",
    }
)

GYM_DECK_SET_IDS = frozenset(
    {"NiviCG", "HanadaCG", "KuchibaCG", "TamamushiCG", "YamabukiCG", "GurenTG"}
)


def strip_gym_deck_donor_ids(classic_prints: list[dict]) -> int:
    """Remove PMCG donor ids; gym-deck exclusives need printing-accurate art."""
    stripped = 0
    for p in classic_prints:
        sid = str(p.get("set_id") or "")
        if sid not in GYM_DECK_SET_IDS:
            continue
        if p.get("image_url"):
            p.pop("tcgplayer_id", None)
            continue
        if p.pop("tcgplayer_id", None) is not None:
            stripped += 1
    return stripped


def main() -> None:
    missing_sets: dict[str, dict] = json.loads(SETS.read_text(encoding="utf-8"))
    missing_prints: list[dict] = json.loads(PRINTS.read_text(encoding="utf-8"))
    en_map: dict[str, str] = json.loads(EN_MAP.read_text(encoding="utf-8"))

    catalog: dict = {"sets": {}, "prints": []}
    if OUT.exists():
        catalog = json.loads(OUT.read_text(encoding="utf-8"))

    sets_obj: dict = catalog.setdefault("sets", {})
    for sid, meta in missing_sets.items():
        name_en = str(meta.get("name_en") or "").strip() or sid
        en_map[sid] = name_en
        sets_obj[sid] = {
            "name_en": name_en,
            "name_ja": str(meta.get("name_ja") or ""),
            "releaseDate": str(meta.get("releaseDate") or ""),
        }

    EN_MAP.write_text(
        json.dumps(dict(sorted(en_map.items())), ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    classic_ids = {s for s in missing_sets}
    classic_keys = {
        (str(p.get("set_id")), str(p.get("local_id"))) for p in missing_prints
    }
    existing: list[dict] = catalog.get("prints") or []
    kept = [
        p
        for p in existing
        if (str(p.get("set_id")), str(p.get("local_id"))) not in classic_keys
        or str(p.get("set_id")) not in classic_ids
    ]
    # Drop all prints for classic set ids, then append the curated list.
    kept = [p for p in kept if str(p.get("set_id")) not in classic_ids]
    stripped = strip_gym_deck_donor_ids(missing_prints)
    kept.extend(missing_prints)
    catalog["prints"] = kept

    OUT.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"merged {len(missing_sets)} classic sets and {len(missing_prints)} prints "
        f"into {OUT} (total prints={len(kept)}, gym donor ids stripped={stripped})"
    )


if __name__ == "__main__":
    main()
