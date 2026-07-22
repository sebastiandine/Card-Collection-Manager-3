#!/usr/bin/env python3
"""Fill neo1–neo4 catalog image_url from Japanese CardIndex scans only.

TCGdex JA neo sets have image:null. This ETL scrapes CardIndex Japanese set
pages (Awakening Legends, etc.) and writes HTTPS image_url values for exact
JA setId+localId catalog rows.

Policy (UnnumberedPromo parity): Japanese scans only. If CardIndex has no JP
image for a print, image_url is cleared — never store English pokemontcg.io
art as a fallback.

Usage:
  python tools/pokemon_jp/enrich_neo_image_urls.py
  python tools/pokemon_jp/enrich_neo_image_urls.py --dry-run
  python tools/pokemon_jp/enrich_neo_image_urls.py --overwrite
  python tools/pokemon_jp/enrich_neo_image_urls.py --overwrite --limit 20
"""

from __future__ import annotations

import argparse
import json
import re
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "ui_wx" / "assets" / "pokemon_jp_en_catalog.json"

NEO_SETS = frozenset({"neo1", "neo2", "neo3", "neo4"})
UA = {
    "User-Agent": (
        "CCM3-pokemon-jp-etl/1.0 "
        "(local; +https://github.com/sebastiandine/Card-Collection-Manager-3)"
    )
}

# JA neo set id -> (CardIndex set page slug, image CDN folder)
SET_META: dict[str, tuple[str, str]] = {
    "neo1": ("gold-silver-to-a-new-world", "neo-jp-gold-silver"),
    "neo2": ("crossing-the-ruins", "neo-jp-crossing-ruins"),
    "neo3": ("awakening-legends", "neo-jp-awakening-legends"),
    "neo4": ("darkness-and-to-light", "neo-jp-darkness-light"),
}

IMG_RE = re.compile(
    r"https://images\.cardindex\.co/cardindex-images/cards/"
    r"(?P<folder>[^/\"']+)/(?P<file>[^\"'\s>]+)\.(?P<ext>jpe?g|png|webp)",
    re.IGNORECASE,
)


def log(msg: str) -> None:
    try:
        print(msg, flush=True)
    except UnicodeEncodeError:
        print(msg.encode("ascii", errors="replace").decode("ascii"), flush=True)


def http_get(url: str, timeout: float = 60.0) -> str:
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode("utf-8", "replace")


def normalize_name(s: str) -> str:
    """Lowercase alnum tokens for matching catalog names to CardIndex slugs."""
    s = s.lower().replace("\u2019", "'").replace("'", "")
    s = re.sub(r"[^a-z0-9]+", " ", s)
    return " ".join(s.split())


def slug_base_name(slug: str) -> str:
    """shining-magikarp-129 / balloon-berry-promo -> shining magikarp / balloon berry."""
    s = slug.strip().lower()
    s = re.sub(r"-promo$", "", s)
    s = re.sub(r"-\d+$", "", s)
    return normalize_name(s.replace("-", " "))


def pick_set_image(html: str, image_folder: str, card_slug: str) -> str | None:
    """Prefer full-size JP scan in this set's CDN folder for this card slug."""
    folder_l = image_folder.lower()
    slug_l = card_slug.lower()
    full: list[str] = []
    small: list[str] = []
    for m in IMG_RE.finditer(html):
        if m.group("folder").lower() != folder_l:
            continue
        file_stem = m.group("file").lower()
        # Require the card's own slug (with optional -small).
        if not (file_stem == slug_l or file_stem == f"{slug_l}-small"):
            # Also accept promo variant files named "{base}-promo".
            base = re.sub(r"-\d+$", "", slug_l)
            if not (
                file_stem == f"{base}-promo"
                or file_stem == f"{base}-promo-small"
                or file_stem.startswith(f"{slug_l}")
            ):
                continue
        url = m.group(0)
        if file_stem.endswith("-small"):
            small.append(url)
        else:
            full.append(url)
    if full:
        # Prefer exact slug match over promo/other.
        for u in full:
            if f"/{slug_l}." in u.lower():
                return u
        for u in full:
            if f"/{slug_l}-" not in u.lower() or "-promo." in u.lower():
                return u
        return full[0]
    if small:
        # Upgrade -small to full-size URL when possible.
        u = small[0]
        return re.sub(r"-small\.(jpe?g|png|webp)$", r".\1", u, flags=re.I)
    return None


