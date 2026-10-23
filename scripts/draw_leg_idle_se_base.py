"""Fresh absolute-grid SE idle art; E palette/brush utilities only are reused.

No E foot image, guide mask, affine transformation, or game asset is sampled.
"""
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont
import draw_leg_idle_e_grid as brush
from prepare_leg_idle_e_base import PALETTE
from prepare_leg_idle_e_grid_review import band_observations

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/gear/legs/cotton-straw'
OUT = ART / 'idle-se-base-1'
REF = ART / 'reference'
brush.COL.update(PALETTE)


def canvas(): return Image.new('RGBA', (3072, 3072))
def path(im, commands, fill=None, stroke=None, width=.3, closed=False):
    brush.path(im, commands, fill, stroke, width, closed)
def shade(im, commands, color): brush.shade(im, commands, color)
def rope(im, commands, width=.8): brush.rope(im, commands, width)
def save_json(name, value):
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf8', newline='\n')


def trousers(side):
    im = canvas()
    if side == 'far':
        contour=[('M',100.0,137.8),('L',111.3,138.0),('C',110.2,145.6,107.5,151.0,106.4,156.4),('C',105.6,160.1,105.3,162.7,103.8,164.0),('C',102.3,164.5,99.6,164.4,98.1,163.8),('C',97.0,161.2,98.0,158.7,98.4,155.3),('C',99.1,149.9,99.7,144.2,100.0,137.8)]
        light=[('M',102.0,138.0),('L',108.7,138.1),('C',108.1,146.0,103.3,152.6,101.9,158.1),('C',101.1,160.0,101.1,162.0,102.0,163.4),('L',100.1,163.6),('C',98.9,159.7,100.7,153.8,101.4,150.1)]
        shadow=[('M',109.0,139.0),('L',112.0,138.0),('C',110.0,150.0,105.8,161.9,103.8,164.0),('L',102.8,163.4),('C',104.9,157.6,104.8,149.9,109.0,139.0)]
        folds=[[('M',99.5,155.8),('C',100.1,157.6,101.3,158.3,102.1,159.4)], [('M',105.7,158.6),('C',104.1,160.0,102.6,161.0,102.1,162.5)]]
    else:
        contour=[('M',84.4,138.9),('L',99.8,139.0),('C',99.6,148.9,98.5,155.4,97.6,161.5),('C',97.0,165.0,97.3,167.5,95.1,169.2),('C',93.1,169.9,90.1,169.7,88.8,169.1),('C',87.4,165.4,87.1,160.4,86.6,157.1),('C',85.7,151.0,85.3,145.6,84.4,138.9)]
        light=[('M',87.3,139.0),('L',96.4,139.1),('C',96.4,147.9,91.3,155.7,90.8,160.5),('C',90.2,164.0,91.6,166.5,92.2,168.8),('L',90.4,168.6),('C',88.7,164.7,88.5,158.3,89.2,154.3),('C',90.3,147.5,87.6,143.0,87.3,139.0)]
        shadow=[('M',97.3,140.0),('L',100.2,139.0),('C',99.4,153.7,98.3,164.8,95.1,169.2),('L',93.4,168.5),('C',95.1,161.2,93.6,157.0,95.1,151.9)]
        folds=[[('M',87.5,158.1),('C',88.4,159.7,90.5,160.1,91.4,161.3)], [('M',96.7,161.6),('C',94.5,162.2,92.4,164.0,92.3,166.2)], [('M',88.7,166.2),('L',90.3,168.5),('L',91.4,167.8)]]
    path(im,contour,'cotton','outline',.42,True)
    shade(im,light,'cotton_light');shade(im,shadow,'cotton_mid')
    for fold in folds: path(im,fold,stroke='cotton_dark',width=.25)
    return im


