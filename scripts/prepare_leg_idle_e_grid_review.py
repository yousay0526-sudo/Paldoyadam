"""Inspect final raster pixels, separately from the direct-paint definitions."""
from pathlib import Path
import json
import cv2
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from prepare_leg_idle_e_review import ART,REF,flatten,sha

OUT=ART/'idle-e-grid-1'
PAINT=OUT/'paint-v4'

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

def sock_cap_observations(im):
    """Natural white paint lobes aid heel/toe reading; not anatomical joints."""
    a=np.array(im.convert('RGBA')).astype(np.int16)
    mask=(a[:,:,3]>=250)&(a[:,:,0]>=250)&(a[:,:,1]>=248)&(a[:,:,2]>=235)
    count,labels,stats,centers=cv2.connectedComponentsWithStats(mask.astype('uint8'),8)
    found=[]
    for i,(x,y,w,h,area) in enumerate(stats[1:],1):
        if area<5:continue
        found.append({'bbox_4x':[int(x),int(y),int(x+w-1),int(y+h-1)],'area_4x':int(area),
                      'center_native':(centers[i]/4).tolist()})
    found=sorted(found,key=lambda q:q['area_4x'],reverse=True)[:2]
    return sorted(found,key=lambda q:q['center_native'][0])

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
    sensitivity={}
    for threshold in (64,128,192):
        mask=la[:,:,3]>=threshold;ty,tx=np.where(mask)
        sensitivity[str(threshold)]={'visible_silhouette_iou':round(float((mask&gm).sum()/(mask|gm).sum()),4),'lowest_visible_y':float(ty.max()/4)}
    opaque=occ&(body[:,:,3]==255)
    changed=int(np.any(np.array(master)[opaque]!=body[opaque],axis=1).sum())
    observed=band_observations(Image.fromarray(la))
    isolated={};caps={}
    for side,label in [('left','far-unoccluded'),('right','near-unoccluded')]:
        im=Image.open(PAINT/(label+'-4x.png')).convert('RGBA')
        isolated[side]={'bands':band_observations(im),'image':label+'-4x.png'}
        caps[side]=sock_cap_observations(im)
        aa=np.array(im)[:,:,3]>=128;iy,ix=np.where(aa)
        isolated[side]['lowest_painted_y']=float(iy.max()/4)
    raw_observations={'source':'paint-v4 final rendered PNGs only; no authoring coordinates or target points read',
        'composite_visible_bands':observed,'isolated_parts':isolated,'sock_cap_paint_lobes':caps,
        'limits':'White-lobe centroids are source-only aids to reading sock heel/toe areas, not certified anatomical joint centers. Far toe is obscured in the composite; isolated source observations do not prove visible composite landmarks. Source contour/raster uncertainty about +/-0.25px for bands and +/-1px for heel/toe interpretation.'}
    (OUT/'observations.json').write_text(json.dumps(raw_observations,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
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
    report['alpha_threshold_sensitivity']=sensitivity
    report['ankle_max_error_native_px']=max(q['error_native_px'] for q in observed) if len(observed)==2 else None
    report['gates']={'visible_iou_0_75_pass':iou>=.75,'visible_band_centers_1_5px_pass':len(observed)==2 and report['ankle_max_error_native_px']<=1.5,'global_envelope_175_177_pass':report['global_envelope_175_177_pass'],'overall_idle_approved':False}
    report['isolated_part_observations']=isolated
    cap_comparisons=[]
    for side in ('left','right'):
        for landmark,q in zip(('heel','toe'),caps[side]):
            target=joints[side][landmark]
            cap_comparisons.append({'side':side,'landmark':landmark,**q,'target_native':target,
                'proxy_error_native_px':round(float(np.linalg.norm(np.array(q['center_native'])-target)),3)})
        for q in isolated[side]['bands']:
            q['target_native']=joints[side]['ankle']
            q['error_native_px']=round(float(np.linalg.norm(np.array(q['center_native'])-joints[side]['ankle'])),3)
    report['sock_cap_centerline_aids']=cap_comparisons
    report['contact_note']='Near/global outsole envelope only. Far painted outsole is higher in projection; contact=true to projected heel/toe/outsole relationship is undefined. Per-foot physical contact remains null.'
    report['art_review']={'isolated_two_transverse_straps_each_foot':True,'near_composite_two_straps_readable':True,
        'far_composite_straps_fully_readable':False,'heel_loops_separate_from_transverse_bands':True,
        'dangling_indigo_knots':False,'style_approval':'pending; directly painted paths are simpler/flatter than generated candidates; higher sock toe and heel construction require user art review',
        'runtime_ready':False}
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
    marks=Image.new('RGB',(1536,672),(238,237,229));md=ImageDraw.Draw(marks)
    for i,(side,label) in enumerate([('left','far-unoccluded'),('right','near-unoccluded')]):
        im=flatten(Image.open(PAINT/(label+'-4x.png')));dd=ImageDraw.Draw(im)
        for q in isolated[side]['bands']+caps[side]:
            x,y=np.array(q['center_native'])*4;dd.ellipse((x-3,y-3,x+3,y+3),outline=(0,175,160),width=2)
        marks.paste(im.crop((320,608,464,720)).resize((720,560),Image.Resampling.NEAREST),(i*768+24,64))
        md.text((i*768+24,16),label+' / raster observations',font=font,fill=(50,45,40))
    marks.save(OUT/'observed-isolated.png')
    (OUT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps({k:report[k] for k in ['visible_silhouette_iou','lowest_visible_leg_y','source_only_band_observations','gates']},indent=2))

if __name__=='__main__':review()
