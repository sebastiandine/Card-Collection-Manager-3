#!/usr/bin/env python3
"""Enrich pokemon_jp_en_catalog.json prints from TCGdex data-asia.

Harvests per-card:
  - tcgplayer_id (thirdParty.tcgplayer) for classic-image gap-fill
  - name_ja from the card source
  - name_en via National Dex id → English species name (when dexId present)
  - name_en for owner / Rocket's / Dark / Light / Shining variants (full titles)
  - name_en for Trainer/Energy via tools/pokemon_jp/non_pokemon_en_by_ja.json

English names are required for Auto-detect when the user types "Mewtwo" /
"Switch" / "Erika's Oddish" / "Dark Charizard" etc. — TCGdex set résumés only
expose Japanese names.

Usage:
  python tools/pokemon_jp/enrich_preview_images.py
  python tools/pokemon_jp/enrich_preview_images.py --data-asia path/to/data-asia
"""

from __future__ import annotations

import argparse
import io
import json
import re
import shutil
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "ui_wx" / "assets" / "pokemon_jp_en_catalog.json"
CACHE_DIR = Path(__file__).resolve().parent / "_tcgdex_cards_database"
SPECIES_CACHE = Path(__file__).resolve().parent / "species_en.json"
NON_POKEMON_EN = Path(__file__).resolve().parent / "non_pokemon_en_by_ja.json"
ZIP_URL = "https://github.com/tcgdex/cards-database/archive/refs/heads/master.zip"
# National-dex-ordered English names (index 0 = Bulbasaur / dex 1).
SPECIES_URL = (
    "https://raw.githubusercontent.com/sindresorhus/pokemon/main/data/en.json"
)

TCGPLAYER_RE = re.compile(r"tcgplayer\s*:\s*(\d+)")
NAME_JA_RE = re.compile(r"name\s*:\s*\{\s*ja\s*:\s*\"([^\"]+)\"", re.DOTALL)
DEX_RE = re.compile(r"dexId\s*:\s*\[\s*(\d+)")
CATEGORY_RE = re.compile(r'category\s*:\s*"([^"]+)"')
LOCAL_ID_RE = re.compile(r"^[0-9A-Za-z]+$")

# Chronological first 15 main Japanese expansions in TCGdex (for coverage checks).
# Longest JA prefixes first. Maps to English product-title prefix + National Dex species.
VARIANT_JA_PREFIXES: list[tuple[str, str]] = [
    ("R団の", "Rocket's "),
    ("エリカの", "Erika's "),
    ("タケシの", "Brock's "),
    ("カスミの", "Misty's "),
    ("マチスの", "Lt. Surge's "),
    ("ナツメの", "Sabrina's "),
    ("カツラの", "Blaine's "),
    ("キョウの", "Koga's "),
    ("サカキの", "Giovanni's "),
    ("ヤナギの", "Pryce's "),
    ("カンナの", "Lorelei's "),
    ("シバの", "Bruno's "),
    ("キクコの", "Agatha's "),
    ("やさしい", "Light "),
    ("ひかる", "Shining "),
    ("輝く", "Shining "),  # neo Destiny upstream garble
    ("軽い", "Light "),  # neo Destiny upstream garble
    ("わるい", "Dark "),
    ("暗い", "Dark "),  # neo Destiny upstream garble
    ("ダーク", "Dark "),  # neo Destiny upstream garble (e.g. ダークアリアドス)
]

PROTECTED_NAME_EN_SOURCES = frozenset(
    {"manual", "trainer-table", "energy-table", "bulbapedia", "tcgdex-thirdparty"}
)

FIRST15_SETS = [
    "PMCG1",
    "PMCG2",
    "PMCG3",
    "PMCG4",
    "PMCG5",
    "PMCG6",
    "neo1",
    "neo2",
    "neo3",
    "neo4",
    "VS1",
    "web1",
    "E1",
    "E2",
    "E3",
]