def footwear(side):
    im = canvas()
    if side == 'far':
        sole=[('M',95.4,167.6),('C',94.4,165.0,96.0,163.3,98.3,163.8),('C',102.5,164.4,106.9,166.3,110.6,168.1),('C',113.5,169.2,114.1,171.1,112.8,172.8),('C',110.8,175.2,108.3,175.1,105.9,174.1),('C',101.9,172.6,96.8,170.5,95.4,167.6)]
        sock=[('M',98.1,164.3),('C',99.8,164.8,102.4,164.8,103.9,164.3),('L',104.2,165.5),('C',106.2,166.3,107.9,167.8,109.2,167.3),('C',110.5,166.8,112.2,168.2,112.3,169.8),('C',112.3,171.7,110.4,172.8,108.2,172.2),('C',104.9,171.5,101.3,169.5,97.7,168.6),('C',95.4,167.8,95.3,164.8,98.1,164.3)]
        lower=[('M',96.7,166.8),('C',101.7,168.9,106.4,171.8,112.1,170.4),('L',112.5,173.0),('L',105.2,173.6),('L',95.8,169.1)]
        heel=[('M',96.6,166.0),('C',97.5,165.2,99.1,165.8,99.4,166.7),('L',98.3,167.8),('L',96.7,167.0)]
        toe=[('M',108.7,169.5),('C',109.7,168.8,111.1,169.4,111.3,170.1),('L',110.2,171.4),('L',108.7,170.7)]
        loop=[('M',96.3,168.4),('C',94.5,166.1,95.1,163.8,97.2,164.0),('C',98.2,164.2,98.5,165.4,98.0,166.1)]
        straps=[[('M',104.1,165.6),('C',102.8,167.0,101.6,169.1,101.5,170.5)], [('M',109.7,167.5),('C',108.0,169.1,107.1,171.2,107.1,172.8)]]
        rim=[('M',95.9,167.8),('C',99.9,171.7,108.6,175.7,112.9,171.7)]
        band=[('M',98.1,163.8),('C',99.7,164.1,102.3,164.3,104.0,164.0),('L',103.8,165.8),('C',102.1,166.3,99.2,166.1,97.8,165.5),('L',98.1,163.8)]
        bandlight=[('M',98.4,164.5),('C',100.0,165.1,102.0,165.2,103.6,164.8)]
    else:
        sole=[('M',86.1,172.7),('C',85.0,170.0,86.2,168.6,88.7,169.0),('C',92.9,169.8,97.0,171.3,101.4,173.4),('C',104.1,174.6,105.6,176.6,104.0,178.4),('C',101.9,180.6,99.6,180.4,97.2,179.4),('C',93.6,178.1,87.4,175.5,86.1,172.7)]
        sock=[('M',88.9,169.5),('C',90.5,170.0,93.4,170.1,95.2,169.5),('L',95.4,170.9),('C',97.4,171.8,99.0,173.4,100.1,172.8),('C',101.9,172.5,103.9,174.1,103.5,175.7),('C',103.4,177.8,101.0,178.3,99.0,177.7),('C',95.1,176.8,91.4,174.7,88.2,173.7),('C',86.2,173.0,86.2,169.5,88.9,169.5)]
        lower=[('M',87.4,172.5),('C',91.8,174.4,98.4,178.3,103.4,176.1),('L',103.8,179.1),('L',96.7,179.0),('L',86.2,174.1)]
        heel=[('M',87.7,171.3),('C',88.8,170.8,90.1,171.0,90.4,172.0),('L',89.1,173.2),('L',87.9,172.4)]
        toe=[('M',99.2,174.8),('C',100.1,174.2,102.0,174.9,102.2,175.6),('L',100.7,176.8),('L',99.3,176.0)]
        loop=[('M',86.9,173.9),('C',85.0,171.5,85.6,169.0,87.9,169.2),('C',89.0,169.5,89.3,170.6,88.9,171.5)]
        straps=[[('M',95.2,171.0),('C',93.7,172.5,92.6,174.4,92.4,175.9)], [('M',100.6,173.0),('C',99.3,174.3,98.3,176.3,98.3,178.0)]]
        rim=[('M',86.6,173.1),('C',90.6,177.0,100.0,181.2,104.0,177.0)]
        band=[('M',88.7,169.1),('C',90.3,169.6,93.0,169.7,95.4,169.3),('L',95.2,171.1),('C',93.5,171.7,90.3,171.6,88.5,170.8),('L',88.7,169.1)]
        bandlight=[('M',89.0,169.9),('C',90.6,170.4,93.1,170.5,95.0,170.0)]
    path(im,sole,'straw','rope_dark',.4,True)
    path(im,sock,'sock','outline',.3,True)
    shade(im,lower,'sock_shadow');shade(im,heel,'sock_light');shade(im,toe,'sock_light')
    rope(im,loop,.65)
    for commands in straps:rope(im,commands,.82)
    rope(im,rim,.82)
    path(im,band,'indigo','indigo_dark',.28,True)
    path(im,bandlight,stroke='indigo_mid',width=.25)
    return im


