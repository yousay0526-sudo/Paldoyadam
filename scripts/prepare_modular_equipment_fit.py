"""Second idle equipment fit: real palm annotations and pose-specific hilt depth.

Uses unchanged v1 artwork. Writes only idle-fit-v2, never v1 or game assets.
"""
from copy import deepcopy
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw

import prepare_modular_equipment_samples as lib

ROOT = lib.ROOT
SOURCE = ROOT / "art/hero/gear/modular-samples/idle-v1"
OUT = ROOT / "art/hero/gear/modular-samples/idle-fit-v2"
CELL, SCALE, NAMES = lib.CELL, lib.SCALE, lib.NAMES
POSES = deepcopy(lib.POSES)

# Finger/knuckle boundaries annotated on the original 192px sprite; unlike v1,
# these do not deliberately include the original brass guard below the palm.
HANDS = {
    "N": [[126,112],[129,112],[132,114],[133,115],[132,117],[128,116],[126,114]],
    "NE": [[118,123],[121,123],[124,125],[125,127],[123,129],[120,128],[118,126]],
    "E": [[92,119],[96,120],[99,121],[102,120],[103,122],[100,125],[96,124],[93,122]],
    "SE": [[71,113],[75,112],[77,114],[76,116],[74,118],[74,120],[71,121],[69,119],[72,116]],
    "S": [[63,105],[66,105],[67,107],[67,110],[65,112],[63,111],[62,108]],
    "SW": [[68,91],[71,91],[73,94],[73,97],[71,99],[68,97],[67,94]],
    "W": [],  # The anatomical right palm is behind the torso, not observed.
    "NW": [[120,98],[123,98],[126,100],[126,103],[123,104],[120,102]],
}
HEADS = {
    "N": [[85,46],[108,46],[108,55],[97,59],[86,55]],
    "NE": [[93,46],[106,44],[111,48],[108,56],[99,59],[89,53]],
    "E": [[102,44],[109,43],[112,47],[107,55],[100,55],[96,50]],
    "SE": [[93,45],[105,44],[109,47],[106,53],[98,58],[90,52]],
    "S": [[86,46],[106,46],[108,52],[102,58],[96,60],[86,54]],
    "SW": [[85,45],[97,44],[102,48],[99,54],[90,58],[82,52]],
    "W": [[84,44],[93,44],[100,50],[96,55],[87,55],[81,48]],
    "NW": [[88,46],[102,44],[109,50],[108,56],[97,60],[87,54]],
}
OLD_GUARDS = {
    "N": [132,116,140,124], "NE": [124,125,135,134],
    "E": [103,121,111,130], "SE": [72,121,81,130],
    "S": [55,110,64,119], "SW": [60,97,68,106],
    "W": [73,93,81,102], "NW": [126,100,134,109],
}


def empty_mask():
    return Image.new("L", (CELL,CELL))


def body_selection(points):
    if not points:
        return empty_mask()
    small = lib.polygon(points).resize((192,192),Image.Resampling.NEAREST)
    return small.resize((CELL,CELL),Image.Resampling.NEAREST)


def forward_blade_selector(pose):
    """Only the outward blade/guard can be in front; hilt stays behind cuff.

    This partitions the untouched weapon artwork. It does not shrink, repaint
    or discard a hilt or infer a hidden hand on the opposite side.
    """
    if pose["depth"] == "rear":
        return empty_mask()
    p=np.array(pose["grip"],dtype=float)*SCALE
    direction=np.array(pose["tip"],dtype=float)*SCALE-p
    direction/=np.linalg.norm(direction)
    yy,xx=np.mgrid[:CELL,:CELL]
    distance=(xx-p[0])*direction[0]+(yy-p[1])*direction[1]
    return Image.fromarray(np.where(distance>=2.5*SCALE,255,0).astype(np.uint8))


