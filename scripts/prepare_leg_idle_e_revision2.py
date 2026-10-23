"""E-only revision 2 review. Construction annotations are never final artwork."""
from pathlib import Path
import json
import numpy as np
from PIL import Image, ImageDraw, ImageFont
from prepare_leg_idle_e_review import ART, REF, extract, flatten, sha

OUT=ART/'idle-e-revision-2'
CROP=(64,128,128,192)

def band_components(source):
    """Source RGB only; no guide or target points used."""
    a=np.array(source.convert('RGB')).astype(np.int16);h,w=a.shape[:2]
    active=(a[:,:,2]>a[:,:,0]+5)&(a[:,:,2]>a[:,:,1]+1)&(a[:,:,0]<95)&(a[:,:,2]<155)
    active[:int(h*.35)]=False
    visited=np.zeros(active.shape,bool);found=[]
    for sy,sx in zip(*np.where(active)):
        if visited[sy,sx]:continue
        todo=[(int(sy),int(sx))];visited[sy,sx]=True;points=[]
        while todo:
            y,x=todo.pop();points.append((y,x))
            for ny,nx in ((y-1,x),(y+1,x),(y,x-1),(y,x+1)):
                if 0<=ny<h and 0<=nx<w and active[ny,nx] and not visited[ny,nx]:
                    visited[ny,nx]=True;todo.append((ny,nx))
        if len(points)>150:
            yy,xx=np.array(points).T;box=[int(xx.min()),int(yy.min()),int(xx.max()),int(yy.max())]
            found.append({'area':len(points),'bbox_source':box,'center_source':[(box[0]+box[2])/2,(box[1]+box[3])/2]})
    return sorted(found,key=lambda c:-c['area'])[:4]

def review():
    baseline=np.array(Image.open(ART/'idle-e-revision-1/visible-legs-e.png').convert('RGBA'))
    body=np.array(Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768)))
    occ=np.array(Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768)))>0
    body[~occ]=0
    guide=np.array(Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((1536,0,2304,768))).astype(np.int16)
    gm=(((guide[:,:,2]>guide[:,:,0]+40)&(guide[:,:,2]>guide[:,:,1]+20)&(guide[:,:,1]>65))|((guide[:,:,0]>guide[:,:,1]+40)&(guide[:,:,1]>guide[:,:,2]+40)&(guide[:,:,0]>170)))&~occ
    roi=np.zeros(occ.shape,bool);roi[560:752,256:512]=True;gm&=roi
    results=[];masters=[];detections=[]
    for n,crop in [(1,CROP),(2,(80,148,116,184))]:
        source=Image.open(OUT/f'source-attempt{n}.png').convert('RGBA');assert source.width==source.height
        x0,y0,x1,y1=[v*4 for v in crop]
        la=baseline.copy();la[y0:y1,x0:x1]=np.array(extract(source).resize((x1-x0,y1-y0),Image.Resampling.LANCZOS))
        la[occ]=0
        master=Image.alpha_composite(Image.fromarray(la),Image.fromarray(body)); masters.append(master)
        master.save(OUT/f'attempt{n}-master-e.png');Image.fromarray(la).save(OUT/f'attempt{n}-visible-legs-e.png')
        a=la[:,:,3]>=128;yy,xx=np.where(a)
        protected=occ&(body[:,:,3]==255)
        changed=int(np.any(np.array(master)[protected]!=body[protected],axis=1).sum())
        r={'attempt':n,'source_size':list(source.size),'source_sha256':sha(OUT/f'source-attempt{n}.png'),'fixed_crop_native':crop,
           'visible_silhouette_iou':round(int((a&gm).sum())/int((a|gm).sum()),4),
           'lowest_visible_leg_y':float(yy.max()/4),'global_envelope_175_177_pass':bool(175<=yy.max()/4<=177),
           'protected_opaque_pixels_changed':changed,'cell_border_clear':not(a[:8].any() or a[-8:].any() or a[:,:8].any() or a[:,-8:].any())}
        results.append(r)
        components=band_components(source);detections.append({'attempt':n,'components':components})
        marked=source.copy();d=ImageDraw.Draw(marked)
        for c in components:
            d.rectangle(c['bbox_source'],outline=(0,200,160),width=3)
        marked.save(OUT/f'attempt{n}-band-detection.png')
    previous=Image.open(ART/'idle-e-revision-1/master-e.png').convert('RGBA')
    panel=Image.new('RGB',(1536,576),(238,237,229));d=ImageDraw.Draw(panel);font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',21)
    for i,(name,im) in enumerate([('BEFORE / E revision 1',previous),('E revision 2 / attempt 1',masters[0]),('E revision 2 / attempt 2',masters[1])]):
        tile=flatten(im).crop((256,512,512,768)).resize((512,512),Image.Resampling.NEAREST)
        panel.paste(tile,(i*512,52));d.text((i*512+8,12),name,font=font,fill=(50,45,40))
    panel.save(OUT/'comparison.png')
    joints=json.loads((REF/'leg-guides.json').read_text('utf8'))['frames']['idle/2(e)']
    for n,im in enumerate(masters,1):
        overlay=flatten(im);d=ImageDraw.Draw(overlay);d.line((0,704,767,704),fill=(40,155,70),width=2)
        for side,pts in joints.items():
            color=(40,120,230) if side=='left' else (230,120,25)
            for key in ('ankle','heel','toe'):
                x,y=np.array(pts[key])*4
                d.line((x-6,y,x+6,y),fill=color,width=2);d.line((x,y-6,x,y+6),fill=color,width=2)
        overlay.crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST).save(OUT/f'attempt{n}-alignment.png')
    report={'baseline_commit':'5a90dceb5472a14505b0e7e7ff7b6b949cb1cd94','direction':'E','status':'unapproved_alignment_review',
        'runtime_ready':False,'generation_count':2,'selected_attempt':None,'attempts':results,'source_band_components':detections,
        'reference_sha256':{p.name:sha(p) for p in REF.iterdir()},
        'method':'Whole square source uniformly resampled into each predeclared fixed crop. Existing revision1 art outside crop retained; no foot fitting, translation, distortion, mirroring or guide clipping. Construction rectangles are generator input only and are never composited into candidates or used as observed points.',
        'joint_validation_complete':False,'per_foot_contact_pass':None,
        'not_verified':['Far-foot contact projection semantics','Game/SDL/mobile rendering']}
    if (OUT/'observations.json').exists():
        obs=json.loads((OUT/'observations.json').read_text('utf8'));rows=[]
        for item in obs['landmarks']:
            row=dict(item);xy=item.get('source_xy')
            if xy is not None:
                crop=results[item['attempt']-1]['fixed_crop_native'];scale=(crop[2]-crop[0])/1254
                native=[crop[0]+xy[0]*scale,crop[1]+xy[1]*scale];target=joints[item['side']][item['landmark']]
                row.update(observed_native=native,target_native=target,error_native_px=round(float(np.linalg.norm(np.array(native)-target)),3))
            rows.append(row)
        report['independent_observations']={'method':obs['method'],'landmarks':rows,'full_joint_max_error_native_px':None}
        report['visual_review']=obs['visual_review']
        report['selected_attempt']=obs['selected_attempt']
        report['promotion_status']=obs['promotion_status']
        for n in (1,2):
            report['attempts'][n-1]['ankle_max_error_native_px']=max(q['error_native_px'] for q in rows if q['attempt']==n and q['landmark']=='ankle')
        report['selected_observable_centerline_proxy_max_error_native_px']=max(q['error_native_px'] for q in rows if q['attempt']==1)
        selected=results[obs['selected_attempt']-1]
        report['gates']={'silhouette_iou_0_75_pass':selected['visible_silhouette_iou']>=.75,'ankles_1_5px_pass':selected['ankle_max_error_native_px']<=1.5,'global_envelope_175_177_pass':selected['global_envelope_175_177_pass'],
            'full_joint_validation_complete':False,'per_foot_contact_pass':None,'overall_idle_approved':False}
        selected_rows={(q['side'],q['landmark']):q for q in rows if q['attempt']==1}
        report['shoe_centerline_spans_native_px']={side:round(selected_rows[side,'toe']['observed_native'][0]-selected_rows[side,'heel']['observed_native'][0],3) for side in ('left','right')}
        annotated=flatten(masters[0]);d=ImageDraw.Draw(annotated)
        for row in rows:
            if row['attempt']!=1 or 'observed_native' not in row:continue
            x,y=np.array(row['observed_native'])*4;d.ellipse((x-4,y-4,x+4,y+4),outline=(0,180,160),width=2)
        annotated.crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST).save(OUT/'observed-landmarks.png')
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps({'attempts':results,'bands':detections},indent=2))

