"""E-only material study. Reuse authored geometry; never fit pixels to a guide.

Run from the repo: python scripts/prepare_leg_idle_e_base.py
Optional --palette PATH changes material colors on this E pose only.
"""
from pathlib import Path
import argparse
import hashlib
import json
import numpy as np
from PIL import Image, ImageFilter, ImageDraw, ImageFont
import draw_leg_idle_e_grid as geometry
from prepare_leg_idle_e_grid_review import band_observations

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/gear/legs/cotton-straw'
OUT = ART / 'idle-e-base-1'
REF = ART / 'reference'
PALETTE = {
    'outline': '#655640',
    'cotton': '#cabc9f', 'cotton_dark': '#746856',
    'cotton_mid': '#a39173', 'cotton_light': '#e0d6bc',
    'indigo': '#303955', 'indigo_dark': '#1e2238', 'indigo_mid': '#545c84',
    'sock': '#e5e4dd', 'sock_shadow': '#969aa2', 'sock_light': '#fcfcf8',
    'straw': '#b48b4d', 'rope_dark': '#704e26',
    'rope_light': '#e4c990', 'rope_mid': '#d6b06c', 'indigo_fold': '#434d70'
}


def save_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n',
                    encoding='utf8', newline='\n')


def rgb(value):
    return tuple(bytes.fromhex(value.removeprefix('#')))


def material(key):
    if key.startswith('cotton'): return 'cotton'
    if key.startswith('indigo'): return 'indigo'
    if key.startswith('sock'): return 'sock'
    if key.startswith('rope') or key == 'straw': return 'straw'
    return 'outline'


def paint(source, side, palette):
    # Classify the actual authored paint, never the immutable guide silhouette.
    a = np.array(source)
    groups = {name: np.zeros(a.shape[:2], bool)
              for name in ('cotton', 'indigo', 'sock', 'straw', 'outline')}
    old = dict(geometry.COL, indigo_fold='#384762')
    for key, color in old.items():
        match = np.all(a[:, :, :3] == rgb(color), axis=2) & (a[:, :, 3] > 0)
        groups[material(key)] |= match
        a[match, :3] = rgb(palette[key])
    painted = Image.fromarray(a)
    # Short blended brush transitions inside each material. No noise or displacement.
    soft = painted.convert('RGB').filter(ImageFilter.GaussianBlur(3.2))
    for name in ('cotton', 'sock'):
        mask = Image.fromarray((groups[name] * 255).astype('uint8'))
        painted.paste(soft, (0, 0), mask)
    brush = Image.new('RGBA', source.size)
    if side == 'near':
        creases = [
            [('M', 94.0, 158.0), ('C', 96.6, 158.8, 98.5, 160.0, 99.4, 160.9)],
            [('M', 103.0, 159.8), ('C', 101.1, 161.3, 99.7, 162.3, 98.7, 164.0)],
            [('M', 94.0, 164.9), ('C', 95.3, 165.2, 96.0, 166.4, 96.4, 167.3)],
        ]
    else:
        creases = [
            [('M', 87.5, 155.9), ('C', 88.9, 156.6, 90.0, 157.1, 90.6, 158.1)],
            [('M', 94.2, 158.5), ('C', 92.9, 159.7, 91.4, 160.1, 90.7, 161.7)],
        ]
    for commands in creases:
        geometry.path(brush, commands, stroke=palette['cotton_dark'], width=.27)
    brush = brush.filter(ImageFilter.GaussianBlur(1.1))
    b = np.array(brush)
    b[:, :, 3] = (b[:, :, 3] * groups['cotton']).astype('uint8')
    painted = Image.alpha_composite(painted, Image.fromarray(b))
    # Appearance-only edit: retain the exact authored alpha, not a guide-derived mask.
    painted.putalpha(source.getchannel('A'))
    return painted, groups


def flatten(im):
    bg = Image.new('RGBA', im.size, (238, 237, 229, 255))
    return Image.alpha_composite(bg, im.convert('RGBA')).convert('RGB')


