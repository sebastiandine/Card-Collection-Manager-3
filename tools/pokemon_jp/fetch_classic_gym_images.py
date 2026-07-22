#!/usr/bin/env python3
"""Download and convert bundled classic Japanese gym-deck scans.

Writes JPEGs under ui_wx/assets/pokemon_jp_classic/<setId>/<localId>.jpg.
Uses dwebp + cjpeg from the local MSYS2 toolchain so we do not need Pillow.
"""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "ui_wx" / "assets" / "pokemon_jp_classic"

# Curated direct CDN/file URLs for printing-accurate deck scans.
SOURCES: dict[tuple[str, str], str] = {
    # TCGCollector static CDN URL for Erika (City Gym Decks No. 061).
    ("TamamushiCG", "016"): (
        "https://static.tcgcollector.com/content/images/9d/33/c6/"
        "9d33c6ffe701da03266dd5a65c6ee9537c7043b63b880cf3889f644e1c66aa6f.webp"
    ),
}


def tool(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise SystemExit(f"required tool not found on PATH: {name}")
    return path


def download(url: str, dest: Path) -> None:
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        dest.write_bytes(resp.read())


def convert_to_jpeg(src: Path, dest: Path) -> None:
    suffix = src.suffix.lower()
    if suffix in {".jpg", ".jpeg"}:
        shutil.copyfile(src, dest)
        return
    if suffix == ".webp":
        dwebp = tool("dwebp")
        cjpeg = tool("cjpeg")
        with tempfile.NamedTemporaryFile(suffix=".ppm", delete=False) as tmp:
            ppm = Path(tmp.name)
        try:
            subprocess.run([dwebp, str(src), "-ppm", "-o", str(ppm)], check=True)
            with open(dest, "wb") as out:
                subprocess.run([cjpeg, "-quality", "92", str(ppm)], check=True, stdout=out)
        finally:
            ppm.unlink(missing_ok=True)
        return
    raise SystemExit(f"unsupported source format: {src}")


def main() -> None:
    for (set_id, local_id), url in SOURCES.items():
        out_dir = OUT_DIR / set_id
        out_dir.mkdir(parents=True, exist_ok=True)
        target = out_dir / f"{local_id}.jpg"
        with tempfile.TemporaryDirectory() as td:
            src = Path(td) / Path(url).name
            print(f"download {set_id}/{local_id} <- {url}")
            download(url, src)
            convert_to_jpeg(src, target)
        print(f"wrote {target}")


if __name__ == "__main__":
    main()
