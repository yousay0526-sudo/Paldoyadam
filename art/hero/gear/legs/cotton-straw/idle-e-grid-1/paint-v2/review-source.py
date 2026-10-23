"""Inspect final raster pixels, separately from the direct-paint definitions."""
from pathlib import Path
import json
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from prepare_leg_idle_e_review import ART,REF,flatten,sha

OUT=ART/'idle-e-grid-1'
PAINT=OUT/'paint-v2'

def band_observations(im):
    # Image-only segmentation. No targets or authoring paths are inputs.
    a=np.array(im.convert('RGBA')).astype(np.int16)
    m=(a[:,:,3]>=128)&(a[:,:,2]>a[:,:,0]+7)&(a[:,:,2]>a[:,:,1]+5)&(a[:,:,0]<100)
    m[:600]=False
    count,labels,stats,centers=cv2.connectedComponentsWithStats(m.astype('uint8'),8)
    observed=[]
    for x,y,w,h,area in stats[1:]:
        if area<10:continue
        observed.append({'bbox_4x':[int(x),int(y),int(x+w-1),int(y+h-1)],'area_4x':int(area),
                         'center_native':[(float(x)+(float(w)-1)/2)/4,(float(y)+(float(h)-1)/2)/4]})
    return sorted(observed,key=lambda q:q['center_native'][1])

def review():
    OUT.mkdir(exist_ok=True)
    la=np.array(Image.open(PAINT/'painted-legs-4x.png').convert('RGBA'))
    body=np.array(Image.open(REF/'idle-8dir-body-nolegs.png').convert('RGBA').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768)))
    occ=np.array(Image.open(REF/'idle-8dir-occluder.png').convert('L').resize((3072,1536),Image.Resampling.NEAREST).crop((1536,0,2304,768)))>0
    body[~occ]=0;la[occ]=0
    master=Image.alpha_composite(Image.fromarray(la),Image.fromarray(body))
    master.save(OUT/'master-e.png');Image.fromarray(la).save(OUT/'visible-legs-e.png')
    guideim=Image.open(REF/'idle-8dir-guide4x.png').convert('RGB').crop((1536,0,2304,768))
    guide=np.array(guideim).astype(np.int16)
    gm=(((guide[:,:,2]>guide[:,:,0]+40)&(guide[:,:,2]>guide[:,:,1]+20)&(guide[:,:,1]>65))|((guide[:,:,0]>guide[:,:,1]+40)&(guide[:,:,1]>guide[:,:,2]+40)&(guide[:,:,0]>170)))&~occ
    roi=np.zeros(occ.shape,bool);roi[560:752,256:512]=True;gm&=roi
    active=la[:,:,3]>=128;ys,xs=np.where(active)
    iou=float((active&gm).sum()/(active|gm).sum())
    opaque=occ&(body[:,:,3]==255)
    changed=int(np.any(np.array(master)[opaque]!=body[opaque],axis=1).sum())
    observed=band_observations(Image.fromarray(la))
    # Only now load the immutable targets for the comparison.
    joints=json.loads((REF/'leg-guides.json').read_text('utf8'))['frames']['idle/2(e)']
    for side,q in zip(('left','right'),observed):
        q['side']=side;q['target_native']=joints[side]['ankle']
        q['error_native_px']=round(float(np.linalg.norm(np.array(q['center_native'])-joints[side]['ankle'])),3)
    report={'baseline_commit':'a1a32de45ca76542edbc2c7e666f637c6c3c2aea','direction':'E','method':'Fresh absolute-grid drawing; final raster compared to unchanged guide. No guide-mask clipping, previous-foot movement, distortion or image sampling. Only original body/occluder used for preservation.',
            'runtime_ready':False,'status':'unapproved_direct_grid_review','visible_silhouette_iou':round(iou,4),
            'lowest_visible_leg_y':float(ys.max()/4),'global_envelope_175_177_pass':bool(175<=ys.max()/4<=177),
            'protected_opaque_pixels_changed':changed,'cell_border_clear':not(active[:8].any() or active[-8:].any() or active[:,:8].any() or active[:,-8:].any()),
            'source_only_band_observations':observed,'ankle_method':'Visible-band RGB connected component bounding-box centers, read from final raster before loading target coordinates. +/-0.25 native px raster/contour uncertainty; partially occluded far band is reported as visible center, not hidden anatomical center.',
            'full_joint_max_error_native_px':None,'per_foot_contact_pass':None,'reference_sha256':{p.name:sha(p) for p in REF.iterdir()}}
    report['ankle_max_error_native_px']=max(q['error_native_px'] for q in observed) if len(observed)==2 else None
    report['gates']={'visible_iou_0_75_pass':iou>=.75,'visible_band_centers_1_5px_pass':len(observed)==2 and report['ankle_max_error_native_px']<=1.5,'global_envelope_175_177_pass':report['global_envelope_175_177_pass'],'overall_idle_approved':False}
    previous=Image.open(ART/'idle-e-revision-1/master-e.png').convert('RGBA')
    rejected=Image.open(ART/'idle-e-revision-2/attempt1-master-e.png').convert('RGBA')
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',20)
    compare=Image.new('RGB',(2048,576),(238,237,229));d=ImageDraw.Draw(compare)
    for i,(label,im) in enumerate([('FIXED GUIDE',guideim),('E revision 1',previous),('E revision 2 / rejected',rejected),('DIRECT GRID / unapproved',master)]):
        tile=flatten(im).crop((256,512,512,768)).resize((512,512),Image.Resampling.NEAREST)
        compare.paste(tile,(i*512,52));d.text((i*512+8,12),label,font=font,fill=(50,45,40))
    compare.save(OUT/'comparison.png')
    alignment=flatten(master);d=ImageDraw.Draw(alignment);d.line((0,704,767,704),fill=(40,155,65),width=2)
    for side,points in joints.items():
        color=(40,120,230) if side=='left' else (230,120,25)
        for name in ('ankle','heel','toe'):
            x,y=np.array(points[name])*4;d.line((x-6,y,x+6,y),fill=color,width=2);d.line((x,y-6,x,y+6),fill=color,width=2)
    alignment.crop((256,512,512,768)).resize((1024,1024),Image.Resampling.NEAREST).save(OUT/'alignment.png')
    for label in ('far-unoccluded','near-unoccluded'):
        im=Image.open(PAINT/(label+'-4x.png')).convert('RGBA')
        flatten(im).crop((320,608,464,720)).resize((1152,896),Image.Resampling.NEAREST).save(OUT/(label+'-inspection.png'))
    if (OUT/'observations.json').exists():
        report['independent_foot_observations']=json.loads((OUT/'observations.json').read_text('utf8'))
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps({k:report[k] for k in ['visible_silhouette_iou','lowest_visible_leg_y','source_only_band_observations','gates']},indent=2))

if __name__=='__main__':review()
