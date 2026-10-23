"""Prepare an unapproved design review; never move or fit individual legs."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'art/hero/gear/legs/cotton-straw'
REF = OUT / 'reference'

source = Image.open(OUT/'design-generated-source.png').convert('RGBA')
raw = np.array(source)
rgb = raw[:,:,:3].astype(np.int16)
# Flood the pale source background. Enclosed sock highlights remain foreground.
background = (rgb.min(2) > 205) & ((rgb.max(2)-rgb.min(2)) < 27)
mask = Image.fromarray(np.where(background, 0, 255).astype('uint8')).copy()
ImageDraw.floodfill(mask, (0,0), 128, thresh=0)
fg = np.array(mask) != 128
raw[:,:,3] = np.where(fg,255,0)
raw[~fg,:3] = 0
paint = Image.fromarray(raw).resize((3072,1536), Image.Resampling.LANCZOS)
arr = np.array(paint)
body = Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST)
occ = Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST)
protect = np.array(occ) > 0
roi = np.zeros((1536,3072), dtype=bool)
for i in range(8):
    x,y=(i%4)*768,(i//4)*768
    roi[y+140*4:y+185*4,x+50*4:x+136*4]=True
keep = roi & ~protect
arr[~keep]=0
legs = Image.fromarray(arr)
ba=np.array(body)
ba[~protect]=0
protected_body=Image.fromarray(ba)
composite=Image.alpha_composite(legs,protected_body)
composite.save(OUT/'design-8dir-master.png')
legs.save(OUT/'design-visible-legs-review.png')
comp=np.array(composite)
opaque_protect=protect & (ba[:,:,3]==255)
changed_protected=int(np.count_nonzero(np.any(comp[opaque_protect]!=ba[opaque_protect],axis=1)))

guide=np.array(Image.open(REF/'idle-8dir-guide4x.png').convert('RGB')).astype(np.int16)
blue=(guide[:,:,2]>guide[:,:,0]+40)&(guide[:,:,2]>guide[:,:,1]+20)&(guide[:,:,1]>65)
orange=(guide[:,:,0]>guide[:,:,1]+40)&(guide[:,:,1]>guide[:,:,2]+40)&(guide[:,:,0]>170)
guide_visible=(blue|orange)&keep
drawn=arr[:,:,3]>=128
names=['N','NE','E','SE','S','SW','W','NW']
data=json.loads((REF/'leg-guides.json').read_text(encoding='utf-8'))
results=[]
for i,name in enumerate(names):
    x,y=(i%4)*768,(i//4)*768
    a=drawn[y:y+768,x:x+768]; b=guide_visible[y:y+768,x:x+768]
    union=int(np.count_nonzero(a|b)); intersection=int(np.count_nonzero(a&b))
    yy,xx=np.where(a)
    bottom=None if not len(yy) else float(yy.max()/4)
    edge=bool(a[:8].any() or a[-8:].any() or a[:,:8].any() or a[:,-8:].any())
    results.append({'direction':name,'max_joint_error_px':None,'joint_error_status':'not_measured_independent_heel_toe_landmarks_required',
        'visible_silhouette_iou':None if not union else round(intersection/union,4),
        'lowest_visible_leg_y':bottom,'contact_envelope_pass':bottom is not None and 175<=bottom<=177,
        'per_foot_contact_failure_count':None,'per_foot_contact_status':'not_measured_feet_overlap_and_far_foot_perspective',
        'cell_border_clear':not edge})

bg=Image.new('RGBA',(3072,1536),(238,237,229,255));bg.alpha_composite(composite)
font_path='C:/Windows/Fonts/arial.ttf'
font=ImageFont.truetype(font_path,36)
preview=Image.new('RGB',(2048,704),(238,237,229))
draw=ImageDraw.Draw(preview)
for i,name in enumerate(names):
    x,y=(i%4)*768,(i//4)*768
    crop=bg.crop((x+196,y+548,x+548,y+744)).resize((440,245),Image.Resampling.NEAREST).convert('RGB')
    px,py=(i%4)*512+36,(i//4)*352+58
    preview.paste(crop,(px,py))
    draw.text((px,py-46),name,font=font,fill=(55,48,41))
preview.save(OUT/'design-8dir-closeups.png')

# Reference overlay makes registration errors reviewable without fitting the art.
audit=bg.copy(); d=ImageDraw.Draw(audit)
for i,name in enumerate(names):
    x,y=(i%4)*768,(i//4)*768
    d.line((x,y+704,x+767,y+704),fill=(40,155,75,255),width=2)
    joints=data['frames'][f'idle/{i}({name.lower()})']
    for side,j in joints.items():
        color=(45,125,230,255) if side=='left' else (230,120,25,255)
        for key in ('ankle','heel','toe'):
            qx,qy=j[key]; cx,cy=x+qx*4,y+qy*4
            d.line((cx-7,cy,cx+7,cy),fill=color,width=2)
            d.line((cx,cy-7,cx,cy+7),fill=color,width=2)
audit.save(OUT/'design-alignment-review.png')
hashes={'leg-gear-for-gpt.zip': 'e5c1f2fd5fe6ad82da70f81ff022d4e0e24aa10a64cea8a0f9075856204eaf70'}
for p in (OUT/'design-generated-source.png',*(OUT/'reference').iterdir()):
    hashes[p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
report={'stage':'1_design_candidate_only','approval_status':'pending','runtime_ready':False,
    'generator':'image_gen','generation_attempts':4,'selected_attempt':4,
    'generated_size':list(source.size),'master_size':[3072,1536],'frame_cell_px':768,
    'canvas_resolution_normalization':'Entire 1774x887 output uniformly resampled to 3072x1536 with Lanczos; no per-cell translations, fitting, mirroring or warping.',
    'body_preservation':'Original supplied 192px body expanded 4x nearest-neighbour; only original occluder-protected pixels copied. Existing boot remnants outside the occluder omitted in composite; input files unchanged.',
    'protected_opaque_body_pixel_count':int(np.count_nonzero(opaque_protect)),
    'protected_opaque_body_pixels_changed':changed_protected,
    'silhouette_measurement':'Visible guide blue/orange color mask, outside provided occluder, versus actual extracted art alpha>=128. Not full hidden-leg IoU; source full legs behind skirt have not been generated.',
    'thresholds':{'joint_error_px':1.5,'silhouette_iou':0.75,'contact_y_range':[175,177]},
    'results':results,'source_sha256':hashes,
    'not_tested':['Independent heel/toe/ankle landmark error','Per-foot contact','Walking naturalness and flicker','Game or SDL rendering','Fully hidden trouser legs'],
    'known_issues':['Generated leg registration must pass independent joint tests before runtime use.','Some directions do not meet visible guide IoU or contact envelope.','Far feet have perspective y offsets in the supplied guide despite all idle contact flags being true.','N/NE/NW heel-loop and toe-away readability still needs art approval.']}
(OUT/'preparation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'protected_opaque_pixels_changed':changed_protected,'results':results},ensure_ascii=True,indent=2))
