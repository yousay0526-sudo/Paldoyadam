"""Paldo Asset Studio: a standalone editor for Paldoyadam sprite assets.

The package is split so that everything except ``paldo_studio.ui`` works
without Qt: the project model, image compositing, checks and the game
specification reader can be tested and scripted with Pillow and NumPy only.
"""

__version__ = "0.3.0"
