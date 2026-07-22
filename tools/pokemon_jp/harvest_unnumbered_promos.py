#!/usr/bin/env python3
"""Harvest Bulbapedia Unnumbered Promotional cards into classic_missing seed JSON.

Fetches:
  - Unnumbered_Promotional_cards_(TCG)/1996-2005
  - Yearly sections on Unnumbered_Promotional_cards_(TCG) (2006+)

Writes/updates:
  - tools/pokemon_jp/classic_missing_sets.json  (adds UnnumberedPromo)
  - tools/pokemon_jp/classic_missing_prints.json (replaces UnnumberedPromo prints)

Synthetic local_ids are sequential 001… (cards are unnumbered in print).
Each print carries a qualified English title when the setlist row has a
{{TCG ID|Set|Name|num}} (e.g. "Mewtwo (CoroCoro promo)") plus a
`bulbapedia_page` hint for enrich_unnumbered_promo_images.py.

Run after harvest:
  python tools/pokemon_jp/enrich_unnumbered_promo_images.py
  python tools/pokemon_jp/merge_classic_missing.py
"""

from __future__ import annotations

import json
import re
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
SETS = HERE / "classic_missing_sets.json"
PRINTS = HERE / "classic_missing_prints.json"

SET_ID = "UnnumberedPromo"
SET_META = {
    "name_en": "Unnumbered Promotional cards",
    "name_ja": "番号なしプロモーションカード",
    "releaseDate": "1996/10/15",
}

UA = "CCM3-pokemon-jp-etl/1.0 (local; +https://github.com/sebastiandine/Card-Collection-Manager-3)"

# {{TCG ID|Set|Name|num}} — name may contain δ / &amp; etc.
TCG_ID_FULL_RE = re.compile(
    r"\{\{TCG ID\|([^}|]+)\|([^}|]+)(?:\|([^}|]*))?\}\}", re.IGNORECASE
)
TCG_RE = re.compile(r"\{\{TCG\|([^}|]+)(?:\|[^}]*)?\}\}", re.IGNORECASE)
OBP_RE = re.compile(r"\{\{OBP\|([^}|]+)(?:\|[^}]*)?\}\}", re.IGNORECASE)
SMALL_TAG_RE = re.compile(
    r"<small>\s*'''?\s*\[([^\]]+)\]\s*'''?\s*</small>", re.IGNORECASE
)
ITALIC_NOTE_RE = re.compile(r"\(''([^']+)''\)")
HTML_TAG_RE = re.compile(r"<[^>]+>")
TEMPLATE_RE = re.compile(r"\{\{[^{}]*\}\}")


def fetch_wikitext(page: str) -> str:
    qs = urllib.parse.urlencode(
        {
            "action": "parse",
            "page": page,
            "prop": "wikitext",
            "format": "json",
        }
    )
    url = f"https://bulbapedia.bulbagarden.net/w/api.php?{qs}"
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=90) as resp:
        data = json.load(resp)
    return data["parse"]["wikitext"]["*"]


def decode_wiki_text(s: str) -> str:
    return (
        s.replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&#39;", "'")
        .strip()
    )


def bulbapedia_page_from_tcg_id(set_name: str, card_name: str, num: str) -> str:
    """Best-effort Bulbapedia article title for a TCG ID triple."""
    set_name = decode_wiki_text(set_name)
    card_name = decode_wiki_text(card_name)
    num = decode_wiki_text(num or "").strip()
    if not num or num.lower() == "promo":
        return f"{card_name} ({set_name} promo)"
    return f"{card_name} ({set_name} {num})"


def qualified_name_en(card_name: str, set_name: str, num: str, extras: list[str]) -> str:
    """Distinct English title: Name (Set promo) plus optional [Jumbo]/ markers."""
    card_name = decode_wiki_text(card_name)
    set_name = decode_wiki_text(set_name)
    num = decode_wiki_text(num or "").strip()
    if set_name:
        if not num or num.lower() == "promo":
            base = f"{card_name} ({set_name} promo)"
        else:
            base = f"{card_name} ({set_name} {num})"
    else:
        base = card_name
    # Avoid duplicating qualifier already present in extras.
    remaining = [
        e
        for e in extras
        if e.lower() not in base.lower() and e.lower() not in {"promo"}
    ]
    if remaining:
        return f"{base} ({'; '.join(remaining)})"
    return base


