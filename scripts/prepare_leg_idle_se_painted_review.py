"""Assemble generated 2D equipment with canonical body pixels; never fit feet.

The painting and separation are image-generation edits, not programmatic art.
This script only scales the complete square canvas uniformly, composites the
immutable body/occluder, and observes the resulting PNGs.
"""
import hashlib
import json
from pathlib import Path
import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from prepare_leg_idle_e_grid_review import band_observations

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/hero/gear/legs/cotton-straw'
OUT=ART/'idle-se-painted-1'
REF=ART/'reference'


def write(name,value):
    (OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')


def flatten(im):
    return Image.alpha_composite(Image.new('RGBA',im.size,(238,237,229,255)),im.convert('RGBA')).convert('RGB')


def review():
    source=Image.open(OUT/'generated-legs-source.png').convert('RGBA')
    assert source.width==source.height, 'Only uniform full-square normalization is allowed'
    legs=source.resize((768,768),Image.Resampling.LANCZOS)
    legs.save(OUT/'legs-se.png')
    body=np.array(Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST).crop((2304,0,3072,768)))
    occ=np.array(Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST).crop((2304,0,3072,768)))>0
    body[~occ]=0
    visible=np.array(legs);visible[occ]=0
    master=Image.alpha_composite(Image.fromarray(visible),Image.fromarray(body))
    master.save(OUT/'master-se.png');Image.fromarray(visible).save(OUT/'visible-legs-se.png')
    # Observe output first. No guide coordinates or authoring points are inputs.
    observed={'method':'Final PNG RGB connected components and alpha bounds; no guide inputs',
              'visible_bands_y150_plus':band_observations(Image.fromarray(visible)),
              'unoccluded_bands_y150_plus':band_observations(legs),
              'band_detector_limit':'Legacy observer only searches y>=150 native; a high far band can be omitted. Missing detections are not a pass.',
              'isolated_parts':{}}
    a=np.array(legs);count,labels,stats,centers=cv2.connectedComponentsWithStats((a[:,:,3]>=128).astype('uint8'),8)
    parts=sorted([(i,int(q[4])) for i,q in enumerate(stats) if i>0 and q[4]>=60],key=lambda q:q[1],reverse=True)[:2]
    parts=sorted(parts,key=lambda q:float(centers[q[0]][0]))
    for side,(label,area) in zip(('near','far'),parts):
        part=a.copy();part[labels!=label]=0
        Image.fromarray(part).save(OUT/(side+'-isolated.png'))
        ys,xs=np.where(labels==label)
        # Local RGB segmentation includes high bands omitted by the legacy observer.
        c=part.astype(np.int16)
        blue=(c[:,:,3]>=128)&(c[:,:,2]>c[:,:,0]+7)&(c[:,:,2]>c[:,:,1]+5)&(c[:,:,0]<100)
        by,bx=np.where(blue)
        band={'bbox_center_native':[(float(bx.min()+bx.max())/2)/4,(float(by.min()+by.max())/2)/4],
              'bbox_native':[float(bx.min()/4),float(by.min()/4),float(bx.max()/4),float(by.max()/4)]} if len(bx) else None
        observed['isolated_parts'][side]={'alpha_component_area_4x':area,'lowest_outsole_y':float(ys.max()/4),'bounds_native':[float(xs.min()/4),float(ys.min()/4),float(xs.max()/4),float(ys.max()/4)],'blue_band':band}
    write('observations.json',observed)
    targets=json.loads((REF/'leg-guides.json').read_text('utf8'))['frames']['idle/3(se)']
    comparisons=[]
    for side,key in (('near','right'),('far','left')):
        band=observed['isolated_parts'].get(side,{}).get('blue_band')
        if band:
            comparisons.append({'side':side,'observed_native':band['bbox_center_native'],'guide_native':targets[key]['ankle'],'distance_px':round(float(np.linalg.norm(np.array(band['bbox_center_native'])-targets[key]['ankle'])),3)})
    guideim=Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((2304,0,3072,768));g=np.array(guideim).astype(np.int16)
    gm=(((g[:,:,2]>g[:,:,0]+40)&(g[:,:,2]>g[:,:,1]+20)&(g[:,:,1]>65))|((g[:,:,0]>g[:,:,1]+40)&(g[:,:,1]>g[:,:,2]+40)&(g[:,:,0]>170)))&~occ
    roi=np.zeros(occ.shape,bool);roi[552:752,304:480]=True;gm&=roi
    active=visible[:,:,3]>=128;opaque=occ&(body[:,:,3]==255)
    report={'status':'painterly_style_study_unapproved_not_aligned','direction':'SE','baseline_commit':'a9b6712174236047b1a01b25e6ada49873258d78',
        'production_decision':'Modular hand-painted 2D equipment matching the original character. User rejected procedural flat/vector candidates; those are historical records only.',
        'generation_steps':['Full-pose painterly style edit','Image edit separating leg equipment and filling hidden areas'],
        'source_sizes':{name:list(Image.open(OUT/name).size) for name in ('generated-source.png','generated-legs-source.png')},
        'normalization':'Uniform full-square resize to 768px. No foot translation, independent scaling, warping, reflection, or guide-silhouette clipping.',
        'protected_opaque_pixels_changed':int(np.any(np.array(master)[opaque]!=body[opaque],axis=1).sum()),
        'visible_silhouette_iou':round(float((active&gm).sum()/(active|gm).sum()),4),
        'isolated_band_comparisons':comparisons,
        'literal_contact_y175_177':{side:{'observed_outsole_y':q['lowest_outsole_y'],'pass':175<=q['lowest_outsole_y']<=177} for side,q in observed['isolated_parts'].items()},
        'cell_border_clear':not(active[:8].any() or active[-8:].any() or active[:,:8].any() or active[:,-8:].any()),
        'anatomical_joint_max_error':None,'overall_idle_approved':False,'runtime_ready':False,
        'art_review':{'cloth':'Painterly overlapping folds and midtones restored; user approval pending','sandals':'Two separate instep straps and woven sole detail visible in separated artwork; original sword occludes far foot in composite','assembly':'Separation edit changed proportions/placement; far ankle is high and far foot too far right. Canonical occluder exposes uneven transition. Do not use as aligned equipment.'},
        'pending':['User approval of painterly material treatment','Redraw proportions/placement at the fixed frame while preserving this art quality','Anatomical heel/toe validation','Existing contact-contract ambiguity remains unresolved'],
        'reference_sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in REF.iterdir() if p.is_file()}}
    write('validation.json',report)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',22)
    compare=Image.new('RGB',(1536,1100),(238,237,229));d=ImageDraw.Draw(compare)
    images=[('PREVIOUS PAINTERLY REFERENCE',Image.open(OUT/'paint-reference-se.png')),('REJECTED FLAT STUDY',Image.open(ART/'idle-se-base-1/master-se.png')),('NEW PAINT / ALIGNMENT PENDING',master)]
    for i,(label,im) in enumerate(images):
        d.text((i*512+8,16),label,font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',17),fill='#332c24')
        compare.paste(flatten(im).resize((512,512),Image.Resampling.NEAREST),(i*512,52))
        compare.paste(flatten(im).crop((280,536,552,752)).resize((504,400),Image.Resampling.NEAREST),(i*512+4,580))
    d.text((16,1012),'Original upper body, skirt and sword pixels restored. New equipment is a STYLE STUDY, not game-ready.',font=font,fill='#332c24')
    compare.save(OUT/'comparison.png')
    beauty=Image.new('RGB',(1152,768),(238,237,229));beauty.paste(flatten(master),(0,0));beauty.paste(flatten(legs).crop((280,536,552,752)).resize((544,432),Image.Resampling.NEAREST),(608,220));beauty.save(OUT/'preview.png')
    alignment=flatten(master);d=ImageDraw.Draw(alignment);d.line((280,704,552,704),fill='#299b41',width=2)
    for side,color in (('left','#2878e6'),('right','#e67819')):
        for name in ('ankle','heel','toe'):
            x,y=np.array(targets[side][name])*4;d.line((x-5,y,x+5,y),fill=color,width=2);d.line((x,y-5,x,y+5),fill=color,width=2)
    alignment.crop((280,536,552,752)).resize((816,648),Image.Resampling.NEAREST).save(OUT/'alignment.png')
    print(json.dumps({k:report[k] for k in ('status','protected_opaque_pixels_changed','visible_silhouette_iou','isolated_band_comparisons','literal_contact_y175_177')},ensure_ascii=False,indent=2))


if __name__=='__main__':review()
