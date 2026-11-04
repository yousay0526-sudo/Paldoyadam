#!/usr/bin/env python3
"""Write the runtime JPEG copies of the large opaque paintings.

The lossless PNG masters stay under art/. The game loads the JPEG copies, which
keeps the browser download small; sprites with transparency remain PNG.
Quality 95 with full-resolution colour (4:4:4) is visually identical at game
zoom. Requires Pillow. tests/test_paintings.c checks the copies still match.
"""
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PAINTINGS = {
    'art/village/village-remake-pass1-master.png': 'assets/village.jpg',
    'art/maps/village.png': 'assets/maps/village.jpg',
    'art/maps/solbaramjae.png': 'assets/maps/solbaramjae.jpg',
    'art/maps/tamsikgul.png': 'assets/maps/tamsikgul.jpg',
    'art/prologue/01-homeward.png': 'assets/prologue/01-homeward.jpg',
    'art/prologue/02-hunger.png': 'assets/prologue/02-hunger.jpg',
    'art/prologue/03-reflection.png': 'assets/prologue/03-reflection.jpg',
}


def main():
    for master, runtime in PAINTINGS.items():
        image = Image.open(ROOT / master)
        if image.mode == 'RGBA' and image.getchannel('A').getextrema() != (255, 255):
            raise SystemExit(f'{master} has transparency; keep it as PNG')
        out = ROOT / runtime
        out.parent.mkdir(parents=True, exist_ok=True)
        image.convert('RGB').save(out, 'JPEG', quality=95, subsampling=0, optimize=True)
        print(f'{runtime}: {out.stat().st_size // 1024} KiB from {master}')


if __name__ == '__main__':
    main()