def collect_extras(raw: str) -> list[str]:
    extras: list[str] = []
    for m in SMALL_TAG_RE.finditer(raw):
        extras.append(m.group(1).strip())
    for m in ITALIC_NOTE_RE.finditer(raw):
        extras.append(m.group(1).strip())
    if "Jumbo" in raw and not any("jumbo" in e.lower() for e in extras):
        extras.append("Jumbo")
    if "Mini" in raw and not any("mini" in e.lower() for e in extras):
        extras.append("Mini")
    return extras


def extract_entry(field: str) -> dict | None:
    """Parse one Setlist/nmentry name field into print metadata."""
    raw = field.strip()
    extras = collect_extras(raw)

    tcg = TCG_ID_FULL_RE.search(raw)
    if tcg:
        set_name = tcg.group(1).strip()
        card_name = tcg.group(2).strip()
        num = (tcg.group(3) or "").strip()
        name_en = qualified_name_en(card_name, set_name, num, extras)
        page = bulbapedia_page_from_tcg_id(set_name, card_name, num)
        return {
            "name_en": name_en,
            "bulbapedia_page": page,
            "tcg_set": decode_wiki_text(set_name),
            "tcg_name": decode_wiki_text(card_name),
            "tcg_num": decode_wiki_text(num) if num else "promo",
        }

    name = None
    for rx in (TCG_RE, OBP_RE):
        m = rx.search(raw)
        if m:
            name = decode_wiki_text(m.group(1))
            break
    if not name:
        cleaned = TEMPLATE_RE.sub("", raw)
        cleaned = HTML_TAG_RE.sub("", cleaned)
        cleaned = cleaned.split("|", 1)[0].strip()
        cleaned = re.sub(r"\[\[([^|\]]+)(?:\|[^\]]+)?\]\]", r"\1", cleaned)
        name = decode_wiki_text(cleaned.strip(" '\""))
    if not name:
        return None

    if extras:
        name = f"{name} ({'; '.join(extras)})"
    return {"name_en": name, "bulbapedia_page": name}


def harvest_entries(wikitext: str) -> list[dict]:
    entries: list[dict] = []
    for m in re.finditer(r"\{\{Setlist/nmentry\|None\|", wikitext):
        start = m.end()
        depth = 0
        i = start
        while i < len(wikitext):
            if wikitext.startswith("{{", i):
                depth += 1
                i += 2
                continue
            if wikitext.startswith("}}", i):
                depth = max(0, depth - 1)
                i += 2
                continue
            if wikitext[i] == "|" and depth == 0:
                break
            i += 1
        parsed = extract_entry(wikitext[start:i])
        if parsed:
            entries.append(parsed)
    return entries


def main() -> None:
    pages = [
        "Unnumbered_Promotional_cards_(TCG)/1996-2005",
        "Unnumbered_Promotional_cards_(TCG)",
    ]
    all_entries: list[dict] = []
    for page in pages:
        wt = fetch_wikitext(page)
        got = harvest_entries(wt)
        print(f"{page}: {len(got)} setlist rows")
        all_entries.extend(got)

    prints: list[dict] = []
    for i, entry in enumerate(all_entries, start=1):
        row = {
            "set_id": SET_ID,
            "local_id": f"{i:03d}",
            "name_en": entry["name_en"],
            "name_ja": "",
            "name_en_source": "manual",
            "bulbapedia_page": entry.get("bulbapedia_page") or entry["name_en"],
        }
        for key in ("tcg_set", "tcg_name", "tcg_num"):
            if entry.get(key):
                row[key] = entry[key]
        prints.append(row)

    sets_obj: dict = json.loads(SETS.read_text(encoding="utf-8"))
    sets_obj[SET_ID] = SET_META
    SETS.write_text(
        json.dumps(sets_obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    existing: list[dict] = json.loads(PRINTS.read_text(encoding="utf-8"))
    kept = [p for p in existing if str(p.get("set_id")) != SET_ID]
    kept.extend(prints)
    PRINTS.write_text(
        json.dumps(kept, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"wrote {SET_ID}: {len(prints)} prints "
        f"(classic_missing_prints total={len(kept)})"
    )


if __name__ == "__main__":
    main()
