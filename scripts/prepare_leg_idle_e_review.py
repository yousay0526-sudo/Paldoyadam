"""Fixed-frame E-only review preparation; never fit individual art landmarks."""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'art/hero/gear/legs/cotton-straw'
OUT = ART / 'idle-e-revision-1'
REF = ART / 'reference'
CROP = (64, 128, 128, 192)
SELECTED = 3

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def flatten(im):
    bg=Image.new('RGBA',im.size,(238,237,229,255))
    bg.alpha_composite(im.convert('RGBA'))
    return bg.convert('RGB')

def extract(source):
    """Border-connected pale background only; no guide-based silhouette cutting."""
    raw=np.array(source.convert('RGBA'))
    rgb=raw[:,:,:3].astype(np.int16)
    pale=(rgb.min(2)>205)&((rgb.max(2)-rgb.min(2))<27)
    mask=Image.fromarray(np.where(pale,0,255).astype('uint8')).copy()
    w,h=mask.size
    for x,y in ([(x,0) for x in range(w)]+[(x,h-1) for x in range(w)]+
                [(0,y) for y in range(h)]+[(w-1,y) for y in range(h)]):
        if mask.getpixel((x,y))==0:
            ImageDraw.floodfill(mask,(x,y),128,thresh=0)
    raw[np.array(mask)==128]=0
    return Image.fromarray(raw)

