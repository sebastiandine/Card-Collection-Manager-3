#!/usr/bin/env python3
"""Fill UnnumberedPromo image_url / name_ja from Bulbapedia card pages.

Reads tools/pokemon_jp/classic_missing_prints.json rows with set_id=UnnumberedPromo,
resolves each `bulbapedia_page` (with redirects), and prefers Japanese /
Unnumbered Promotional scans from reprint/gallery fields over the English
primary `|image=` (often a Wizards Black Star print).

If Bulbapedia only hosts an English scan, image_url is left empty (card-back)
rather than storing a misleading EN preview.

Usage:
  python tools/pokemon_jp/enrich_unnumbered_promo_images.py
  python tools/pokemon_jp/enrich_unnumbered_promo_images.py --force
  python tools/pokemon_jp/enrich_unnumbered_promo_images.py --dry-run
  python tools/pokemon_jp/enrich_unnumbered_promo_images.py --limit 20

Then:
  python tools/pokemon_jp/merge_classic_missing.py
"""

from __future__ import annotations

import argparse
import json
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
PRINTS = HERE / "classic_missing_prints.json"
SET_ID = "UnnumberedPromo"
UA = "CCM3-pokemon-jp-etl/1.0 (local; +https://github.com/sebastiandine/Card-Collection-Manager-3)"
API = "https://bulbapedia.bulbagarden.net/w/api.php"

JNAME_RE = re.compile(r"\|\s*jname\s*=\s*([^\n|]+)", re.IGNORECASE)
# |image= / |image1= / |reprint1= / |caption= / |caption2= / |recaption1=
FIELD_RE = re.compile(
    r"\|\s*(image|reprint|caption|recaption)(\d*)\s*=\s*([^\n]+)",
    re.IGNORECASE,
)

SKIP_IMAGE_SUBSTR = (
    "attack.png",
    "card_back",
    "cardback",
    "project_tcg",
    "setsymbol",
    "rare_",
    "energy.png",
    "tcg1_",
    "tcg2_",
    "misprint",
)

# Filename / caption hints that the scan is the Japanese unnumbered print.
JP_FILENAME_MARKERS = (
    "corocoro",
    "whf",
    "fanbook",
    "unnumbered",
    "japanese",
    "gb2",
    "illustrator",
    "battleroad",
    "movie",
    "parentchild",
    "vending",
    "asobikata",
    "jogress",
    "pokedude",
    "daisuki",
    "specialsheet",
    "informationpack",
    "howibecame",
    "newgarura",
    "touchgeneration",
    "championleague",
    "worldofillusions",
    "clashatthesummit",
    "blackwhitetour",
    "warnerbros",
    "nintendo64",
    "teamgr",
    "imakuni",
    "tradeplease",
    "hungrysnorlax",
    "coolporygon",  # often still EN — scored only with caption
)

# Captions that mark the Unnumbered / JP print on shared EN+JP articles.
JP_CAPTION_MARKERS = (
    "unnumbered promotional",
    "unnumbered promo",
    "japanese",
    "jpexpansion",
)

# English primary prints we must not prefer when a JP candidate exists.
EN_FILENAME_MARKERS = (
    "wizardspromo",
    "baseset",
    "neogenesis",
    "fossil",
    "teamrocket",
    "jungle",
    "legendarycollection",
    "dppromo",
    "mysterious treasures",
    "mysterioustreasures",
    "diamondpearl",
    "exdragon",
    "exholon",
    "exdelta",
    "neodiscovery",
    "neorevelations",
    "neodestiny",
    "gymheroes",
    "gymchallenge",
    "nintendopromo",  # often EN Black Star; allow if also JP-captioned
)


def api(**params: object) -> dict:
    qs = urllib.parse.urlencode({k: v for k, v in params.items() if v is not None})
    req = urllib.request.Request(f"{API}?{qs}", headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=90) as resp:
        return json.load(resp)


def fetch_wikitext(page: str) -> tuple[str, str] | None:
    """Return (resolved_title, wikitext) or None if missing."""
    try:
        data = api(
            action="parse",
            page=page,
            prop="wikitext",
            format="json",
            redirects=1,
        )
    except urllib.error.HTTPError:
        return None
    except urllib.error.URLError:
        return None
    if "error" in data:
        return None
    parsed = data.get("parse") or {}
    wt = (parsed.get("wikitext") or {}).get("*")
    title = parsed.get("title") or page
    if not wt:
        return None
    return title, wt


