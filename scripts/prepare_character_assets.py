#!/usr/bin/env python3
"""Downscale the character masters in art/characters/ to runtime sprites.

The masters (1254 px squares and a 2172x724 NPC strip) are drawn at roughly
46-96 logical pixels, so the game shipped 20x more pixels than it shows and
sampled them with nearest filtering. Each runtime copy is about 3x its
largest on-screen size at 1.2x world zoom, resized with Lanczos in
premultiplied alpha so edges do not pick up dark fringes.
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
SIZES = {
    "butcher.png": (320, 320),
    "minion.png": (192, 192),
    "warrior.png": (160, 160),
    "npcs.png": (543, 181),
}

for name, size in SIZES.items():
    master = Image.open(ROOT / "art" / "characters" / name).convert("RGBA")
    small = master.convert("RGBa").resize(size, Image.LANCZOS).convert("RGBA")
    small.save(ROOT / "assets" / name, optimize=True)
    print(f"{name}: {master.size} -> {small.size}")