def prepare():
    OUT.mkdir(exist_ok=True)
    guide=Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((1792,512,2048,768)).resize((1024,1024),Image.Resampling.NEAREST)
    guide.save(OUT/'guide-input.png')
    before=Image.open(ART/'idle-e-revision-1/master-e.png').convert('RGBA').crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST)
    bg=Image.new('RGBA',before.size,(238,237,229,255));bg.alpha_composite(before)
    bg.convert('RGB').save(OUT/'before-input.png')
    joints=json.loads((REF/'leg-guides.json').read_text('utf8'))['frames']['idle/2(e)']
    annotated=guide.copy(); d=ImageDraw.Draw(annotated)
    marks={}
    for side,width in [('left',96),('right',112)]:
        x,y=joints[side]['ankle'];x=(x-64)*16;y=(y-128)*16
        rect=[round(x-width/2),round(y-16),round(x+width/2),round(y+16)]
        d.rectangle(rect,fill=(29,39,66),outline=(10,18,40),width=2)
        marks[side]={'target_center_crop_1024':[x,y],'annotation_box':rect}
    annotated.save(OUT/'construction-reference.png')
    close_crop=(80,148,116,184)
    close=Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((1536+320,592,1536+464,736)).resize((1024,1024),Image.Resampling.NEAREST)
    close.save(OUT/'close-guide-input.png')
    cd=ImageDraw.Draw(close)
    for side,width in [('left',144),('right',176)]:
        x,y=joints[side]['ankle'];x=(x-80)*1024/36;y=(y-148)*1024/36
        cd.rectangle((round(x-width/2),round(y-23),round(x+width/2),round(y+23)),fill=(29,39,66),outline=(10,18,40),width=2)
    close.save(OUT/'close-construction-reference.png')
    (OUT/'layout.json').write_text(json.dumps({'direction':'E','crop_native':CROP,'reference_pixels':[1024,1024],
        'construction_reference':'Diagnostic ankle-band placement rectangles over a copy of the immutable guide, not candidate art. Original reference/ files unchanged. Never used for measured artwork or to clip/warp source.',
        'annotations':marks,'attempt2_crop_native':close_crop,
        'attempt2_method':'Second fixed 36px square crop selected before generation to emphasize ankle and shoe geometry. Original painted trousers above this crop retained; no per-foot placement or fitting.'},indent=2)+'\n',encoding='utf8',newline='\n')

if __name__=='__main__':
    prepare()
    if (OUT/'source-attempt2.png').exists():review()