def file_url(filename: str) -> str | None:
    fname = filename.strip().replace(" ", "_")
    if not fname:
        return None
    data = api(
        action="query",
        titles=f"File:{fname}",
        prop="imageinfo",
        iiprop="url",
        format="json",
    )
    pages = (data.get("query") or {}).get("pages") or {}
    for page in pages.values():
        infos = page.get("imageinfo") or []
        if infos and infos[0].get("url"):
            return str(infos[0]["url"])
    return None


def normalize_filename(raw: str) -> str | None:
    s = raw.strip().split("|", 1)[0].strip()
    s = re.sub(r"\[\[(?:File:)?([^\]|]+).*", r"\1", s, flags=re.IGNORECASE)
    s = s.strip()
    if not s:
        return None
    low = s.lower().replace(" ", "_")
    if any(tok in low for tok in SKIP_IMAGE_SUBSTR):
        return None
    if not re.search(r"\.(jpe?g|png|gif|webp)$", low):
        return None
    return s


def score_candidate(filename: str, caption: str) -> int:
    """Higher is better. Score <= 0 means EN-only / reject for UnnumberedPromo."""
    fl = filename.lower().replace(" ", "").replace("_", "")
    cl = caption.lower()
    score = 0

    if any(m in cl for m in JP_CAPTION_MARKERS):
        score += 100
    if any(m.replace(" ", "") in fl for m in JP_FILENAME_MARKERS):
        score += 50
    if "promo" in fl and not any(m in fl for m in ("wizardspromo", "nintendopromo", "dppromo")):
        score += 10

    en_hit = any(m.replace(" ", "") in fl for m in EN_FILENAME_MARKERS)
    if en_hit:
        # EN primary unless caption explicitly marks Unnumbered/JP.
        if score < 100:
            return -100
        score -= 20

    return score


def collect_image_candidates(wikitext: str) -> list[tuple[int, str]]:
    """Return (score, filename) for JP-eligible images, best first."""
    # Map field key -> value for pairing imageN with captionN / reprintN with recaptionN.
    fields: dict[str, str] = {}
    for m in FIELD_RE.finditer(wikitext):
        kind = m.group(1).lower()
        num = m.group(2) or ""
        val = m.group(3).strip()
        fields[f"{kind}{num}"] = val

    candidates: list[tuple[int, str]] = []
    seen: set[str] = set()

    def add(fname_raw: str, caption: str) -> None:
        fname = normalize_filename(fname_raw)
        if not fname:
            return
        key = fname.lower().replace(" ", "_")
        if key in seen:
            return
        score = score_candidate(fname, caption)
        if score <= 0:
            return
        seen.add(key)
        candidates.append((score, fname))

    # Primary image + caption (usually EN — only kept if JP-scored).
    if "image" in fields:
        add(fields["image"], fields.get("caption", ""))

    # reprintN + recaptionN (common home of Unnumbered JP scans).
    for key, val in list(fields.items()):
        m = re.fullmatch(r"reprint(\d+)", key)
        if not m:
            continue
        n = m.group(1)
        add(val, fields.get(f"recaption{n}", "") or fields.get(f"caption{n}", ""))

    # Gallery imageN + captionN.
    for key, val in list(fields.items()):
        m = re.fullmatch(r"image(\d+)", key)
        if not m:
            continue
        n = m.group(1)
        add(val, fields.get(f"caption{n}", "") or fields.get(f"recaption{n}", ""))

    candidates.sort(key=lambda t: (-t[0], t[1].lower()))
    return candidates


def normalize_token_blob(s: str) -> str:
    """Lowercase alnum-only blob for substring affinity checks."""
    return re.sub(r"[^a-z0-9]+", "", s.lower())


def identity_tokens(print_row: dict) -> list[str]:
    """Significant tokens from this print's promo identity (set / page)."""
    raw_bits: list[str] = []
    for key in ("tcg_set", "bulbapedia_page", "name_en"):
        val = str(print_row.get(key) or "").strip()
        if val:
            raw_bits.append(val)
    # Prefer longer set-like phrases first.
    tokens: list[str] = []
    for bit in raw_bits:
        # Drop trailing extras like "(Jumbo)".
        bit = re.sub(r"\s*\([^)]*(?:Jumbo|Mini|Silver|Gold)[^)]*\)\s*", " ", bit)
        # Pull parenthetical set qualifier: "Mewtwo (WHF Special Sheet promo)".
        m = re.search(r"\(([^)]+)\)", bit)
        if m:
            inner = m.group(1)
            inner = re.sub(r"\bpromo\b", "", inner, flags=re.I).strip()
            if inner:
                tokens.append(inner)
        tokens.append(bit)
    # Significant wordy tokens (>=3 chars after normalize), longest first.
    out: list[str] = []
    seen: set[str] = set()
    for t in tokens:
        norm = normalize_token_blob(t)
        if len(norm) < 4:
            continue
        if norm in seen:
            continue
        # Skip generic card-name-only blobs when we have set context.
        seen.add(norm)
        out.append(norm)
    out.sort(key=len, reverse=True)
    return out