def scrape_set_index(
    set_id: str, *, sleep_s: float
) -> dict[str, list[tuple[str, str]]]:
    """Return normalize_name -> [(card_slug, image_url), ...] for one neo set."""
    page_slug, image_folder = SET_META[set_id]
    set_url = f"https://www.cardindex.co/pokemon-cards/{page_slug}"
    log(f"scraping set index {set_id}: {set_url}")
    html = http_get(set_url)
    card_slugs = sorted(
        set(
            re.findall(
                rf"/pokemon-cards/{re.escape(page_slug)}/([a-z0-9\-]+)",
                html,
            )
        )
    )
    log(f"  {len(card_slugs)} card pages")

    by_name: dict[str, list[tuple[str, str]]] = {}
    for i, slug in enumerate(card_slugs, start=1):
        card_url = f"https://www.cardindex.co/pokemon-cards/{page_slug}/{slug}"
        try:
            time.sleep(sleep_s)
            card_html = http_get(card_url)
        except (urllib.error.URLError, TimeoutError) as exc:
            log(f"  [{i}/{len(card_slugs)}] FAIL {slug}: {exc}")
            continue
        img = pick_set_image(card_html, image_folder, slug)
        if not img:
            log(f"  [{i}/{len(card_slugs)}] no JP image {slug}")
            continue
        name = slug_base_name(slug)
        by_name.setdefault(name, []).append((slug, img))
        log(f"  [{i}/{len(card_slugs)}] {name!r} <- {img}")
    return by_name


def resolve_url_for_print(
    name_en: str, index: dict[str, list[tuple[str, str]]]
) -> str | None:
    key = normalize_name(name_en)
    if not key:
        return None
    hits = index.get(key) or []
    if not hits:
        return None
    # Unique image only — ambiguous Unown / multi-print names stay empty.
    urls = sorted({u for _slug, u in hits})
    if len(urls) == 1:
        return urls[0]
    return None


def enrich_neo_images(
    catalog: dict,
    *,
    dry_run: bool,
    overwrite: bool,
    sleep_s: float,
    limit: int,
    out_path: Path,
) -> tuple[int, int, int, int]:
    prints = catalog.get("prints", [])
    candidates = [
        p
        for p in prints
        if p.get("set_id") in NEO_SETS and (p.get("name_en") or "").strip()
    ]
    if not overwrite:
        candidates = [
            p for p in candidates if not (p.get("image_url") or "").strip()
        ]
    if limit > 0:
        candidates = candidates[:limit]

    # Scrape only the sets we need.
    needed_sets = sorted({str(p["set_id"]) for p in candidates})
    indexes: dict[str, dict[str, list[tuple[str, str]]]] = {}
    for sid in needed_sets:
        indexes[sid] = scrape_set_index(sid, sleep_s=sleep_s)

    filled = 0
    changed = 0
    missed = 0
    updates = 0

    def persist() -> None:
        if dry_run:
            return
        out_path.write_text(
            json.dumps(catalog, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )

    for i, p in enumerate(candidates, start=1):
        sid = str(p["set_id"])
        lid = str(p["local_id"])
        name_en = str(p["name_en"]).strip()
        prev = (p.get("image_url") or "").strip()
        log(f"[{i}/{len(candidates)}] {sid}-{lid} {name_en!r} …")
        url = resolve_url_for_print(name_en, indexes.get(sid, {}))
        if url:
            if not dry_run:
                p["image_url"] = url
            filled += 1
            if url != prev:
                changed += 1
                updates += 1
                log(f"  -> {url}" + (f" (was {prev})" if prev else ""))
            else:
                log(f"  -> {url} (unchanged)")
        else:
            missed += 1
            if prev:
                if not dry_run:
                    p.pop("image_url", None)
                changed += 1
                updates += 1
                log(f"  -> (miss, cleared {prev})")
            else:
                log("  -> (miss)")
        if updates >= 25:
            persist()
            updates = 0
            log(f"  checkpoint wrote {out_path}")

    persist()
    return len(candidates), filled, changed, missed


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=OUT)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument(
        "--overwrite",
        action="store_true",
        help="Re-resolve neo prints that already have image_url (clears EN URLs)",
    )
    ap.add_argument("--limit", type=int, default=0, help="Max neo prints to process")
    ap.add_argument("--sleep", type=float, default=0.35, help="Seconds between HTTP calls")
    args = ap.parse_args()

    if not args.out.is_file():
        raise SystemExit(f"catalog not found: {args.out}")

    catalog = json.loads(args.out.read_text(encoding="utf-8"))
    total, filled, changed, missed = enrich_neo_images(
        catalog,
        dry_run=args.dry_run,
        overwrite=args.overwrite,
        sleep_s=args.sleep,
        limit=args.limit,
        out_path=args.out,
    )

    print(
        f"neo image_url: candidates={total} filled={filled} changed={changed} "
        f"missed={missed}"
        + (" (dry-run)" if args.dry_run else f" wrote {args.out}"),
        flush=True,
    )


if __name__ == "__main__":
    main()
