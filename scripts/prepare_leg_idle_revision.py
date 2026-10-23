"""Prepare unapproved idle revision review; never fit art to target coordinates."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image
from PIL import ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/gear/legs/cotton-straw'
OUT = ART / 'idle-revision-1'
OUT.mkdir(exist_ok=True)
CROP = (64, 140, 128, 188)

def crop_sheet(source):
    source = source.convert('RGBA')
    sheet = Image.new('RGBA', (2048, 768), (238, 237, 229, 255))
    for i in range(8):
        x, y = i % 4 * 768, i // 4 * 768
        box = tuple(v * 4 + (x if k % 2 == 0 else y) for k, v in enumerate(CROP))
        crop = source.crop(box).resize((512, 384), Image.Resampling.NEAREST)
        sheet.alpha_composite(crop, (i % 4 * 512, i // 4 * 384))
    return sheet

def evaluate():
    source = Image.open(OUT/'generated-source.png').convert('RGBA')
    # Only a whole-sheet resolution normalization is permitted.
    normalized = source.resize((2048,768),Image.Resampling.LANCZOS)
    raw = np.array(normalized)
    rgb = raw[:,:,:3].astype(np.int16)
    pale = (rgb.min(2)>205)&(rgb.max(2)-rgb.min(2)<27)
    bg = Image.fromarray(np.where(pale,0,255).astype('uint8')).copy()
    # Flood from every canvas edge so each separate ivory panel is reached.
    for x,y in [(0,0),(2047,0),(0,767),(2047,767),(512,383),(1024,383),(1536,383)]:
        if bg.getpixel((x,y)) == 0:
            ImageDraw.floodfill(bg,(x,y),128,thresh=0)
    raw[:,:,3] = np.where(np.array(bg)==128,0,255)
    raw[raw[:,:,3]==0,:3]=0
    # Remove only disconnected extraction specks smaller than one native pixel.
    # This does not reshape connected shoes or use any target coordinates.
    active=raw[:,:,3]>0
    visited=np.zeros(active.shape,dtype=bool)
    retained=np.zeros(active.shape,dtype='uint8')
    removed=0
    for sy,sx in zip(*np.where(active)):
        if visited[sy,sx]:
            continue
        stack=[(int(sy),int(sx))]; visited[sy,sx]=True; component=[]
        while stack:
            cy,cx=stack.pop(); component.append((cy,cx))
            for ny,nx in ((cy-1,cx),(cy+1,cx),(cy,cx-1),(cy,cx+1)):
                if 0<=ny<768 and 0<=nx<2048 and ny//384==sy//384 and nx//512==sx//512 and active[ny,nx] and not visited[ny,nx]:
                    visited[ny,nx]=True; stack.append((ny,nx))
        if len(component)>=64:
            for cy,cx in component:
                retained[cy,cx]=255
        else:
            removed+=len(component)
    raw[retained==0]=0
    cut = Image.fromarray(raw)
    legs = Image.new('RGBA',(3072,1536))
    for i in range(8):
        x,y=i%4*512,i//4*384
        tile=cut.crop((x,y,x+512,y+384)).resize((256,192),Image.Resampling.LANCZOS)
        legs.paste(tile,(i%4*768+256,i//4*768+560))
    body = Image.open(ART/'reference/idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST)
    occ=np.array(Image.open(ART/'reference/idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST))>0
    ba=np.array(body); ba[~occ]=0
    la=np.array(legs); la[occ]=0
    legs=Image.fromarray(la)
    master=Image.alpha_composite(legs,Image.fromarray(ba))
    master.save(OUT/'master.png'); legs.save(OUT/'visible-legs.png')
    crop_sheet(master).save(OUT/'after-crops.png')
    guide=np.array(Image.open(ART/'reference/idle-8dir-guide4x.png').convert('RGB')).astype(np.int16)
    gm=(((guide[:,:,2]>guide[:,:,0]+40)&(guide[:,:,2]>guide[:,:,1]+20)&(guide[:,:,1]>65))|((guide[:,:,0]>guide[:,:,1]+40)&(guide[:,:,1]>guide[:,:,2]+40)&(guide[:,:,0]>170)))&~occ
    roi=np.zeros(occ.shape,dtype=bool)
    for i in range(8):
        x,y=i%4*768,i//4*768
        roi[y+560:y+752,x+256:x+512]=True
    gm &= roi
    names=['N','NE','E','SE','S','SW','W','NW']
    results=[]
    for i,name in enumerate(names):
        x,y=i%4*768,i//4*768
        a=la[y:y+768,x:x+768,3]>=128; b=gm[y:y+768,x:x+768]
        yy,xx=np.where(a)
        bottom=float(yy.max()/4)
        results.append({'direction':name,'visible_silhouette_iou':round(int((a&b).sum())/int((a|b).sum()),4),'lowest_visible_leg_y':bottom,'contact_envelope_pass':175<=bottom<=177,'cell_border_clear':not (a[:8].any() or a[-8:].any() or a[:,:8].any() or a[:,-8:].any()),'max_joint_error_px':None,'per_foot_contact_failure_count':None})
    protected=occ&(ba[:,:,3]==255)
    changed=int(np.any(np.array(master)[protected]!=ba[protected],axis=1).sum())
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ART/'reference').iterdir()}
    report={'status':'unapproved_revision_candidate','runtime_ready':False,'generation_attempts_this_revision':2,'selected_attempt':2,'generated_size':source.size,'source_sha256':hashlib.sha256((OUT/'generated-source.png').read_bytes()).hexdigest(),'reference_sha256':hashes,'protected_opaque_pixels_changed':changed,'results':results,'removed_disconnected_source_pixels':removed,'method':'Same native crop (64,140,128,188) per direction; uniform inverse 2x crop enlargement; no per-foot or per-direction fitting, warping or flipping. Original occluder and body copied unchanged. Pale border-connected background extracted, disconnected components smaller than 64 source pixels (one native pixel area) omitted; not a full hidden-leg layer.','not_measured':['Complete independent ankle/heel/toe landmark set','Per-foot contact and projection definition','Full hidden-leg silhouette','SDL/game render'],'visual_review':['E far foot now visible','Two separated transverse straps replace Y shapes in front/side views','N heel loop is clearer but rear-view orientation remains questionable in N/NE/NW','Ankle bands remain too high and several shoes too large']}
    if (OUT/'ankle-observations.json').exists():
        observations=json.loads((OUT/'ankle-observations.json').read_text(encoding='utf-8'))
        frame_joints=json.loads((ART/'reference/leg-guides.json').read_text(encoding='utf-8'))['frames']
        errors=[]
        for i,name in enumerate(names):
            for side,point in observations['points'][name].items():
                px,py=point; observed=[(px-i%4*512)/8+64,(py-i//4*384)/8+140]
                target=frame_joints[f'idle/{i}({name.lower()})'][side]['ankle']
                error=float(np.linalg.norm(np.array(observed)-target))
                errors.append({'direction':name,'side':side,'observed_native':observed,'target_native':target,'error_native_px':round(error,3)})
        report['ankle_only_review']={'method':observations['method'],'max_error_native_px':max(e['error_native_px'] for e in errors),'results':errors,'complete_joint_validation':False}
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    bg=Image.new('RGBA',master.size,(238,237,229,255)); bg.alpha_composite(master)
    d=ImageDraw.Draw(bg)
    joints=json.loads((ART/'reference/leg-guides.json').read_text(encoding='utf-8'))['frames']
    for i,name in enumerate(names):
        x,y=i%4*768,i//4*768
        d.line((x,y+704,x+767,y+704),fill=(40,155,75),width=2)
        for side,j in joints[f'idle/{i}({name.lower()})'].items():
            color=(45,125,230) if side=='left' else (230,120,25)
            for key in ('ankle','heel','toe'):
                qx,qy=j[key]; cx,cy=x+qx*4,y+qy*4
                d.line((cx-7,cy,cx+7,cy),fill=color,width=2)
                d.line((cx,cy-7,cx,cy+7),fill=color,width=2)
    crop_sheet(bg).save(OUT/'alignment-crops.png')
    comparison=Image.new('RGB',(2048,1664),(238,237,229))
    label=ImageDraw.Draw(comparison)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',32)
    label.text((20,12),'BEFORE: PR #43 initial draft | N NE E SE / S SW W NW',font=font,fill=(55,48,41))
    label.text((20,844),'AFTER: revision 1 / attempt 2 | UNAPPROVED - alignment failed',font=font,fill=(120,45,25))
    comparison.paste(Image.open(OUT/'before-crops.png').convert('RGB'),(0,64))
    comparison.paste(Image.open(OUT/'after-crops.png').convert('RGB'),(0,896))
    comparison.save(OUT/'comparison.png')
    print(json.dumps({'protected_opaque_pixels_changed':changed,'results':results},indent=2))

if __name__ == '__main__':
    crop_sheet(Image.open(ART/'reference/idle-8dir-guide4x.png')).save(OUT/'guide-crops.png')
    crop_sheet(Image.open(ART/'design-8dir-master.png')).save(OUT/'before-crops.png')
    (OUT/'crop-layout.json').write_text(json.dumps({'crop_native': CROP, 'cell': [512,384], 'sheet':[2048,768], 'directions':['N','NE','E','SE','S','SW','W','NW'], 'method':'Same fixed crop and uniform 2x enlargement for every 4x reference cell; original files unchanged.'}, indent=2)+'\n',encoding='utf-8')
    if (OUT/'generated-source.png').exists():
        evaluate()