def has_print_affinity(
    print_row: dict,
    requested_page: str,
    resolved_title: str,
    filename: str,
    caption: str = "",
) -> bool:
    """True if this JP candidate belongs to this print, not a borrowed promo."""
    tokens = identity_tokens(print_row)
    token_set = set(tokens)
    hay = normalize_token_blob(filename + " " + caption + " " + resolved_title)
    fl = normalize_token_blob(filename)
    req = normalize_token_blob(requested_page)
    resolved = normalize_token_blob(resolved_title)

    # Filename names a specific JP promo family this print is not part of → reject.
    foreign_markers = (
        "whf",
        "corocoro",
        "fanbook",
        "gb2",
        "specialsheet",
        "songbest",
        "battleroad",
        "teamgr",
        "illustrator",
        "asobikata",
        "vending",
        "movie",
    )
    for marker in foreign_markers:
        if marker in fl and not any(marker in tok for tok in token_set):
            # e.g. WHF file on a Wizards Promo / Song Best Collection row.
            return False

    # Resolved title still matches what we asked for (allow mild redirect rename).
    if req and (req in resolved or resolved in req):
        # Still require filename not foreign (handled above); OK.
        if any(m in fl for m in foreign_markers) or "unnumbered" in hay or any(
            len(tok) >= 5 and tok in fl for tok in token_set
        ):
            return True
        # Requested page matched but image is generic EN — leave to score_candidate.
        if any(len(tok) >= 5 and tok in hay for tok in token_set):
            return True

    tcg_set = str(print_row.get("tcg_set") or "").strip()
    tcg_set_norm = normalize_token_blob(tcg_set)
    # Wizards Promo rows may use the Wizards article, but only with a
    # non-foreign JP file (foreign_markers already rejected WHF/etc.).
    if tcg_set_norm.startswith("wizardspromo") and "wizardspromo" in resolved:
        if "wizardspromo" in fl or (
            any(m in caption.lower() for m in JP_CAPTION_MARKERS)
            and not any(m in fl for m in foreign_markers)
        ):
            return True
        return False

    # Reject borrowing from a generic Wizards Promo dump unless this print is that set.
    if "wizardspromo" in resolved and not tcg_set_norm.startswith("wizardspromo"):
        for tok in tokens:
            if len(tok) >= 5 and tok in hay and "wizardspromo" not in tok:
                species = normalize_token_blob(str(print_row.get("tcg_name") or ""))
                if species and tok == species:
                    continue
                return True
        return False

    for tok in tokens:
        if len(tok) >= 5 and tok in hay:
            species = normalize_token_blob(str(print_row.get("tcg_name") or ""))
            if species and tok == species:
                continue
            return True
        if len(tok) >= 3 and tok in ("whf", "gb2") and tok in hay:
            return True

    for tok in tokens:
        if len(tok) >= 5 and tok in fl:
            species = normalize_token_blob(str(print_row.get("tcg_name") or ""))
            if species and tok == species:
                continue
            return True

    return False


def pick_image_filename_for_print(
    print_row: dict,
    requested_page: str,
    resolved_title: str,
    wikitext: str,
) -> str | None:
    """Best JP scan that also has affinity with this print's promo identity."""
    fields: dict[str, str] = {}
    for m in FIELD_RE.finditer(wikitext):
        fields[f"{m.group(1).lower()}{m.group(2) or ''}"] = m.group(3).strip()

    def caption_for(fname: str) -> str:
        target = fname.lower().replace(" ", "_")
        for key, val in fields.items():
            nf = normalize_filename(val)
            if not nf or nf.lower().replace(" ", "_") != target:
                continue
            if key == "image":
                return fields.get("caption", "")
            m = re.fullmatch(r"(reprint|image)(\d+)", key)
            if not m:
                continue
            n = m.group(2)
            if m.group(1) == "reprint":
                return fields.get(f"recaption{n}", "") or fields.get(f"caption{n}", "")
            return fields.get(f"caption{n}", "") or fields.get(f"recaption{n}", "")
        return ""

    for _score, fname in collect_image_candidates(wikitext):
        if has_print_affinity(
            print_row, requested_page, resolved_title, fname, caption_for(fname)
        ):
            return fname
    return None


