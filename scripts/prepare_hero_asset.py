#!/usr/bin/env python3
"""Prepare the approved hero illustration for its 46px in-game display.

Requires Pillow for this authoring step only. The game does not need Python.
The generated master is preserved; only its resolution changes for runtime.
"""
import argparse
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path,
                        default=ROOT / 'art/hero/warrior-clarity-master.png')
    parser.add_argument('--output', type=Path, default=ROOT / 'assets/warrior-v2.png')
    args = parser.parse_args()
    with Image.open(args.source) as source:
        # Keep the original square canvas and transparent margins so the
        # existing feet anchor and 46x46 world-space bounds stay unchanged.
        if source.width != source.height or source.mode != 'RGBA':
            parser.error('The hero master must be square and have an RGBA channel.')
        # RGBA resizing in Pillow filters in premultiplied-alpha space, which
        # avoids dark fringes around the transparent sword and clothing.
        prepared = source.resize((138, 138), Image.Resampling.LANCZOS)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    prepared.save(args.output, optimize=True)
    print(f'Hero runtime image: {args.output} (138x138 RGBA)')


if __name__ == '__main__':
    main()
