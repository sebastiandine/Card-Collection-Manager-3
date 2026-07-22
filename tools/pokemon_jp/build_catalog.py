#!/usr/bin/env python3
"""Scaffold ETL for Japanese Pokémon EN catalog JSON.

Seed mode (default) writes a small valid catalog matching the committed asset
schema. Extend this script to pull TCGdex + Bulbapedia joins for full coverage.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

SEED = {
    "sets": {
        "PMCG1": {
            "name_en": "Expansion Pack",
            "name_ja": "拡張パック",
            "releaseDate": "1996/10/20",
        },
        "PMCG2": {
            "name_en": "Pokémon Jungle",
            "name_ja": "ポケモンジャングル",
            "releaseDate": "1997/03/14",
        },
        "SV1a": {
            "name_en": "Triplet Beat",
            "name_ja": "トリプレットビート",
            "releaseDate": "2023/03/10",
        },
        "SV4a": {
            "name_en": "Shiny Treasure ex",
            "name_ja": "シャイニートレジャーex",
            "releaseDate": "2023/11/10",
        },
    },
    "prints": [
        {
            "set_id": "PMCG1",
            "local_id": "014",
            "name_en": "Charmander",
            "name_ja": "ヒトカゲ",
            "name_en_source": "bulbapedia",
        },
        {
            "set_id": "PMCG1",
            "local_id": "021",
            "name_en": "Charizard",
            "name_ja": "リザードン",
            "name_en_source": "bulbapedia",
        },
        {
            "set_id": "SV1a",
            "local_id": "001",
            "name_en": "Tropius",
            "name_ja": "トロピウス",
            "name_en_source": "bulbapedia",
        },
    ],
}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("ui_wx/assets/pokemon_jp_en_catalog.json"),
        help="Output catalog path",
    )
    args = parser.parse_args()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(SEED, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Wrote seed catalog to {args.out}")


if __name__ == "__main__":
    main()