def original_removal(name, hand):
    removal_pose=lib.POSES[name]
    if name=="W":
        # Original W blade runs from (74.5,100.5) to (31.5,130.5).
        # The v1 equipment mount used (76,94)->(16,130); that line passes
        # above the old blade and must not be reused as its removal guide.
        removal_pose={**removal_pose,"grip":[74.5,100.5],"tip":[31.5,130.5]}
    selection=lib.old_weapon_selection(removal_pose)
    if name=="W":
        # The lower edge curves below the center-line trapezoid; include its
        # two-pixel lower margin, rather than leaving a silver edge behind.
        selection=ImageChops.lighter(selection,ImageChops.offset(selection,0,2*SCALE))
        # This rectangle is outside the original torso and contains only the
        # old external blade/background. Its weak antialias must also go.
        external=empty_mask()
        ImageDraw.Draw(external).rectangle((28*SCALE,97*SCALE,73*SCALE-1,136*SCALE-1),fill=255)
        selection=ImageChops.lighter(selection,external)
    x0,y0,x1,y1=OLD_GUARDS[name]
    guard=lib.polygon([[x0,y0],[x1,y0],[x1,y1],[x0,y1]])
    selection=ImageChops.lighter(selection,guard)
    return ImageChops.subtract(selection,hand)


def microscope(before,after,filename,part):
    canvas=Image.new("RGB",(1536,752),(239,237,227));d=ImageDraw.Draw(canvas)
    title="손·자루 접합 전후" if part=="hand" else "모자 착용 위치 전후"
    d.text((24,12),title+" · 각 칸 왼쪽 v1 / 오른쪽 v2",font=lib.font(28),fill=(28,42,46))
    for i,name in enumerate(NAMES):
        x=i%4*384;y=64+i//4*338
        d.text((x+14,y+4),name,font=lib.font(22),fill=(37,61,66))
        if part=="hand":
            c=np.array(POSES[name]["grip"]);box=[round(c[0]-13),round(c[1]-13),round(c[0]+13),round(c[1]+13)]
        else:
            box=[73,16,119,62]
        box=tuple(v*SCALE for v in box)
        for j,im in enumerate([before[i],after[i]]):
            tile=im.crop(box).resize((184,184),Image.Resampling.NEAREST)
            canvas.paste(tile,(x+j*192+4,y+40),tile)
        d.text((x+15,y+240),"이전",font=lib.font(18),fill=(65,73,73))
        d.text((x+207,y+240),"보정",font=lib.font(18),fill=(65,73,73))
    canvas.save(OUT/filename)