def review():
    body=Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768))
    occ=np.array(Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768)))>0
    ba=np.array(body); ba[~occ]=0
    guide=np.array(Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((1536,0,2304,768))).astype(np.int16)
    gm=(((guide[:,:,2]>guide[:,:,0]+40)&(guide[:,:,2]>guide[:,:,1]+20)&(guide[:,:,1]>65))|((guide[:,:,0]>guide[:,:,1]+40)&(guide[:,:,1]>guide[:,:,2]+40)&(guide[:,:,0]>170)))&~occ
    common=np.zeros(occ.shape,dtype=bool); common[560:752,256:512]=True
    original=np.array(Image.open(ART/'idle-revision-1/master.png').crop((1536,0,2304,768)).convert('RGBA'))
    oldlegs=np.array(Image.open(ART/'idle-revision-1/visible-legs.png').crop((1536,0,2304,768)).convert('RGBA'))
    results=[]; masters={}; legs_by_attempt={}
    def metrics(la):
        a=la[:,:,3]>=128; b=gm&common
        ys,xs=np.where(a)
        return {'visible_silhouette_iou':round(int((a&b).sum())/int((a|b).sum()),4),
                'lowest_visible_leg_y':float(ys.max()/4),
                'global_envelope_175_177_pass':bool(175<=ys.max()/4<=177),
                'visible_bounds_native':[float(xs.min()/4),float(ys.min()/4),float(xs.max()/4),float(ys.max()/4)],
                'outside_previous_review_roi_pixels_4x':int((a&~common).sum()),
                'cell_border_clear':not(a[:8].any() or a[-8:].any() or a[:,:8].any() or a[:,-8:].any())}
    for attempt in (1,2,3):
        source=Image.open(OUT/f'source-attempt{attempt}.png').convert('RGBA')
        assert source.width==source.height, 'Non-square source requires review; no anisotropic resize permitted'
        cut=extract(source).resize((256,256),Image.Resampling.LANCZOS)
        legs=Image.new('RGBA',(768,768)); legs.paste(cut,(256,512))
        la=np.array(legs); la[occ]=0
        master=Image.alpha_composite(Image.fromarray(la),Image.fromarray(ba))
        opaque=occ&(ba[:,:,3]==255)
        changed=int(np.any(np.array(master)[opaque]!=ba[opaque],axis=1).sum())
        m={'attempt':attempt,'source_size':list(source.size),'source_sha256':sha(OUT/f'source-attempt{attempt}.png'),
           'protected_opaque_pixels_changed':changed,**metrics(la)}
        results.append(m); masters[attempt]=master; legs_by_attempt[attempt]=la
        master.save(OUT/f'attempt{attempt}-master-e.png')
    master=masters[SELECTED]; la=legs_by_attempt[SELECTED]
    master.save(OUT/'master-e.png'); Image.fromarray(la).save(OUT/'visible-legs-e.png')
    before=Image.fromarray(original); before.save(OUT/'before-e.png')
    # Diagnostic thumbnails use the same fixed crop and uniform scale for all panels.
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',23)
    panel=Image.new('RGB',(2048,580),(238,237,229)); d=ImageDraw.Draw(panel)
    for i,(name,im) in enumerate([('BEFORE / revision 1',before),*[(f'E attempt {n}',masters[n]) for n in (1,2,3)]]):
        tile=flatten(im).crop((256,512,512,768)).resize((512,512),Image.Resampling.NEAREST)
        panel.paste(tile,(i*512,52)); d.text((i*512+10,12),name,font=font,fill=(50,45,40))
    panel.save(OUT/'attempts-comparison.png')
    compare=Image.new('RGB',(1536,584),(238,237,229)); d=ImageDraw.Draw(compare)
    for i,(name,im) in enumerate([('FIXED GUIDE',Image.open(REF/'idle-8dir-guide4x.png').crop((1536,0,2304,768))),('BEFORE / revision 1',before),('AFTER / E attempt 3 / UNAPPROVED',master)]):
        tile=flatten(im).crop((256,512,512,768)).resize((512,512),Image.Resampling.NEAREST)
        compare.paste(tile,(i*512,56)); d.text((i*512+8,10),name,font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',19),fill=(50,45,40))
    compare.save(OUT/'comparison.png')
    joints=json.loads((REF/'leg-guides.json').read_text('utf-8'))['frames']['idle/2(e)']
    overlay=flatten(master); d=ImageDraw.Draw(overlay)
    d.line((0,704,767,704),fill=(35,155,65),width=2)
    for side,points in joints.items():
        color=(40,120,230) if side=='left' else (230,120,25)
        for key in ('ankle','heel','toe'):
            x,y=points[key]; x*=4;y*=4
            d.line((x-6,y,x+6,y),fill=color,width=2); d.line((x,y-6,x,y+6),fill=color,width=2)
    overlay.crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST).save(OUT/'alignment.png')
    # Detect ankle bands from source RGB, without reading any guide point.
    # Restrict to lower image to exclude blue armor, then report every large component.
    band_reports=[]
    for attempt in (1,2,3):
        raw=np.array(Image.open(OUT/f'source-attempt{attempt}.png').convert('RGB')).astype(np.int16)
        h,w=raw.shape[:2]
        active=(raw[:,:,2]>raw[:,:,0]+5)&(raw[:,:,2]>raw[:,:,1]+1)&(raw[:,:,0]<95)&(raw[:,:,2]<155)
        active[:int(h*.44)]=False
        visited=np.zeros(active.shape,bool); components=[]
        for sy,sx in zip(*np.where(active)):
            if visited[sy,sx]: continue
            todo=[(int(sy),int(sx))];visited[sy,sx]=True; pts=[]
            while todo:
                cy,cx=todo.pop();pts.append((cy,cx))
                for ny,nx in ((cy-1,cx),(cy+1,cx),(cy,cx-1),(cy,cx+1)):
                    if 0<=ny<h and 0<=nx<w and active[ny,nx] and not visited[ny,nx]:
                        visited[ny,nx]=True;todo.append((ny,nx))
            if len(pts)>=100:
                ys,xs=np.array(pts).T
                components.append({'area':len(pts),'source_bounds':[int(xs.min()),int(ys.min()),int(xs.max()),int(ys.max())],
                                   'source_bbox_center':[(int(xs.min())+int(xs.max()))/2,(int(ys.min())+int(ys.max()))/2]})
        band_reports.append({'attempt':attempt,'components':sorted(components,key=lambda c:-c['area'])[:4]})
    report={'status':'unapproved_e_only_candidate','runtime_ready':False,'selected_attempt':SELECTED,
            'baseline_commit':'d8a87c289870a666d38ae24923abce7d5e6b221f','before':metrics(oldlegs),'attempts':results,
            'source_color_band_detections':band_reports,
            'reference_sha256':{p.name:sha(p) for p in REF.iterdir()},
            'method':'Fixed E crop (64,128,128,192); border-connected pale background removal; uniform square source to 256x256; paste at fixed (256,512) in 768px E cell; original occluder/body composited. No individual foot translation, warp, mirror, guide clipping or coordinate edits. IoU uses previous common ROI for guide and full candidate alpha (outside-ROI drawing remains penalized).',
            'full_joint_validation_complete':False,'per_foot_contact_pass':None,
            'not_measured':['Far-foot toe occluded; internal anatomical heel/toe need review','Far-foot contact screen-height projection definition','Game/SDL/mobile rendering'],
            'generation_path_issue':'Windows referenced_image_paths rejected before generation; same inspected references supplied through recent conversation images. Three successful E-only generations.'}
    if (OUT/'observations.json').exists():
        obs=json.loads((OUT/'observations.json').read_text('utf-8'))
        entries=[]
        for item in obs['landmarks']:
            q={**item}; xy=item.get('source_xy')
            if xy is not None:
                native=[64+xy[0]*64/obs['source_size'][0],128+xy[1]*64/obs['source_size'][1]]
                target=joints[item['side']][item['landmark']]
                q.update(observed_native=native,target_native=target,error_native_px=round(float(np.linalg.norm(np.array(native)-target)),3))
            entries.append(q)
        report['independent_observations']={'method':obs['method'],'landmarks':entries,
            'max_measurable_landmark_error_native_px':max(q['error_native_px'] for q in entries if 'error_native_px' in q),
            'full_joint_max_error_native_px':None}
        report['shoe_review']=obs['shoe_review']
        report['per_foot_contact']=obs['per_foot_contact']
        near={q['landmark']:q for q in entries if q['side']=='right'}
        report['near_foot_centerline_span']={
            'observed_horizontal_native_px':round(near['toe']['observed_native'][0]-near['heel']['observed_native'][0],3),
            'guide_horizontal_native_px':joints['right']['toe'][0]-joints['right']['heel'][0],
            'note':'Heel/toe centerline estimates, not external shoe width; two manual endpoints each uncertain by +/-1 native px.'}
        annotated=flatten(master); dd=ImageDraw.Draw(annotated)
        for q in entries:
            if 'observed_native' not in q: continue
            x,y=np.array(q['observed_native'])*4
            dd.ellipse((x-4,y-4,x+4,y+4),outline=(0,180,165),width=2)
        annotated.crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST).save(OUT/'observed-landmarks.png')
        report['ankle_max_error_native_px']=max(q['error_native_px'] for q in entries if q['landmark']=='ankle' and 'error_native_px' in q)
        report['gates']={'visible_silhouette_iou_0_75_pass':results[SELECTED-1]['visible_silhouette_iou']>=.75,
                         'ankle_centers_1_5px_pass':report['ankle_max_error_native_px']<=1.5,
                         'global_envelope_175_177_pass':results[SELECTED-1]['global_envelope_175_177_pass'],
                         'full_joint_validation_pass':False,'per_foot_contact_pass':None,
                         'overall_idle_approved':False}
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'before':report['before'],'attempts':results,'bands':band_reports},indent=2))

def prepare():
    OUT.mkdir(exist_ok=True)
    for name, source in [('guide-input.png', REF/'idle-8dir-guide4x.png'),
                         ('before-input.png', ART/'idle-revision-1/master.png')]:
        im = Image.open(source).convert('RGBA')
        box = (1536 + CROP[0]*4, CROP[1]*4,
               1536 + CROP[2]*4, CROP[3]*4)
        crop = im.crop(box).resize((1024,1024), Image.Resampling.NEAREST)
        bg = Image.new('RGBA', crop.size, (238,237,229,255))
        bg.alpha_composite(crop)
        bg.convert('RGB').save(OUT/name)
    layout = {'direction':'E', 'source_cell_index':2, 'native_cell':192,
              'crop_native':CROP, 'reference_display_size':[1024,1024],
              'method':'Fixed square crop; only whole-canvas uniform resizing allowed.'}
    (OUT/'layout.json').write_text(json.dumps(layout,indent=2)+'\n',encoding='utf-8',newline='\n')

if __name__ == '__main__':
    prepare()
    if (OUT/'source-attempt3.png').exists():
        review()