def download_data_asia(dest: Path) -> Path:
    dest.mkdir(parents=True, exist_ok=True)
    marker = dest / "data-asia"
    if marker.is_dir() and any(marker.rglob("*.ts")):
        return marker

    print(f"Downloading {ZIP_URL} …")
    req = urllib.request.Request(ZIP_URL, headers={"User-Agent": "ccm-pokemonjp-etl"})
    with urllib.request.urlopen(req, timeout=180) as resp:
        blob = resp.read()

    with zipfile.ZipFile(io.BytesIO(blob)) as zf:
        members = [n for n in zf.namelist() if "/data-asia/" in n.replace("\\", "/")]
        for name in members:
            parts = Path(name).parts
            if "data-asia" not in parts:
                continue
            idx = parts.index("data-asia")
            rel = Path(*parts[idx:])
            target = dest / rel
            if name.endswith("/"):
                target.mkdir(parents=True, exist_ok=True)
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            with zf.open(name) as src, open(target, "wb") as out:
                shutil.copyfileobj(src, out)

    if not marker.is_dir():
        raise SystemExit("data-asia missing after zip extract")
    return marker


def load_species_en() -> dict[int, str]:
    """Map National Dex id -> English species name."""
    if SPECIES_CACHE.is_file():
        raw = json.loads(SPECIES_CACHE.read_text(encoding="utf-8"))
    else:
        print(f"Downloading {SPECIES_URL} …")
        req = urllib.request.Request(
            SPECIES_URL, headers={"User-Agent": "ccm-pokemonjp-etl"}
        )
        with urllib.request.urlopen(req, timeout=60) as resp:
            raw = json.loads(resp.read().decode("utf-8"))
        SPECIES_CACHE.write_text(
            json.dumps(raw, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
    # File is a list: ["Bulbasaur", "Ivysaur", ...]
    if isinstance(raw, list):
        return {i + 1: name for i, name in enumerate(raw) if isinstance(name, str)}
    if isinstance(raw, dict):
        return {int(k): str(v) for k, v in raw.items()}
    raise SystemExit("unexpected species_en.json shape")


def load_non_pokemon_en() -> dict[str, str]:
    """Map Japanese Trainer/Energy (etc.) names → English display names."""
    if not NON_POKEMON_EN.is_file():
        return {}
    raw = json.loads(NON_POKEMON_EN.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise SystemExit("non_pokemon_en_by_ja.json must be a JSON object")
    return {str(k): str(v) for k, v in raw.items() if str(k).strip() and str(v).strip()}


def extract_cards(data_asia: Path) -> dict[tuple[str, str], dict]:
    """Map (setId, localId) -> {tcgplayer_id, name_ja, dex_id}."""
    out: dict[tuple[str, str], dict] = {}
    for path in data_asia.rglob("*.ts"):
        try:
            rel = path.relative_to(data_asia)
        except ValueError:
            continue
        parts = rel.parts
        if len(parts) != 3:
            continue
        set_id = parts[1]
        local_id = path.stem
        if not LOCAL_ID_RE.match(local_id):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        entry: dict = {}
        m = TCGPLAYER_RE.search(text)
        if m:
            entry["tcgplayer_id"] = m.group(1)
        m = NAME_JA_RE.search(text)
        if m:
            entry["name_ja"] = m.group(1)
        m = DEX_RE.search(text)
        if m:
            entry["dex_id"] = int(m.group(1))
        m = CATEGORY_RE.search(text)
        category = m.group(1) if m else ""
        # TCGdex PMCG1-102 Fighting Energy has an empty Japanese name in source.
        if (
            set_id == "PMCG1"
            and local_id == "102"
            and category == "Energy"
            and not entry.get("name_ja")
        ):
            entry["name_ja"] = "基本闘エネルギー"
        if not entry:
            continue
        out[(set_id, local_id)] = entry
    return out


def variant_en_prefix(name_ja: str) -> str | None:
    """Return English title prefix for a known JA variant pattern, or None."""
    for ja_prefix, en_prefix in VARIANT_JA_PREFIXES:
        if name_ja.startswith(ja_prefix):
            return en_prefix
    return None


def compose_species_name_en(
    name_ja: str, dex_id: int | None, species_en: dict[int, str]
) -> tuple[str, str] | None:
    """Return (name_en, name_en_source) from dex + optional variant prefix."""
    if dex_id is None or dex_id not in species_en:
        return None
    species = species_en[dex_id]
    prefix = variant_en_prefix(name_ja)
    if prefix:
        return prefix + species, "species-table-variant"
    return species, "species-table"


def upgrade_variant_titles(
    prints: list[dict],
    cards: dict[tuple[str, str], dict],
    species_en: dict[int, str],
) -> int:
    """Upgrade bare species-table rows to full variant English titles."""
    upgraded = 0
    for p in prints:
        if (p.get("name_en_source") or "") in PROTECTED_NAME_EN_SOURCES:
            continue
        ja = (p.get("name_ja") or "").strip()
        if not ja or variant_en_prefix(ja) is None:
            continue
        sid = str(p.get("set_id", ""))
        lid = str(p.get("local_id", ""))
        dex = cards.get((sid, lid), {}).get("dex_id")
        composed = compose_species_name_en(ja, dex, species_en)
        if composed is None:
            continue
        full_en, src = composed
        if p.get("name_en") == full_en and p.get("name_en_source") == src:
            continue
        p["name_en"] = full_en
        p["name_en_source"] = src
        upgraded += 1
    return upgraded


def verify_first15_trainer_coverage(
    data_asia: Path, non_pokemon_en: dict[str, str]
) -> list[str]:
    """Return unique Trainer/Energy JA names in FIRST15 still missing from the map."""
    missing: set[str] = set()
    for path in data_asia.rglob("*.ts"):
        try:
            rel = path.relative_to(data_asia)
        except ValueError:
            continue
        parts = rel.parts
        if len(parts) != 3 or parts[1] not in FIRST15_SETS:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        catm = CATEGORY_RE.search(text)
        if not catm or catm.group(1) == "Pokemon":
            continue
        jam = NAME_JA_RE.search(text)
        ja = jam.group(1) if jam else ""
        if path.stem == "102" and parts[1] == "PMCG1" and not ja:
            ja = "基本闘エネルギー"
        if not ja:
            missing.add(f"{parts[1]}/{path.stem} <empty name_ja>")
            continue
        if ja not in non_pokemon_en:
            missing.add(ja)
    return sorted(missing)


def merge_catalog(
    catalog: dict,
    cards: dict[tuple[str, str], dict],
    species_en: dict[int, str],
    non_pokemon_en: dict[str, str],
) -> tuple[int, int, int, int]:
    prints = catalog.setdefault("prints", [])
    by_key: dict[tuple[str, str], dict] = {}
    for p in prints:
        sid = str(p.get("set_id", ""))
        lid = str(p.get("local_id", ""))
        if sid and lid:
            by_key[(sid, lid)] = p

    updated = 0
    added = 0
    species_named = 0
    table_named = 0
    for (sid, lid), meta in sorted(cards.items()):
        existing = by_key.get((sid, lid))
        if existing is None:
            existing = {
                "set_id": sid,
                "local_id": lid,
                "name_en": "",
                "name_ja": "",
                "name_en_source": "",
            }
            prints.append(existing)
            by_key[(sid, lid)] = existing
            added += 1

        changed = False
        pid = meta.get("tcgplayer_id")
        if pid and existing.get("tcgplayer_id") != pid:
            existing["tcgplayer_id"] = pid
            changed = True

        name_ja = meta.get("name_ja", "")
        if name_ja and not (existing.get("name_ja") or "").strip():
            existing["name_ja"] = name_ja
            changed = True

        if not (existing.get("name_en") or "").strip():
            dex = meta.get("dex_id")
            ja_key = (existing.get("name_ja") or name_ja or "").strip()
            composed = compose_species_name_en(ja_key, dex, species_en)
            if composed is not None:
                existing["name_en"], existing["name_en_source"] = composed
                species_named += 1
                changed = True
            else:
                ja_key = (existing.get("name_ja") or name_ja or "").strip()
                if ja_key and ja_key in non_pokemon_en:
                    existing["name_en"] = non_pokemon_en[ja_key]
                    # Energies vs trainers: basic energy names share a pattern.
                    if "エネルギー" in ja_key and ja_key.startswith("基本"):
                        existing["name_en_source"] = "energy-table"
                    elif "エネルギー" in ja_key:
                        existing["name_en_source"] = "energy-table"
                    else:
                        existing["name_en_source"] = "trainer-table"
                    table_named += 1
                    changed = True

        if changed:
            updated += 1

    # Also apply the JA→EN table to existing prints that were never in data-asia
    # walk (or already present with name_ja but empty name_en).
    for p in prints:
        if (p.get("name_en") or "").strip():
            continue
        ja_key = (p.get("name_ja") or "").strip()
        if not ja_key or ja_key not in non_pokemon_en:
            continue
        p["name_en"] = non_pokemon_en[ja_key]
        if "エネルギー" in ja_key:
            p["name_en_source"] = "energy-table"
        else:
            p["name_en_source"] = "trainer-table"
        table_named += 1
        updated += 1

    catalog["prints"] = prints
    return updated, added, species_named, table_named


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--data-asia", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=OUT)
    args = ap.parse_args()

    if args.data_asia:
        data_asia = args.data_asia
        if not data_asia.is_dir():
            raise SystemExit(f"data-asia not found: {data_asia}")
    else:
        data_asia = download_data_asia(CACHE_DIR)

    species_en = load_species_en()
    non_pokemon_en = load_non_pokemon_en()
    cards = extract_cards(data_asia)
    print(f"found {len(cards)} card files under {data_asia}")
    print(f"non-pokemon EN map entries: {len(non_pokemon_en)}")

    if args.out.exists():
        catalog = json.loads(args.out.read_text(encoding="utf-8"))
    else:
        catalog = {"sets": {}, "prints": []}

    updated, added, species_named, table_named = merge_catalog(
        catalog, cards, species_en, non_pokemon_en
    )
    variant_upgraded = upgrade_variant_titles(
        catalog["prints"], cards, species_en
    )

    for lid, expect_en in (
        ("021", "Charizard"),
        ("032", "Blastoise"),
        ("050", "Mewtwo"),
        ("073", "Switch"),
    ):
        hit = next(
            (
                p
                for p in catalog["prints"]
                if p.get("set_id") == "PMCG1" and p.get("local_id") == lid
            ),
            None,
        )
        if hit:
            print(
                f"PMCG1/{lid}: name_en={hit.get('name_en')!r} "
                f"name_ja={hit.get('name_ja')!r} tp={hit.get('tcgplayer_id')}"
            )
            if hit.get("name_en") != expect_en:
                print(f"  WARNING: expected name_en {expect_en!r}")
        else:
            print(f"WARNING: missing PMCG1/{lid}")

    gaps = verify_first15_trainer_coverage(data_asia, non_pokemon_en)
    if gaps:
        print(f"WARNING: {len(gaps)} FIRST15 trainer/energy JA names still unmapped:")
        for ja in gaps[:30]:
            print(f"  - {ja}")
        if len(gaps) > 30:
            print(f"  ... and {len(gaps) - 30} more")
    else:
        print(f"FIRST15 trainer/energy coverage OK ({len(FIRST15_SETS)} sets)")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    # Spot-check variant titles on classic sets.
    for sid, lid, expect_en in (
        ("PMCG5", "002", "Erika's Oddish"),
        ("PMCG4", "017", "Dark Charizard"),
        ("PMCG6", "042", "Rocket's Zapdos"),
    ):
        hit = next(
            (
                p
                for p in catalog["prints"]
                if p.get("set_id") == sid and p.get("local_id") == lid
            ),
            None,
        )
        if hit:
            print(
                f"{sid}/{lid}: name_en={hit.get('name_en')!r} "
                f"source={hit.get('name_en_source')!r}"
            )
            if hit.get("name_en") != expect_en:
                print(f"  WARNING: expected name_en {expect_en!r}")
        else:
            print(f"WARNING: missing {sid}/{lid}")

    print(
        f"wrote {args.out}: touched={updated} added={added} "
        f"species_named={species_named} table_named={table_named} "
        f"variant_upgraded={variant_upgraded} "
        f"prints={len(catalog['prints'])}"
    )


if __name__ == "__main__":
    main()
