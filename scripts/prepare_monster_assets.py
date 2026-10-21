#!/usr/bin/env python3
"""Prepare the eight-direction monster art candidates without changing masters.

Requires Pillow. Output remains in art/monsters/prepared until art approval and
separate renderer integration. No source reflection, recoloring or tweening.
"""
from pathlib import Path
from statistics import median
import hashlib
import json

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/monsters'
OUT = ART / 'prepared'
ORDER = ('n','ne','e','se','s','sw','w','nw')
CELL, ROOT_X, ROOT_Y = 192, 96, 176
ACTIONS = {'boar':('walk','attack'),'butcher':('walk','attack','skill')}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    im = Image.open(path)
    assert im.mode == 'RGBA', path
    alpha = im.getchannel('A')
    zero = alpha.histogram()[0] / (im.width*im.height)
    assert alpha.getextrema()[0] == 0 and zero > .2, (path,zero)
    return im, {'path':str(path.relative_to(ROOT)).replace('\\','/'),
                'sha256':sha(path),'size':im.size,'alpha_range':alpha.getextrema(),
                'alpha_zero_fraction':zero}


def pose(im, seed):
    alpha = im.getchannel('A')
    binary = alpha.point(lambda a:255 if a>16 else 0)
    assert binary.getpixel(seed)==255, ('Selection seed outside pose',seed)
    ImageDraw.floodfill(binary,seed,128)
    selected = binary.point(lambda a:255 if a==128 else 0)
    visible = selected.getbbox()
    assert visible and 0<visible[0]<visible[2]<im.width, ('Clipped visible silhouette horizontally',visible)
    assert 0<visible[1]<visible[3]<im.height, ('Clipped visible silhouette vertically',visible)
    # A one-pixel antialias halo; do not absorb the neighboring pose's tail.
    mask = ImageChops.multiply(alpha,selected.filter(ImageFilter.MaxFilter(3)))
    mask = mask.point(lambda a:0 if a<=4 else a)
    bounds = mask.getbbox()
    assert bounds
    result = im.copy();result.putalpha(mask);result=result.crop(bounds)
    # A provisional torso-column/lowest-ground anchor, recorded for art review.
    solid = result.getchannel('A')
    xs = [x for y in range(int(result.height*.4),int(result.height*.6))
          for x in range(result.width) if solid.getpixel((x,y))>128]
    assert xs
    root = (median(xs),result.height-1)
    return result, {'source_bounds':bounds,'source_visible_bounds':visible,'source_root_in_crop':root}


def place(atlas, item, scale, col, row, planned_scale):
    image,info=item
    size=tuple(round(v*scale) for v in image.size)
    resized=image.convert('RGBa').resize(size,Image.Resampling.LANCZOS).convert('RGBA')
    root=info['source_root_in_crop']
    at=(round(ROOT_X-root[0]*scale),round(ROOT_Y-root[1]*scale))
    assert min(at)>=4 and all(at[k]+size[k]<=CELL-4 for k in range(2)), (col,row,at,size)
    cell=Image.new('RGBA',(CELL,CELL));cell.alpha_composite(resized,at)
    b=cell.getbbox()
    assert b and b[0]>=4 and b[1]>=4 and b[2]<=CELL-4 and b[3]<=CELL-4
    atlas.alpha_composite(cell,(col*CELL,row*CELL))
    return {**info,'scale':scale,'planned_scale':planned_scale,
            'draw_scale_compensation':planned_scale/scale,
            'runtime_bounds':b,'mirrored':False,
            'cell_sha256':hashlib.sha256(cell.tobytes()).hexdigest()}


def safe_scale(poses,target):
    desired=target/median(im.height for im,_ in poses)
    limits=[desired]
    for im,info in poses:
        x,y=info['source_root_in_crop']
        limits.extend([(ROOT_X-5)/max(1,x),(CELL-ROOT_X-5)/max(1,im.width-x),
                       (ROOT_Y-5)/max(1,y),(CELL-ROOT_Y-5)/max(1,im.height-y)])
    return min(limits),desired


def main():
    OUT.mkdir(exist_ok=True)
    report={'status':'art candidate; not loaded by game','direction_order':ORDER,
            'cell':CELL,'foot_root':(ROOT_X,ROOT_Y),'alpha_noise_cutoff':4,'atlases':{}}
    for name,actions in ACTIONS.items():
        master,meta=read(ART/f'{name}-directions-master.png')
        positions=((1,0),(2,0),(2,1),(2,2),(1,2),(0,2),(0,1),(0,0))
        poses=[pose(master,(int((x+.5)*master.width/3),int((y+.5)*master.height/3)))
               for x,y in positions]
        atlas=Image.new('RGBA',(CELL*4,CELL*2));frames=[]
        for d,item in enumerate(poses):
            scale,planned=safe_scale([item],136 if name=='boar' else 144)
            frames.append(place(atlas,item,scale,d%4,d//4,planned))
        path=OUT/f'{name}-idle-8dir-v1.png';atlas.save(path,optimize=True)
        report['atlases'][f'{name}-idle']={'sources':[meta],'runtime_sha256':sha(path),'frames':frames}
        for action in actions:
            atlas=Image.new('RGBA',(CELL*4,CELL*8));frames=[];sources=[]
            for half in range(2):
                master,meta=read(ART/f'{name}-{action}-{half}-master.png');sources.append(meta)
                for row in range(4):
                    poses=[pose(master,(int((col+.5)*master.width/4),int((row+.5)*master.height/4)))
                           for col in range(4)]
                    assert all(im.width<master.width/4*1.3 and im.height<master.height/4*1.3 for im,_ in poses), 'Merged poses'
                    scale,planned=safe_scale(poses,136 if name=='boar' else 144)
                    batch=[place(atlas,item,scale,col,row+half*4,planned) for col,item in enumerate(poses)]
                    assert len({f['cell_sha256'] for f in batch})==4,'Repeated pose'
                    frames.extend(batch)
            path=OUT/f'{name}-{action}-8dir-v1.png';atlas.save(path,optimize=True)
            report['atlases'][f'{name}-{action}']={'sources':sources,'runtime_sha256':sha(path),'frames':frames}
            print(f'{name}-{action}: eight views, four unique frames/view, safe alpha borders',flush=True)
    (ART/'preparation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('PASS: 176 candidate poses; 7 RGBA atlases; no reflection; common foot root; masters preserved',flush=True)


if __name__=='__main__':
    main()
