#!/usr/bin/env python3
"""Build English set names for Japanese Pokémon catalog from Serebii + TCGdex."""

from __future__ import annotations

import json
import re
import urllib.request
from html import unescape
from pathlib import Path

UA = "CardCollectionManager3-ETL/0.1"
OUT = Path("ui_wx/assets/pokemon_jp_en_catalog.json")
SEREBII = "https://www.serebii.net/card/japanese.shtml"
JA_OVERRIDES = {"SV4a": "シャイニートレジャーex"}


def get_bytes(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def contains_cjk(s: str) -> bool:
    return any(ord(c) >= 0x80 for c in s)


def parse_serebii_ja_en(html: str) -> dict[str, str]:
    """Extract Japanese -> English set name pairs from Serebii japanese.shtml."""
    ja_en: dict[str, str] = {}

    # Common patterns on the page:
    #  English Name<br>Japanese
    #  English Name (Japanese)
    #  <a ...>English</a> ... Japanese in nearby cell
    for m in re.finditer(
        r">([A-Za-z0-9][^<]{1,70}?)</(?:a|b|font|td|span)>\s*<br\s*/?>\s*"
        r"([^<]{2,50}?)<",
        html,
        flags=re.IGNORECASE,
    ):
        en = unescape(re.sub(r"\s+", " ", m.group(1))).strip()
        ja = unescape(re.sub(r"\s+", " ", m.group(2))).strip()
        if contains_cjk(ja) and not contains_cjk(en) and len(en) > 1:
            ja_en.setdefault(ja, en)

    for m in re.finditer(
        r">([A-Za-z0-9][^<]{1,70}?)\s*\(([^)]{2,50})\)<",
        html,
    ):
        en = unescape(re.sub(r"\s+", " ", m.group(1))).strip()
        ja = unescape(re.sub(r"\s+", " ", m.group(2))).strip()
        if contains_cjk(ja) and not contains_cjk(en) and len(en) > 1:
            ja_en.setdefault(ja, en)

    return ja_en


def main() -> None:
    sets = [
        s
        for s in json.loads(get_bytes("https://api.tcgdex.net/v2/ja/sets"))
        if not str(s.get("id", "")).startswith("CS")
    ]
    print("tcgdex sets", len(sets))

    html = get_bytes(SEREBII).decode("utf-8", "replace")
    Path("tools/pokemon_jp/_serebii_japanese.html").write_text(html, encoding="utf-8")
    print("serebii bytes", len(html))

    ja_en = parse_serebii_ja_en(html)
    print("ja->en pairs", len(ja_en))
    for ja, en in list(ja_en.items())[:12]:
        print(f"  {en!r} <- {ja!r}")

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
        name_ja = JA_OVERRIDES.get(sid, entry.get("name", ""))
        name_en = ja_en.get(name_ja, "")
        if not name_en:
            for ja, en in ja_en.items():
                if ja == name_ja or ja in name_ja or name_ja in ja:
                    name_en = en
                    break
        if name_en:
            matched += 1
        else:
            name_en = sid
        catalog[sid] = {
            "name_en": name_en,
            "name_ja": name_ja,
            "releaseDate": "",
        }

    print(f"matched {matched}/{len(sets)}; id fallback {len(sets) - matched}")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(
        json.dumps({"sets": catalog, "prints": prints}, ensure_ascii=False, indent=2)
        + "\n",
        encoding="utf-8",
    )
    print("wrote", OUT)


if __name__ == "__main__":
    main()
