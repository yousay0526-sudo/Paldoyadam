#!/usr/bin/env python3
"""Authoring QA for the registered village repaint (requires Pillow and NumPy).

Reports local edge alignment, not a proof of identical silhouettes or art quality.
Run from any directory; the baseline must be available in the local Git checkout.
"""
import argparse
import hashlib
import io
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
BASE = '3accab5b8a4024421a01082ef31cd2f558d55a38'
LANDMARKS = [
    ('smith roof', 210, 125), ('chimney', 343, 102), ('smith gate', 295, 278),
    ('herbalist roof', 600, 156), ('herbalist gate', 515, 253),
    ('shrine', 400, 348), ('lantern', 455, 330), ('well roof', 566, 426),
    ('well wall', 570, 482), ('tavern roof', 182, 455), ('tavern gate', 416, 532),
    ('central pine', 846, 303), ('cart', 816, 502), ('slaughter rack', 1410, 308),
    ('south canopy', 525, 624), ('bare tree', 775, 848),
]


def git_bytes(ref, path):
    return subprocess.check_output(['git', 'show', f'{ref}:{path}'], cwd=ROOT)


def edges(image):
    grey = image.convert('L').filter(ImageFilter.GaussianBlur(1))
    return np.hypot(*np.gradient(np.asarray(grey, dtype=float)))


def normalized(patch):
    return (patch - patch.mean()) / (patch.std() + 1e-6)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', default=BASE)
    parser.add_argument('--output', type=Path,
                        default=ROOT / 'docs/village-remake-pass1-layout.json')
    args = parser.parse_args()
    old_bytes = git_bytes(args.baseline, 'assets/village.png')
    new_bytes = (ROOT / 'assets/village.png').read_bytes()
    master = ROOT / 'art/village/village-remake-pass1-master.png'
    before, after = Image.open(io.BytesIO(old_bytes)), Image.open(io.BytesIO(new_bytes))
    assert before.size == after.size == (1536, 1024), 'Canvas registration changed'
    assert after.convert('RGBA').getchannel('A').getextrema() == (255, 255)
    assert master.read_bytes() == new_bytes, 'Runtime differs from generated master'
    assert old_bytes != new_bytes, 'The artwork was not replaced'
    protected = {}
    for path in ['src/world.c', 'src/world.h', 'src/game.c', 'src/game.h',
                 'src/main.c', 'src/draw.h', 'web/shell.html',
                 'assets/warrior-v2.png', 'assets/warrior-walk-v1.png',
                 'assets/warrior-attack-v1.png']:
        data = (ROOT / path).read_bytes()
        baseline = git_bytes(args.baseline, path)
        # Git may check text out as CRLF on Windows; compare canonical content.
        if not path.endswith('.png'):
            data = data.replace(b'\r\n', b'\n')
            baseline = baseline.replace(b'\r\n', b'\n')
        assert baseline == data, f'Protected file changed: {path}'
        protected[path] = hashlib.sha256(data).hexdigest()
    a, b = edges(before), edges(after)
    landmarks = []
    radius, search = 28, 10
    for name, x, y in LANDMARKS:
        template = normalized(a[y-radius:y+radius, x-radius:x+radius])
        scores = []
        for dy in range(-search, search+1):
            for dx in range(-search, search+1):
                patch = normalized(b[y-radius+dy:y+radius+dy, x-radius+dx:x+radius+dx])
                scores.append((float(np.mean(template * patch)), dx, dy))
        score, dx, dy = max(scores)
        assert abs(dx) <= 2 and abs(dy) <= 2 and score >= .55, f'Inspect {name}: {dx},{dy}, NCC {score}'
        landmarks.append(dict(name=name, x=x, y=y, dx=dx, dy=dy, edge_ncc=round(score, 4)))
    report = dict(baseline=args.baseline, dimensions=list(after.size), mode=after.mode,
                  sha256_before=hashlib.sha256(old_bytes).hexdigest(),
                  sha256_after=hashlib.sha256(new_bytes).hexdigest(),
                  runtime_equals_generated_master=True, protected_files=protected,
                  method='56x56 patches; Gaussian sigma 1; gradient magnitude NCC; +/-10px search',
                  acceptance='Each axis <=2px; NCC >=0.55; supplemental to visual review',
                  limitation='Local texture alignment does not prove every silhouette or passable gap.',
                  landmarks=landmarks)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'PASS: native master, opaque canvas, {len(protected)} protected files, {len(landmarks)} landmarks')


if __name__ == '__main__':
    main()
