#!/usr/bin/env python3
"""Prepare approved eight-way idle art and eight authored walking cycles.

Requires Pillow. Source PNGs stay untouched. Alpha-connected silhouettes are
selected across nominal cell boundaries, retaining swords and red cloth. No
reflection, recoloring, generated in-between frames, or runtime image filters.
"""
import hashlib
import json
from pathlib import Path
from statistics import median

from PIL import Image
from prepare_technique_assets import pose_mask

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/directions'
DIRECTIONS = ('n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw')
IDLE_SEEDS = ((640,200),(1000,200),(1000,600),(1000,1000),
              (640,1000),(280,1000),(280,600),(280,200))
CELL, ANCHOR, HEIGHT = 192, (96,176), 148


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def select(master, seed):
    alpha = master.getchannel('A')
    # Generated transparent RGB can carry alpha 1..4 specks outside the outline.
    # Drop only this nearly invisible alpha fringe, never a color such as red.
    mask = pose_mask(alpha, seed).point(lambda a: 0 if a <= 4 else a)
    bounds = mask.getbbox()
    assert bounds and 0 < bounds[0] < bounds[2] < master.width, bounds
    assert 0 < bounds[1] < bounds[3] < master.height, bounds
    pose = master.copy()
    pose.putalpha(mask)
    pose = pose.crop(bounds)
    # Align the head/pelvis column, not the whole bounding box: the sword is
    # deliberately asymmetric. The lowest boot sole defines the ground plane.
    solid = pose.getchannel('A').point(lambda a: 255 if a > 64 else 0)
    top = solid.crop((0,0,pose.width,max(1,round(pose.height*.1))))
    xs = [x for y in range(top.height) for x in range(top.width) if top.getpixel((x,y))]
    root = (sum(xs)/len(xs), pose.height-2)
    return pose, bounds, root


def pack(poses, scale, output):
    atlas = Image.new('RGBA',(CELL*4,CELL*2))
    frames = []
    hashes = set()
    for i, (pose,bounds,root) in enumerate(poses):
        size = tuple(round(v*scale) for v in pose.size)
        # Pillow's RGBa mode resamples premultiplied alpha, avoiding dark halos.
        resized = pose.convert('RGBa').resize(size,Image.Resampling.LANCZOS).convert('RGBA')
        offset = tuple(round(ANCHOR[k]-root[k]*scale) for k in range(2))
        assert min(offset) >= 4, (output.name,i,offset)
        assert all(offset[k]+size[k] <= CELL-4 for k in range(2)), (output.name,i,size,offset)
        cell = Image.new('RGBA',(CELL,CELL))
        cell.alpha_composite(resized,offset)
        b = cell.getbbox()
        assert b and b[0]>=4 and b[1]>=4 and b[2]<=CELL-4 and b[3]<=CELL-4
        hashes.add(hashlib.sha256(cell.tobytes()).hexdigest())
        atlas.alpha_composite(cell,(i%4*CELL,i//4*CELL))
        frames.append({'source_bounds':bounds,'source_root':root,
                       'runtime_bounds':b,'mirrored':False})
    assert len(hashes)==8, 'Eight different authored poses required'
    atlas.save(output,optimize=True)
    return {'scale':scale,'runtime_sha256':digest(output),'frames':frames}


def source(path):
    master = Image.open(path)
    assert master.mode == 'RGBA', path
    alpha = master.getchannel('A')
    fraction = alpha.histogram()[0]/(master.width*master.height)
    assert alpha.getextrema()[0]==0 and fraction>.25, (path,fraction)
    return master, {'source_size':master.size,'master_sha256':digest(path),
                    'alpha_zero_fraction':fraction,'alpha_range':alpha.getextrema()}


def main():
    report = {'cell':CELL,'root':ANCHOR,'direction_order':DIRECTIONS,
              'alpha_noise_cutoff':4,'atlases':{}}
    master, entry = source(ART/'eight-direction-master.png')
    poses = [select(master,seed) for seed in IDLE_SEEDS]
    # Same physical head-to-ground size in every independently drawn view.
    prepared = []
    for pose,bounds,root in poses:
        scale = HEIGHT/pose.height
        normalized = pose.convert('RGBa').resize((round(pose.width*scale),HEIGHT),Image.Resampling.LANCZOS).convert('RGBA')
        prepared.append((normalized,bounds,(root[0]*scale,root[1]*scale)))
    idle = pack(prepared,1,ROOT/'assets/warrior-idle-8dir-v1.png')
    # Record original-space roots/scales; prepared idle coordinates are internal.
    for frame,(_,bounds,root) in zip(idle['frames'],poses):
        frame['source_bounds']=bounds; frame['source_root']=root
        frame['scale']=HEIGHT/(bounds[3]-bounds[1])
    report['atlases']['idle']={**entry,**idle}
    for name in DIRECTIONS:
        master, entry = source(ART/f'walk/{name}-master.png')
        poses = [select(master,(int((i%4+.5)*master.width/4),
                                int((i//4+.47)*master.height/2))) for i in range(8)]
        scale = HEIGHT/median(pose.height for pose,_,_ in poses)
        prepared = pack(poses,scale,ROOT/f'assets/warrior-walk-{name}-v1.png')
        report['atlases'][name]={**entry,**prepared}
        print(f'{name}: 8 independently drawn walking frames, scale={scale:.6f}, no reflection')
    (ART/'preparation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS: 72 distinct poses, RGBA transparency, 4px safe borders and shared foot root (96,176)')


if __name__ == '__main__':
    main()
