#!/usr/bin/env python3
"""Build a full Japanese Pokémon EN set catalog from TCGdex + Wikipedia.

Strategy:
1. Pull JA set list from TCGdex (exclude CS*).
2. Pull Wikipedia 'List of Pokémon Trading Card Game sets' wikitext and
   extract English / Japanese name pairs from {{lang|ja|...}} near bold titles.
3. Match by Japanese name (after TCGdex overrides). Unmatched sets get a
   Latin fallback of the set id (never leave Japanese in Set.name).
4. Fetch release dates from TCGdex set detail for catalog completeness.
"""

from __future__ import annotations

import argparse
import json
import re
import time
import urllib.request
from pathlib import Path

UA = "CardCollectionManager3-ETL/0.1 (local; set-catalog)"
JA_OVERRIDES = {
    "SV4a": "シャイニートレジャーex",
}


def http_json(url: str):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return json.load(resp)


def http_text_params(base: str, params: dict) -> dict:
    from urllib.parse import urlencode

    return http_json(base + "?" + urlencode(params))


def contains_cjk(s: str) -> bool:
    return any(
        "\u3040" <= ch <= "\u30ff"
        or "\u3400" <= ch <= "\u4dbf"
        or "\u4e00" <= ch <= "\u9fff"
        or "\uf900" <= ch <= "\ufaff"
        for ch in s
    )


def wiki_en_ja_pairs() -> dict[str, str]:
    """Map Japanese set name -> English display name from Wikipedia."""
    data = http_text_params(
        "https://en.wikipedia.org/w/api.php",
        {
            "action": "parse",
            "page": "List of Pokémon Trading Card Game sets",
            "prop": "wikitext",
            "format": "json",
            "formatversion": "2",
        },
    )
    wt = data["parse"]["wikitext"]
    # '''English Name''' ... lang|ja|Japanese Name
    pairs: dict[str, str] = {}
    for m in re.finditer(
        r"'''([^']+)'''(?P<body>.{0,260}?)\{\{lang\|ja\|(?P<ja>[^}]+)\}\}",
        wt,
        flags=re.DOTALL,
    ):
        en = m.group(1).strip()
        ja = m.group("ja").strip()
        # Drop template noise / multi-ja (take first segment before &)
        ja = re.split(r"\s*&\s*", ja)[0].strip()
        ja = re.sub(r"<[^>]+>", "", ja).strip()
        if not ja or not en or not contains_cjk(ja):
            continue
        # Prefer first English seen for a JA name
        pairs.setdefault(ja, en)
    return pairs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("ui_wx/assets/pokemon_jp_en_catalog.json"),
    )
    parser.add_argument(
        "--fetch-dates",
        action="store_true",
        help="Hit TCGdex set detail for each set (slow, ~1 req/s).",
    )
    args = parser.parse_args()

    raw_sets = http_json("https://api.tcgdex.net/v2/ja/sets")
    sets = [s for s in raw_sets if not str(s.get("id", "")).startswith("CS")]
    print(f"TCGdex JA sets (excl CS*): {len(sets)}")

    ja_to_en = wiki_en_ja_pairs()
    print(f"Wikipedia JA->EN pairs: {len(ja_to_en)}")

    # Keep existing print rows if present
    existing_prints = []
    if args.out.exists():
        try:
            prev = json.loads(args.out.read_text(encoding="utf-8"))
            existing_prints = prev.get("prints", [])
        except Exception:
            pass

    catalog_sets: dict[str, dict] = {}
    matched = 0
    for entry in sets:
        sid = entry["id"]
        name_ja = JA_OVERRIDES.get(sid, entry.get("name", ""))
        name_en = ja_to_en.get(name_ja, "")
        if not name_en:
            # Fuzzy: Wikipedia sometimes includes extra spaces / fullwidth
            for ja, en in ja_to_en.items():
                if ja in name_ja or name_ja in ja:
                    name_en = en
                    break
        if name_en:
            matched += 1
        else:
            # Never leave CJK in the UI set picker — fall back to set id.
            name_en = sid
        catalog_sets[sid] = {
            "name_en": name_en,
            "name_ja": name_ja,
            "releaseDate": "",
        }

    print(f"Matched Wikipedia EN names: {matched}/{len(sets)}")
    print(f"Fallback to set id: {len(sets) - matched}")

    if args.fetch_dates:
        for i, sid in enumerate(catalog_sets):
            try:
                detail = http_json(f"https://api.tcgdex.net/v2/ja/sets/{sid}")
                rd = detail.get("releaseDate") or ""
                if rd:
                    catalog_sets[sid]["releaseDate"] = rd.replace("-", "/")
            except Exception as exc:
                print(f"  date fail {sid}: {exc}")
            time.sleep(0.35)
            if (i + 1) % 20 == 0:
                print(f"  dates {i+1}/{len(catalog_sets)}")

    out = {"sets": catalog_sets, "prints": existing_prints}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(out, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Wrote {args.out}")


if __name__ == "__main__":
    main()