def w_blade_comparison(after,body):
    folder=OUT/"w-blade-fix"
    if not (folder/"before-w.png").exists():
        return
    before=Image.open(folder/"before-w.png").convert("RGBA")
    after.save(folder/"after-w.png")
    canvas=Image.new("RGB",(1500,500),(239,237,227));d=ImageDraw.Draw(canvas)
    d.text((24,18),"W 방향 · 기존 칼날 잔여 제거",font=lib.font(31),fill=(28,42,46))
    d.text((24,61),"칼을 뺀 몸체에 남아 있던 원래 칼날을 제거하고, 새 칼 한 자루만 조합",font=lib.font(21),fill=(65,73,73))
    for i,(title,im) in enumerate([("수정 전 · 두 칼날 겹침",before),("기존 칼 제거 후 몸체",body),("수정 후 · 새 칼 한 자루",after)]):
        d.text((i*500+18,119),title,font=lib.font(24),fill=(37,61,66))
        roi=im.crop((12*SCALE,88*SCALE,88*SCALE,142*SCALE)).resize((470,334),Image.Resampling.NEAREST)
        canvas.paste(roi,(i*500+15,158),roi)
    canvas.save(folder/"comparison.png")


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    lib.OUT=OUT  # Reuse only review helpers; legacy main() is never invoked.
    obs=json.loads((OUT/"observations.json").read_text(encoding="utf-8"))
    for name,p in POSES.items():
        p["grip"]=obs["palms"][name] or [78.5,94.0]
        p["source_grip"]=obs["weapon_wrap_annotations"][name]
        p["hand"]=HANDS[name];p["head"]=HEADS[name]
        p["hat_width"]=42
        p["hat_top"]=21 if name in ["SE","S","SW"] else 20
        p["head_x"]=97 if name=="E" else 95 if name in ["SW","W"] else 96
        p["palm_observation"]=obs["palms"][name]
        p["grip_inferred"]=name=="W"
    base=Image.open(ROOT/"assets/warrior-idle-8dir-v1.png").convert("RGBA")
    underpaint=lib.uniform_resize(Image.open(SOURCE/"weapon-underpaint-generated-source.png").convert("RGBA"),base.size)
    hat_source=Image.open(SOURCE/"paeraengi-generated-source.png").convert("RGBA")
    weapon_source=Image.open(SOURCE/"hwando-generated-source.png").convert("RGBA")
    old_preview=Image.open(SOURCE/"layers/preview-equipped-4x.png").convert("RGBA")
    layers={k:[] for k in ["base-without-weapon","body-core","right-hand","head-foreground","headgear-full","headgear-rear","headgear-front","weapon-full","weapon-rear","weapon-front","preview-equipped"]}
    masks={k:[] for k in ["body-occluder","hand-occluder","head-occluder","right-hand-selector","head-foreground-selector","old-weapon-remove","headgear-front-selector","weapon-front-selector"]}
    old_tiles=[];mask_reviews=[];metrics=[];registrations=[]
    for i,name in enumerate(NAMES):
        p=POSES[name];cell=(i%4*192,i//4*192,(i%4+1)*192,(i//4+1)*192)
        orig=base.crop(cell);orig4=orig.resize((CELL,CELL),Image.Resampling.NEAREST)
        hand_selector=body_selection(p["hand"])
        remove=original_removal(name,hand_selector).resize((192,192),Image.Resampling.NEAREST)
        # Removal and hand selection share the original pixel grid exactly.
        hand_small=hand_selector.resize((192,192),Image.Resampling.NEAREST)
        remove=ImageChops.subtract(remove,hand_small)
        clean=Image.composite(underpaint.crop(cell),orig,remove)
        old_blade_probe=None
        if name=="W":
            # There is no concealed clothing in this external blade region;
            # use actual transparency, not generated background antialias.
            clean.paste((0,0,0,0),(28,97,73,136))
            # Original-alpha probe outside the torso, independently read from
            # the source PNG, not from the new weapon or removal coordinates.
            # This failed on the parent commit: 289 of 305 probe pixels remained.
            probe=np.asarray(orig)[97:136,28:73,3]>16
            remaining=int((probe&(np.asarray(clean)[97:136,28:73,3]>16)).sum())
            any_alpha=int((np.asarray(clean)[97:136,28:73,3]>0).sum())
            old_blade_probe=dict(roi_base_px=[28,97,73,136],alpha_threshold=16,original_blade_pixels=int(probe.sum()),remaining_in_unarmed_body=remaining,remaining_any_alpha_in_external_roi=any_alpha)
            assert remaining==0,("W original blade remains in body",remaining)
            assert any_alpha==0,("W old blade antialias remains",any_alpha)
        body=clean.resize((CELL,CELL),Image.Resampling.NEAREST)
        head_selector=body_selection(p["head"])
        core=lib.clip(body,ImageChops.invert(ImageChops.lighter(hand_selector,head_selector)))
        hand=lib.clip(orig4,hand_selector);head=lib.clip(body,head_selector)
        rebuilt=core.copy();rebuilt.alpha_composite(hand);rebuilt.alpha_composite(head)
        aa=np.array(body);bb=np.array(rebuilt);visible=aa[:,:,3]>0
        assert np.array_equal(aa[visible],bb[visible]),name
        crop_box=[round(i%4*hat_source.width/4),round(i//4*hat_source.height/2),round((i%4+1)*hat_source.width/4),round((i//4+1)*hat_source.height/2)]
        hat,bbox_hat=lib.largest_item(hat_source,crop_box)
        width=p["hat_width"]*SCALE;height=round(hat.height*width/hat.width)
        hat_full=Image.new("RGBA",(CELL,CELL))
        hat_full.alpha_composite(lib.uniform_resize(hat,(width,height)),(round(p["head_x"]*SCALE-width/2),p["hat_top"]*SCALE))
        left,right=p["brim_y"];shift=p["hat_top"]-lib.POSES[name]["hat_top"]
        hat_front_selector=lib.polygon([[0,left+shift],[192,right+shift],[192,192],[0,192]])
        crown=lib.polygon([[p["head_x"]-10,14],[p["head_x"]+10,14],[p["head_x"]+12,46],[p["head_x"]-12,46]])
        hat_front_selector=ImageChops.lighter(hat_front_selector,crown)
        hat_rear,hat_front=lib.split(hat_full,hat_front_selector)
        item,bbox_weapon=lib.largest_item(weapon_source,p["window"])
        weapon,transform=lib.mount_weapon(item,bbox_weapon,p)
        weapon_selector=forward_blade_selector(p)
        weapon_rear,weapon_front=lib.split(weapon,weapon_selector)
        final=Image.new("RGBA",(CELL,CELL))
        for layer in [hat_rear,weapon_rear,core,weapon_front,hand,hat_front,head]:
            final.alpha_composite(layer)
        vals={"base-without-weapon":body,"body-core":core,"right-hand":hand,"head-foreground":head,"headgear-full":hat_full,"headgear-rear":hat_rear,"headgear-front":hat_front,"weapon-full":weapon,"weapon-rear":weapon_rear,"weapon-front":weapon_front,"preview-equipped":final}
        mask_vals={"body-occluder":body.getchannel("A"),"hand-occluder":hand.getchannel("A"),"head-occluder":head.getchannel("A"),"right-hand-selector":hand_selector,"head-foreground-selector":head_selector,"old-weapon-remove":remove.resize((CELL,CELL),Image.Resampling.NEAREST),"headgear-front-selector":hat_front_selector,"weapon-front-selector":weapon_selector}
        for k,v in vals.items():layers[k].append(v)
        for k,v in mask_vals.items():masks[k].append(v)
        old_tiles.append(old_preview.crop((i%4*CELL,i//4*CELL,(i%4+1)*CELL,(i//4+1)*CELL)))
        partitions={}
        for label,full,rear,front in [("headgear",hat_full,hat_rear,hat_front),("weapon",weapon,weapon_rear,weapon_front)]:
            a=np.asarray(full.getchannel("A"),dtype=int);r=np.asarray(rear.getchannel("A"),dtype=int);f=np.asarray(front.getchannel("A"),dtype=int)
            assert np.array_equal(a,r+f) and not np.any((r>0)&(f>0)),(name,label)
            b=full.getbbox();margin=min(b[0],b[1],CELL-b[2],CELL-b[3])/SCALE
            assert margin>=4,(name,label,margin)
            partitions[label]=dict(exact_alpha_partition=True,overlap_pixels=0,margin_base_px=margin)
        hand_opaque=np.asarray(hand.getchannel("A"))==255
        hand_changes=int(np.any(np.array(final)!=np.array(orig4),axis=2)[hand_opaque].sum())
        assert hand_changes==0,name
        original_changes=np.any(np.array(clean)!=np.array(orig),axis=2)
        outside=int((original_changes&(np.array(remove)==0)).sum());assert outside==0,name
        # Actual navy wrap pixels in the registered PNG must pass behind the
        # observed original fingers. These are read from RGB, not target points.
        pixels=np.asarray(weapon).astype(int)
        navy=(pixels[:,:,2]>pixels[:,:,0]+6)&(pixels[:,:,2]>pixels[:,:,1]+3)&(pixels[:,:,2]<180)&(pixels[:,:,3]>=96)
        grip_overlap=float((navy&(np.array(hand_selector)>0)).sum())/(SCALE*SCALE)
        if name!="W":assert grip_overlap>=0.125,(name,grip_overlap)
        guard_box=OLD_GUARDS[name]
        old_hand=body_selection(lib.POSES[name]["hand"])
        guard=lib.polygon([[guard_box[0],guard_box[1]],[guard_box[2],guard_box[1]],[guard_box[2],guard_box[3]],[guard_box[0],guard_box[3]]])
        old_guard_kept=float(((np.array(old_hand)>0)&(np.array(guard)>0)).sum())/(SCALE*SCALE)
        new_guard_kept=float(((np.array(hand_selector)>0)&(np.array(guard)>0)).sum())/(SCALE*SCALE)
        metrics.append(dict(direction=name,partition=partitions,body_reassembly_visible_rgba_exact=True,opaque_original_finger_changes=hand_changes,base_changes_outside_removal=outside,navy_wrap_under_finger_area_base_px2=grip_overlap if name!="W" else None,palm_visible=name!="W",guard_region_retained_by_hand_selector_base_px2=dict(v1=old_guard_kept,v2=new_guard_kept),visual_approval=False))
        if old_blade_probe is not None:
            metrics[-1]["old_blade_removal_probe"]=old_blade_probe
        registrations.append(dict(direction=name,**p,source_hat_bbox=bbox_hat,source_weapon_bbox=bbox_weapon,weapon_transform=transform))
        tinted=body.copy()
        for a,color in [(body.getchannel("A"),(63,131,191)),(head.getchannel("A"),(193,81,173)),(hand.getchannel("A"),(245,170,54))]:
            overlay=Image.new("RGBA",(CELL,CELL),(*color,0));overlay.putalpha(a.point(lambda v:round(v*.48)));tinted.alpha_composite(overlay)
        rear_visible=lib.clip(weapon_rear,ImageChops.invert(body.getchannel("A")))
        front_visible=lib.clip(weapon_front,ImageChops.invert(hand.getchannel("A")))
        tinted.alpha_composite(rear_visible);tinted.alpha_composite(front_visible);mask_reviews.append(tinted)
    for group,folder in [(layers,OUT/"layers"),(masks,OUT/"masks")]:
        folder.mkdir(exist_ok=True)
        for key,images in group.items():lib.stitch(images).save(folder/(key+"-4x.png"))
    lib.review_grid(layers["preview-equipped"],"분리 장비 2차 · 착용 위치와 손 가림","기존 원화 유지 | 실제 손 위치 재판독 · 자루는 손목 뒤 · 칼날은 방향별 앞/뒤","preview-equipped-8dir.png")
    lib.review_grid(mask_reviews,"방향별 가림 보정","파랑: 몸체 / 자주: 얼굴·뒷머리 / 주황: 보이는 오른손 | W 손은 몸 뒤에 가림","preview-masks-8dir.png")
    microscope(old_tiles,layers["preview-equipped"],"comparison-hands.png","hand")
    microscope(old_tiles,layers["preview-equipped"],"comparison-heads.png","head")
    w_blade_comparison(layers["preview-equipped"][6],layers["base-without-weapon"][6])
    lib.json_write(OUT/"registration.json",dict(schema_version=2,pose="idle",cell_px=192,art_scale=4,atlas_grid=[4,2],direction_order=NAMES,foot_root=[96,176],observation_file="observations.json",warning="Mounting targets are not independent anatomical alignment certification. W palm is hidden, not observed.",poses=registrations))
    lib.json_write(OUT/"layer-contract.json",dict(schema_version=2,status="review_only",cell_px=768,atlas_px=[3072,1536],directions=NAMES,draw_order=["headgear-rear","weapon-rear","body-core","weapon-front","right-hand","headgear-front","head-foreground"],weapon_depth="The palm-inward grip/pommel region is rear. In SE/S/SW, only points at least 2.5 base pixels outward from the registered palm along the blade axis can be front. Other directions keep the entire weapon rear.",body_parts="Disjoint RGBA core, original visible fingers, and original head foreground. Do not additionally render the complete body.",weapon_hand="anatomical_right",horizontal_reflection=False,occlusion_baked_into_full_items=False,scope="Idle fit only; no walking, attack or runtime integration."))
    lib.json_write(OUT/"validation.json",dict(status="static_checks_passed_visual_approval_pending",checks=metrics,artwork_repainted=False,new_weapon_edit_completed=False,source_directory="../idle-v1",source_sha256={str(p.relative_to(ROOT)):lib.sha(p) for p in [ROOT/"assets/warrior-idle-8dir-v1.png",SOURCE/"paeraengi-generated-source.png",SOURCE/"hwando-generated-source.png",SOURCE/"weapon-underpaint-generated-source.png"]},game_execution_tested=False,game_connected=False,anatomical_alignment_certified=False,limitations=["Requested hilt/guard repaint was blocked by image_gen; no new artwork was generated.","Original guard proportions remain in the source; this revision fixes mounting, old-guard remnants and occlusion only.","W palm is hidden and has no independently observed center.","Weapon-removal underpaint remains a draft reconstruction of concealed clothing."]))
    print("v2 prepared: eight directions, seven visible palms with actual navy-wrap overlap; hidden W palm kept unobserved.")
    print("Original-guard selector area totals (192px squared):",sum(m["guard_region_retained_by_hand_selector_base_px2"]["v1"] for m in metrics),"->",sum(m["guard_region_retained_by_hand_selector_base_px2"]["v2"] for m in metrics))


if __name__=="__main__":main()