def flatten(im):
    return Image.alpha_composite(Image.new('RGBA',im.size,(238,237,229,255)), im.convert('RGBA')).convert('RGB')


def draw():
    OUT.mkdir(exist_ok=True)
    originals={}
    for side in ('far','near'):
        foot=footwear(side)
        foot.resize((768,768),Image.Resampling.LANCZOS).save(OUT/(side+'-foot-only.png'))
        im=trousers(side);im.alpha_composite(foot);originals[side]=im
        im.resize((768,768),Image.Resampling.LANCZOS).save(OUT/(side+'-unoccluded.png'))
    legs=Image.alpha_composite(originals['far'],originals['near']).resize((768,768),Image.Resampling.LANCZOS)
    legs.save(OUT/'painted-legs-se.png')
    body=np.array(Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST).crop((2304,0,3072,768)))
    occ=np.array(Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST).crop((2304,0,3072,768)))>0
    body[~occ]=0;visible=np.array(legs);visible[occ]=0
    master=Image.alpha_composite(Image.fromarray(visible),Image.fromarray(body));master.save(OUT/'master-se.png')
    Image.fromarray(visible).save(OUT/'visible-legs-se.png')
    raw={'method':'Output PNG RGB and alpha observations before target coordinates are loaded. Foot bounds include sandals, not anatomical centerline endpoints.', 'visible_bands':band_observations(Image.fromarray(visible)), 'isolated':{}}
    for side in ('far','near'):
        im=Image.open(OUT/(side+'-unoccluded.png'))
        a=np.array(Image.open(OUT/(side+'-foot-only.png')));ys,xs=np.where(a[:,:,3]>=128)
        raw['isolated'][side]={'bands':band_observations(im),'shoe_bbox_native':[float(xs.min()/4),float(ys.min()/4),float(xs.max()/4),float(ys.max()/4)],'shoe_bbox_size_native':[float((xs.max()-xs.min()+1)/4),float((ys.max()-ys.min()+1)/4)],'lowest_outsole_y':float(ys.max()/4)}
    save_json('observations.json',raw)
    targets=json.loads((REF/'leg-guides.json').read_text('utf8'))['frames']['idle/3(se)']
    comparisons=[]
    visible_comparisons=[]
    for key,band in zip(('left','right'),raw['visible_bands']):
        visible_comparisons.append({'side':key,'observed_native':band['center_native'],'guide_native':targets[key]['ankle'],'distance_px':round(float(np.linalg.norm(np.array(band['center_native'])-targets[key]['ankle'])),3)})
    for side,key in (('far','left'),('near','right')):
        for band in raw['isolated'][side]['bands']:
            comparisons.append({'part':side,'observed_native':band['center_native'],'guide_native':targets[key]['ankle'],'distance_px':round(float(np.linalg.norm(np.array(band['center_native'])-targets[key]['ankle'])),3)})
    guideim=Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((2304,0,3072,768));g=np.array(guideim).astype(np.int16)
    gm=(((g[:,:,2]>g[:,:,0]+40)&(g[:,:,2]>g[:,:,1]+20)&(g[:,:,1]>65))|((g[:,:,0]>g[:,:,1]+40)&(g[:,:,1]>g[:,:,2]+40)&(g[:,:,0]>170)))&~occ
    roi=np.zeros(occ.shape,bool);roi[552:752,304:480]=True;gm&=roi
    active=visible[:,:,3]>=128;opaque=occ&(body[:,:,3]==255)
    report={'direction':'SE','baseline_commit':'eea8d39ed335bb3b9d9709452c0a8ab8b5459ee4','method':'New absolute-coordinate paths; E palette and brush functions only reused. No foot image transformation or guide silhouette clipping.', 'visible_silhouette_iou':round(float((active&gm).sum()/(active|gm).sum()),4),'isolated_band_comparisons':comparisons,'visible_band_observations':raw['visible_bands'],'protected_opaque_pixels_changed':int(np.any(np.array(master)[opaque]!=body[opaque],axis=1).sum()),'cell_border_clear':not(active[:8].any() or active[-8:].any() or active[:,:8].any() or active[:,-8:].any()),'literal_contact_y175_177':{side:{'observed_outsole_y':q['lowest_outsole_y'],'pass':175<=q['lowest_outsole_y']<=177} for side,q in raw['isolated'].items()},'anatomical_joint_max_error':None,'overall_idle_approved':False,'runtime_ready':False,'pending':['User visual review; permission to continue did not approve unseen E art','Anatomical heel/toe centers remain unverified; sword occludes far foot','Literal common screen-ground rule conflicts with centerline-plus-sole-depth guidance; do not reinterpret contact or move feet to force a pass']}
    report['visible_band_comparisons']=visible_comparisons
    report['isolated_shoe_sizes']={side:{'bbox_native':q['shoe_bbox_native'],'bbox_size_native':q['shoe_bbox_size_native']} for side,q in raw['isolated'].items()}
    report['shoe_size_note']='Raster bounding boxes include straw, heel loop and sock; not comparable to the guide heel-to-toe centerline length as if both measured the same thing.'
    report['gates']={'visible_iou_0_75_pass':report['visible_silhouette_iou']>=.75,'visible_band_centers_1_5px_pass':len(visible_comparisons)==2 and all(q['distance_px']<=1.5 for q in visible_comparisons),'overall_idle_approved':False}
    save_json('validation.json',report);save_json('palette.json',PALETTE)
    before=Image.open(ART/'idle-revision-1/master.png').crop((2304,0,3072,768))
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
    compare=Image.new('RGB',(1536,960),(238,237,229));d=ImageDraw.Draw(compare)
    for i,(label,im) in enumerate((('FIXED GUIDE',guideim),('BEFORE / generated SE',before),('AFTER / new SE candidate',master))):
        d.text((i*512+12,14),label,font=font,fill='#332c24')
        compare.paste(flatten(im).resize((512,512),Image.Resampling.NEAREST),(i*512,48))
        compare.paste(flatten(im).crop((304,536,480,736)).resize((352,400),Image.Resampling.NEAREST),(i*512+80,560))
    compare.save(OUT/'comparison.png')
    alignment=flatten(master);d=ImageDraw.Draw(alignment);d.line((304,704,480,704),fill='#299b41',width=2)
    for side,color in (('left','#2878e6'),('right','#e67819')):
        for name in ('ankle','heel','toe'):
            x,y=np.array(targets[side][name])*4;d.line((x-5,y,x+5,y),fill=color,width=2);d.line((x,y-5,x,y+5),fill=color,width=2)
    alignment.crop((304,536,480,736)).resize((704,800),Image.Resampling.NEAREST).save(OUT/'alignment.png')
    inspect=Image.new('RGB',(1024,560),(238,237,229));d=ImageDraw.Draw(inspect)
    for i,side in enumerate(('far','near')):
        d.text((i*512+16,16),side+' / unoccluded',font=font,fill='#332c24')
        inspect.paste(flatten(Image.open(OUT/(side+'-unoccluded.png'))).crop((320,608,480,744)).resize((480,408),Image.Resampling.NEAREST),(i*512+16,72))
    inspect.save(OUT/'isolated-feet.png')
    print(json.dumps(report,ensure_ascii=False,indent=2))


if __name__=='__main__':draw()
