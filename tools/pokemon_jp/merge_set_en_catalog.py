#!/usr/bin/env python3
"""Merge curated English set names into pokemon_jp_en_catalog.json.

Reads tools/pokemon_jp/_tcgdex_sets.json (TCGdex JA list snapshot) and
tools/pokemon_jp/set_en_names.json (curated id -> English display name).
Also preserves / refreshes classic TCGdex-missing products from
classic_missing_sets.json (UnnumberedPromo, City Gym decks, Expansion Sheets,
Southern Islands).
Never writes Japanese into name_en.
"""

from __future__ import annotations

import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
SETS_SNAP = HERE / "_tcgdex_sets.json"
EN_MAP = HERE / "set_en_names.json"
CLASSIC = HERE / "classic_missing_sets.json"
OUT = ROOT / "ui_wx" / "assets" / "pokemon_jp_en_catalog.json"
JA_OVERRIDES = {"SV4a": "シャイニートレジャーex"}


def main() -> None:
    sets = json.loads(SETS_SNAP.read_text(encoding="utf-8"))
    en_map: dict[str, str] = json.loads(EN_MAP.read_text(encoding="utf-8"))
    classic: dict[str, dict] = {}
    if CLASSIC.exists():
        classic = json.loads(CLASSIC.read_text(encoding="utf-8"))

    prints = []
    if OUT.exists():
        try:
            prints = json.loads(OUT.read_text(encoding="utf-8")).get("prints", [])
        except Exception:
            pass

    catalog: dict[str, dict] = {}
    matched = 0
    for entry in sets:
        sid = entry["id"]
        if sid.startswith("CS"):
            continue
        name_ja = JA_OVERRIDES.get(sid, entry.get("name", ""))
        name_en = en_map.get(sid, "").strip()
        if name_en:
            matched += 1
        else:
            name_en = sid
        catalog[sid] = {
            "name_en": name_en,
            "name_ja": name_ja,
            "releaseDate": "",
        }

    for sid, meta in classic.items():
        name_en = str(meta.get("name_en") or en_map.get(sid) or sid).strip()
        catalog[sid] = {
            "name_en": name_en,
            "name_ja": str(meta.get("name_ja") or ""),
            "releaseDate": str(meta.get("releaseDate") or ""),
        }
        if name_en and name_en != sid:
            matched += 1

    OUT.write_text(
        json.dumps({"sets": catalog, "prints": prints}, ensure_ascii=False, indent=2)
        + "\n",
        encoding="utf-8",
    )
    print(f"wrote {OUT}: {matched}/{len(catalog)} curated EN names")


if __name__ == "__main__":
    main()
