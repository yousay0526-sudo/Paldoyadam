#!/usr/bin/env python3
"""Pack authored technique poses without clipping their overlapping sheet cells.

Requires Pillow. Masters are preserved byte-for-byte. Flood selection separates
the six alpha silhouettes before repacking; it never removes red by color.
Runtime: 576x384 RGBA, 3x2 192px cells; common ground root (116, 176).
"""
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageOps

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/techniques'
SPECS = {
    'cleave': [(282, 521), (780, 523), (1324, 524), (340, 977), (802, 978), (1322, 979)],
    'sweep': [(280, 487), (780, 486), (1324, 488), (335, 977), (835, 977), (1340, 975)],
    'thrust': [(298, 475), (762, 473), (1305, 474), (356, 953), (836, 953), (1307, 953)],
    'retreat': [(302, 476), (795, 479), (1270, 480), (325, 952), (825, 956), (1315, 957)],
}
CELL, ANCHOR, SCALE = 192, (116, 176), .27


def pose_mask(alpha, seed):
    binary = alpha.point(lambda a: 255 if a > 16 else 0)
    if binary.getpixel(seed) != 255:
        raise ValueError(f'Pose selection seed is outside the character: {seed}')
    ImageDraw.floodfill(binary, seed, 128)
    selected = binary.point(lambda a: 255 if a == 128 else 0)
    # Recover the source's antialiased outline, keeping its original alpha.
    return ImageChops.multiply(alpha, selected.filter(ImageFilter.MaxFilter(9)))


def main():
    report = {'cell': CELL, 'root': ANCHOR, 'scale': SCALE, 'techniques': {}}
    for name, roots in SPECS.items():
        path = ART / f'{name}-master.png'
        master = Image.open(path)
        assert master.mode == 'RGBA' and master.size == (1536, 1024), path
        alpha = master.getchannel('A')
        assert alpha.getextrema()[0] == 0
        atlas = Image.new('RGBA', (CELL * 3, CELL * 2))
        frames = []
        for i, root in enumerate(roots):
            seed = (256 + i % 3 * 512, 280 + i // 3 * 512)
            mask = pose_mask(alpha, seed)
            bounds = mask.getbbox()
            assert bounds and bounds[2] - bounds[0] < 650 and bounds[3] - bounds[1] < 520, bounds
            # Selection includes sword/sash beyond the original nominal cell.
            pose = master.copy()
            pose.putalpha(mask)
            pose = pose.crop(bounds)
            local_root = (root[0] - bounds[0], root[1] - bounds[1])
            mirrored = name == 'retreat' and i == 1
            if mirrored:
                # The generated wind-up looks right. Restore the engine's
                # canonical left-facing convention for this pose only.
                pose = ImageOps.mirror(pose)
                local_root = (pose.width - local_root[0], local_root[1])
            size = tuple(round(v * SCALE) for v in pose.size)
            pose = pose.resize(size, Image.Resampling.LANCZOS)
            offset = tuple(round(ANCHOR[k] - local_root[k] * SCALE) for k in range(2))
            assert min(offset) >= 4, (name, i, offset)
            assert offset[0] + size[0] <= CELL - 4 and offset[1] + size[1] <= CELL - 4, (name, i, offset, size)
            cell = Image.new('RGBA', (CELL, CELL))
            cell.alpha_composite(pose, offset)
            atlas.alpha_composite(cell, (i % 3 * CELL, i // 3 * CELL))
            frames.append({'source_bounds': bounds, 'source_root': root,
                           'mirrored': mirrored, 'runtime_bounds': cell.getbbox()})
        output = ROOT / f'assets/warrior-{name}-v1.png'
        atlas.save(output, optimize=True)
        report['techniques'][name] = {
            'master_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'runtime_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
            'alpha_zero_fraction': round(alpha.histogram()[0] / (1536 * 1024), 4),
            'frames': frames,
        }
        print(f'{name}: {output.name}, six poses, transparent, unclipped, shared root {ANCHOR}')
    (ART / 'preparation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