def build(palette):
    OUT.mkdir(exist_ok=True)
    (OUT / 'masks').mkdir(exist_ok=True)
    originals = {}; painted = {}; group_layers = {}
    for side in ('far', 'near'):
        source = getattr(geometry, side + '_pants')()
        source.alpha_composite(getattr(geometry, side + '_foot')())
        originals[side] = source
        painted[side], group_layers[side] = paint(source, side, palette)
        small = painted[side].resize((768, 768), Image.Resampling.LANCZOS)
        small.putalpha(source.resize((768, 768), Image.Resampling.LANCZOS).getchannel('A'))
        small.save(OUT / (side + '-unoccluded.png'))
    source = Image.alpha_composite(originals['far'], originals['near'])
    legs = Image.alpha_composite(painted['far'], painted['near']).resize((768, 768), Image.Resampling.LANCZOS)
    source_small = source.resize((768, 768), Image.Resampling.LANCZOS)
    legs.putalpha(source_small.getchannel('A'))
    legs.save(OUT / 'painted-legs-e.png')
    source_small.getchannel('A').save(OUT / 'geometry-e-alpha.png')
    near_active = np.array(originals['near'])[:, :, 3] > 0
    for name in group_layers['near']:
        mask = group_layers['near'][name] | (group_layers['far'][name] & ~near_active)
        Image.fromarray((mask * 255).astype('uint8')).resize((768, 768), Image.Resampling.LANCZOS).save(OUT / 'masks' / (name + '.png'))
    save_json(OUT / 'palette.json', palette)
    body = np.array(Image.open(REF / 'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072, 1536), Image.Resampling.NEAREST).crop((1536, 0, 2304, 768)))
    occ = np.array(Image.open(REF / 'idle-8dir-occluder.png').convert('L').resize((3072, 1536), Image.Resampling.NEAREST).crop((1536, 0, 2304, 768))) > 0
    body[~occ] = 0
    visible = np.array(legs); visible[occ] = 0
    master = Image.alpha_composite(Image.fromarray(visible), Image.fromarray(body))
    master.save(OUT / 'master-e.png')
    Image.fromarray(visible).save(OUT / 'visible-legs-e.png')
    # Observations are extracted from output PNGs before opening target coordinates.
    bands = band_observations(Image.open(OUT / 'visible-legs-e.png'))
    isolated = {}
    for side in ('far', 'near'):
        im = Image.open(OUT / (side + '-unoccluded.png'))
        ys, xs = np.where(np.array(im)[:, :, 3] >= 128)
        isolated[side] = {'bands': band_observations(im), 'lowest_outsole_y': float(ys.max() / 4)}
    save_json(OUT / 'observations.json', {'method': 'Final raster RGB components and alpha bounds; no guide input.', 'visible_bands': bands, 'isolated': isolated})
    joints = json.loads((REF / 'leg-guides.json').read_text('utf8'))['frames']['idle/2(e)']
    comparisons = []
    for side, q in zip(('left', 'right'), bands):
        target = joints[side]['ankle']
        comparisons.append({'observed_native': q['center_native'], 'guide_native': target,
                            'distance_px': round(float(np.linalg.norm(np.array(q['center_native']) - target)), 3)})
    guide = np.array(Image.open(REF / 'idle-8dir-guide4x.png').convert('RGB').crop((1536, 0, 2304, 768))).astype(np.int16)
    gm = (((guide[:, :, 2] > guide[:, :, 0] + 40) & (guide[:, :, 2] > guide[:, :, 1] + 20) & (guide[:, :, 1] > 65)) | ((guide[:, :, 0] > guide[:, :, 1] + 40) & (guide[:, :, 1] > guide[:, :, 2] + 40) & (guide[:, :, 0] > 170))) & ~occ
    roi = np.zeros(occ.shape, bool); roi[560:752, 256:512] = True; gm &= roi
    active = visible[:, :, 3] >= 128
    protected = occ & (body[:, :, 3] == 255)
    report = {
        'direction': 'E', 'status': 'material_base_candidate_unapproved', 'runtime_ready': False,
        'geometry_source': 'idle-e-grid-1/paint-v4; appearance only, exact alpha retained',
        'geometry_alpha_identical': bool(np.array_equal(np.array(legs)[:, :, 3], np.array(source_small)[:, :, 3])),
        'visible_silhouette_iou': round(float((active & gm).sum() / (active | gm).sum()), 4),
        'visible_band_comparisons': comparisons,
        'protected_opaque_pixels_changed': int(np.any(np.array(master)[protected] != body[protected], axis=1).sum()),
        'literal_contact_y175_177': {side: {'observed_outsole_y': q['lowest_outsole_y'], 'pass': 175 <= q['lowest_outsole_y'] <= 177} for side, q in isolated.items()},
        'contact_contract': 'Brief requires both contact=true soles at y176 +/-1, but immutable E heel/toe centerlines differ by 5.76px and brief also places soles approximately 6px below them. Far literal screen-ground test fails. No projection reinterpretation or foot movement applied.',
        'anatomical_heel_toe_validation': 'not certified; prior sock highlight centroids are paint proxies only; inspect isolated feet and alignment overlay',
        'overall_idle_approved': False,
        'pending': ['User visual approval of materials and shoe construction', 'Resolve contact contract', 'Anatomical heel/toe endpoints remain ambiguous in painted contours', 'Other seven idle directions not produced from this base'],
    }
    save_json(OUT / 'validation.json', report)
    font = ImageFont.truetype('C:/Windows/Fonts/arial.ttf', 22)
    old = Image.open(ART / 'idle-e-grid-1/master-e.png')
    compare = Image.new('RGB', (1408, 1040), (238, 237, 229)); draw = ImageDraw.Draw(compare)
    for i, (label, im) in enumerate((('BEFORE / grid geometry', old), ('AFTER / material candidate', master))):
        draw.text((i * 704 + 20, 16), label, font=font, fill='#332c24')
        compare.paste(flatten(im).resize((576, 576), Image.Resampling.NEAREST), (i * 704 + 64, 52))
        compare.paste(flatten(im).crop((320, 552, 464, 720)).resize((336, 392), Image.Resampling.NEAREST), (i * 704 + 184, 636))
    compare.save(OUT / 'comparison.png')
    alignment = flatten(master); d = ImageDraw.Draw(alignment)
    d.line((256, 704, 512, 704), fill='#299b41', width=2)
    for side, color in (('left', '#2878e6'), ('right', '#e67819')):
        for name in ('ankle', 'heel', 'toe'):
            x, y = np.array(joints[side][name]) * 4
            d.line((x-5, y, x+5, y), fill=color, width=2); d.line((x, y-5, x, y+5), fill=color, width=2)
    alignment.crop((320, 552, 464, 720)).resize((576, 672), Image.Resampling.NEAREST).save(OUT / 'alignment.png')
    inspect = Image.new('RGB', (1024, 500), (238, 237, 229)); d = ImageDraw.Draw(inspect)
    for i, side in enumerate(('far', 'near')):
        d.text((i * 512 + 16, 16), side + ' / unoccluded', font=font, fill='#332c24')
        inspect.paste(flatten(Image.open(OUT / (side + '-unoccluded.png'))).crop((320, 608, 464, 720)).resize((504, 392), Image.Resampling.NEAREST), (i * 512 + 4, 64))
    inspect.save(OUT / 'isolated-feet.png')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--palette', type=Path)
    parser.add_argument('--output', type=Path, help='New candidate folder for a palette variation')
    args = parser.parse_args()
    if args.palette and not args.output:
        parser.error('--palette requires --output to preserve the existing candidate')
    if args.output:
        if args.output.exists():
            parser.error('--output must be a new folder')
        OUT = args.output
        OUT.mkdir(parents=True)
    build(json.loads(args.palette.read_text('utf8')) if args.palette else PALETTE)