def pick_jname(wikitext: str) -> str:
    m = JNAME_RE.search(wikitext)
    if not m:
        return ""
    return m.group(1).strip()


def candidate_pages(print_row: dict) -> list[str]:
    """Qualified Bulbapedia titles only — never bare species (avoids shared dumps)."""
    out: list[str] = []
    page = str(print_row.get("bulbapedia_page") or "").strip()
    if page:
        out.append(page)
    tcg_set = str(print_row.get("tcg_set") or "").strip()
    tcg_name = str(print_row.get("tcg_name") or "").strip()
    tcg_num = str(print_row.get("tcg_num") or "").strip()
    if tcg_name and tcg_set:
        if not tcg_num or tcg_num.lower() == "promo":
            out.append(f"{tcg_name} ({tcg_set} promo)")
        else:
            out.append(f"{tcg_name} ({tcg_set} {tcg_num})")
            out.append(f"{tcg_name} ({tcg_set} promo)")
    # Full qualified English title from harvest (may include Jumbo markers).
    name_en = str(print_row.get("name_en") or "").strip()
    if name_en and "(" in name_en:
        # Strip only trailing variant markers, keep set qualifier.
        cleaned = re.sub(
            r"\s*\((?:Jumbo|Mini|Silver|Gold|Silver w/Stamp)[^)]*\)\s*$",
            "",
            name_en,
            flags=re.I,
        ).strip()
        if cleaned:
            out.append(cleaned)
        out.append(name_en)
    seen: set[str] = set()
    uniq: list[str] = []
    for p in out:
        if p and p not in seen:
            seen.add(p)
            uniq.append(p)
    return uniq


def enrich_print(print_row: dict, sleep_s: float) -> bool:
    """Mutate print_row with JP image_url / name_ja. Return True if image filled."""
    already = str(print_row.get("image_url") or "").strip()
    if already:
        return False

    for page in candidate_pages(print_row):
        time.sleep(sleep_s)
        resolved = fetch_wikitext(page)
        if not resolved:
            continue
        title, wt = resolved
        if not str(print_row.get("name_ja") or "").strip():
            jname = pick_jname(wt)
            if jname:
                print_row["name_ja"] = jname
        fname = pick_image_filename_for_print(print_row, page, title, wt)
        if not fname:
            continue
        time.sleep(sleep_s)
        url = file_url(fname)
        if url:
            print_row["image_url"] = url
            return True
    return False


def log(msg: str) -> None:
    try:
        print(msg)
    except UnicodeEncodeError:
        print(msg.encode("ascii", errors="replace").decode("ascii"))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--limit", type=int, default=0, help="Max UnnumberedPromo rows")
    ap.add_argument("--sleep", type=float, default=0.35, help="Seconds between API calls")
    ap.add_argument("--force", action="store_true", help="Overwrite existing image_url")
    ap.add_argument(
        "--save-every",
        type=int,
        default=25,
        help="Persist classic_missing_prints.json every N updates",
    )
    args = ap.parse_args()

    all_prints: list[dict] = json.loads(PRINTS.read_text(encoding="utf-8"))
    targets = [p for p in all_prints if str(p.get("set_id")) == SET_ID]
    if args.limit > 0:
        targets = targets[: args.limit]

    def persist() -> None:
        if args.dry_run:
            return
        PRINTS.write_text(
            json.dumps(all_prints, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )

    filled = 0
    missed = 0
    updates_since_save = 0
    for i, p in enumerate(targets, start=1):
        lid = p.get("local_id")
        name = p.get("name_en")
        if args.force:
            p.pop("image_url", None)
        if str(p.get("image_url") or "").strip():
            log(f"[{i}/{len(targets)}] skip {lid} {name} (already has image)")
            continue
        ok = enrich_print(p, sleep_s=args.sleep)
        if ok:
            filled += 1
            updates_since_save += 1
            log(f"[{i}/{len(targets)}] OK {lid} {name} -> {p.get('image_url')}")
        else:
            missed += 1
            # Ensure stale EN URLs do not linger after --force.
            p.pop("image_url", None)
            log(f"[{i}/{len(targets)}] MISS {lid} {name}")
        if updates_since_save >= args.save_every:
            persist()
            updates_since_save = 0
            log(f"  checkpoint wrote {PRINTS}")

    log(f"filled={filled} missed={missed} total={len(targets)}")
    if args.dry_run:
        log(f"dry-run: not writing {PRINTS}")
        return
    persist()
    log(f"wrote {PRINTS}")


if __name__ == "__main__":
    main()
