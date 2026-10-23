"""Prepare review-only independent head/weapon art and pose-specific occlusion masks.

Artwork is generated separately; this script registers, splits and composites it.
It never writes assets/, src/, old leg guides, or the existing art sources.
"""
from collections import deque
import hashlib
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "art/hero/gear/modular-samples/idle-v1"
SCALE = 4
CELL = 192 * SCALE
NAMES = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]

# Coordinates were annotated on the actual source illustrations, not copied from
# any leg guide. Target annotations are mounting instructions, not QA observations.
POSES = {
    "N": dict(grip=[131, 112], tip=[169, 149], source_grip=[155, 130],
              source_tip=[434, 366], window=[50, 40, 460, 420], depth="rear",
              head_x=96, hat_width=43, hat_top=18, brim_y=[39, 39],
              hand=[[125,106],[134,107],[137,113],[133,119],[126,117]],
              head=[[85,43],[108,43],[108,55],[97,59],[86,55]]),
    "NE": dict(grip=[131, 122], tip=[171, 155], source_grip=[580, 131],
               source_tip=[853, 366], window=[470, 40, 890, 420], depth="rear",
               head_x=96, hat_width=43, hat_top=18, brim_y=[38, 41],
               hand=[[125,116],[133,116],[137,123],[133,128],[126,126]],
               head=[[90,43],[105,41],[111,47],[108,56],[99,59],[89,53]]),
    "E": dict(grip=[101, 121], tip=[158, 158], source_grip=[1025, 132],
              source_tip=[1338, 364], window=[900, 40, 1380, 420], depth="rear",
              head_x=96, hat_width=43, hat_top=18, brim_y=[33, 43],
              hand=[[94,116],[102,115],[107,121],[103,127],[96,125]],
              head=[[98,40],[108,40],[112,46],[107,55],[100,55],[94,49]]),
    "SE": dict(grip=[75, 117], tip=[117, 171], source_grip=[1477, 129],
               source_tip=[1716, 399], window=[1390, 40, 1774, 430], depth="front",
               head_x=96, hat_width=43, hat_top=18, brim_y=[37, 41],
               hand=[[68,111],[77,110],[81,116],[78,122],[72,123],[67,119]],
               head=[[91,41],[103,40],[109,46],[106,53],[98,58],[90,52]]),
    "S": dict(grip=[61, 113], tip=[25, 165], source_grip=[301, 566],
              source_tip=[55, 821], window=[10, 460, 440, 875], depth="front",
              head_x=96, hat_width=44, hat_top=18, brim_y=[40, 40],
              hand=[[56,106],[64,106],[67,112],[64,119],[58,120],[54,115]],
              head=[[86,42],[105,42],[108,50],[103,56],[96,60],[86,54],[83,48]]),
    "SW": dict(grip=[66, 101], tip=[27, 145], source_grip=[745, 570],
               source_tip=[490, 810], window=[450, 460, 880, 875], depth="front",
               head_x=96, hat_width=43, hat_top=18, brim_y=[41, 37],
               hand=[[61,95],[69,95],[72,101],[69,107],[63,109],[59,104]],
               head=[[85,41],[98,40],[104,47],[100,53],[90,58],[82,52],[80,46]]),
    "W": dict(grip=[76, 94], tip=[16, 130], source_grip=[1202, 572],
              source_tip=[910, 798], window=[890, 460, 1330, 875], depth="rear",
              head_x=96, hat_width=43, hat_top=18, brim_y=[43, 33],
              hand=[[72,89],[79,88],[82,94],[79,100],[73,101],[69,97]],
              head=[[85,40],[96,40],[103,49],[97,55],[87,55],[81,48]]),
    "NW": dict(grip=[130, 101], tip=[153, 138], source_grip=[1517, 557],
               source_tip=[1733, 811], window=[1390, 460, 1774, 880], depth="rear",
               head_x=96, hat_width=43, hat_top=18, brim_y=[41, 38],
               hand=[[125,95],[132,95],[136,101],[132,107],[127,108],[123,103]],
               head=[[86,43],[101,41],[109,49],[108,56],[97,60],[87,54]]),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def json_write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def polygon(points):
    im = Image.new("L", (CELL, CELL))
    ImageDraw.Draw(im).polygon([(round(x*SCALE), round(y*SCALE)) for x,y in points], fill=255)
    return im


def largest_item(im, window):
    """Select the connected illustration, including antialias within 1 source px.

    Generated sources contain alpha 1..4 background noise. Keep original color
    and alpha of selected source pixels; no painting or arbitrary RGB keying.
    Windows surround actual objects, rather than blindly dividing the atlas.
    """
    crop = im.crop(tuple(window)).convert("RGBA")
    a = np.asarray(crop.getchannel("A"))
    binary = a > 4
    visited = np.zeros(binary.shape, dtype=bool)
    largest = []
    height, width = binary.shape
    for yy, xx in zip(*np.nonzero(binary)):
        if visited[yy, xx]:
            continue
        q = deque([(int(xx), int(yy))])
        visited[yy, xx] = True
        component = []
        while q:
            x,y = q.popleft()
            component.append((x,y))
            for dy in (-1,0,1):
                for dx in (-1,0,1):
                    nx,ny = x+dx,y+dy
                    if 0 <= nx < width and 0 <= ny < height and binary[ny,nx] and not visited[ny,nx]:
                        visited[ny,nx] = True
                        q.append((nx,ny))
        if len(component) > len(largest):
            largest = component
    support = np.zeros(binary.shape, dtype=np.uint8)
    for x,y in largest:
        support[y,x] = 255
    support = Image.fromarray(support).filter(ImageFilter.MaxFilter(3))
    aa = np.where((np.asarray(support)>0) & (a>4), a, 0).astype(np.uint8)
    crop.putalpha(Image.fromarray(aa))
    bbox = crop.getbbox()
    assert bbox, window
    full_bbox = [window[0]+bbox[0],window[1]+bbox[1],window[0]+bbox[2],window[1]+bbox[3]]
    return crop.crop(bbox), full_bbox


def uniform_resize(im, size):
    return im.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")


def clip(im, mask):
    result=np.array(im,copy=True)
    result[:,:,3]=np.asarray(ImageChops.multiply(im.getchannel("A"),mask))
    result[result[:,:,3]==0,:3]=0
    return Image.fromarray(result)


def split(im, selector):
    # Binary selector gives exact, non-overlapping alpha partition, avoiding
    # double compositing antialiased item edges.
    return clip(im, ImageChops.invert(selector)), clip(im, selector)


def mount_weapon(item, bbox, pose):
    p=np.array(pose["source_grip"],dtype=float)-np.array(bbox[:2])
    t=np.array(pose["source_tip"],dtype=float)-np.array(bbox[:2])
    target=np.array(pose["grip"],dtype=float)*SCALE
    target_tip=np.array(pose["tip"],dtype=float)*SCALE
    u=t-p;v=target_tip-target
    scale=float(np.linalg.norm(v)/np.linalg.norm(u))
    angle=math.atan2(v[1],v[0])-math.atan2(u[1],u[0])
    a=scale*np.array([[math.cos(angle),-math.sin(angle)],[math.sin(angle),math.cos(angle)]])
    translation=target-a@p
    inv=np.linalg.inv(a)
    offset=-inv@translation
    coeff=[float(inv[0,0]),float(inv[0,1]),float(offset[0]),float(inv[1,0]),float(inv[1,1]),float(offset[1])]
    mounted=item.convert("RGBa").transform((CELL,CELL),Image.Transform.AFFINE,coeff,Image.Resampling.BICUBIC).convert("RGBA")
    return mounted, dict(scale=scale,rotation_degrees=math.degrees(angle),determinant=float(np.linalg.det(a)),reflected=False)


def old_weapon_selection(pose):
    p=np.array(pose["grip"],dtype=float);t=np.array(pose["tip"],dtype=float)
    v=t-p;u=v/np.linalg.norm(v);normal=np.array([-u[1],u[0]])
    # Narrow underpaint patch: a complete blade and guard strip, with the
    # original palm/cuff protected separately. Original source is untouched.
    p=p-u*5;t=t+u*3
    return polygon([p+normal*5,p-normal*5,t-normal*3,t+normal*3])


def stitch(images):
    mode=images[0].mode
    atlas=Image.new(mode,(CELL*4,CELL*2))
    for i,im in enumerate(images):
        atlas.paste(im,(i%4*CELL,i//4*CELL))
    return atlas


FONT_PATH=ROOT/"assets/fonts/NanumMyeongjo-Bold.ttf"
def font(size):
    return ImageFont.truetype(str(FONT_PATH),size)


def review_grid(items, title, subtitle, filename, overlay=None):
    canvas=Image.new("RGB",(1536,976),(239,237,227))
    d=ImageDraw.Draw(canvas)
    d.text((30,18),title,font=font(34),fill=(28,42,46))
    d.text((30,63),subtitle,font=font(19),fill=(74,83,80))
    for i,im in enumerate(items):
        x=i%4*384;y=104+i//4*432
        d.rounded_rectangle((x+8,y,x+376,y+420),radius=12,fill=(249,248,241),outline=(215,216,203),width=2)
        d.text((x+22,y+12),NAMES[i],font=font(24),fill=(37,61,66))
        tile=im.resize((384,384),Image.Resampling.LANCZOS)
        canvas.paste(tile,(x,y+31),tile)
        if overlay:
            d.text((x+20,y+392),overlay[i],font=font(16),fill=(65,73,73))
    canvas.save(OUT/filename)


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    base=Image.open(ROOT/"assets/warrior-idle-8dir-v1.png").convert("RGBA")
    underpaint=uniform_resize(Image.open(OUT/"weapon-underpaint-generated-source.png").convert("RGBA"),base.size)
    hat_source=Image.open(OUT/"paeraengi-generated-source.png").convert("RGBA")
    weapon_source=Image.open(OUT/"hwando-generated-source.png").convert("RGBA")
    layers={k:[] for k in ["base-original","base-without-weapon","body-core","right-hand","head-foreground","headgear-full","headgear-rear","headgear-front","weapon-full","weapon-rear","weapon-front","preview-equipped"]}
    masks={k:[] for k in ["head-occluder","hand-occluder","body-occluder","old-weapon-remove","headgear-front-selector","weapon-front-selector","head-foreground-selector","right-hand-selector"]}
    review_masks=[];pose_records=[];metrics=[]
    for i,name in enumerate(NAMES):
        p=POSES[name]
        cell_box=(i%4*192,i//4*192,(i%4+1)*192,(i//4+1)*192)
        orig=base.crop(cell_box)
        hand_small=polygon(p["hand"]).resize((192,192),Image.Resampling.NEAREST)
        removal_small=old_weapon_selection(p).resize((192,192),Image.Resampling.NEAREST)
        removal_small=ImageChops.subtract(removal_small,hand_small)
        clean=Image.composite(underpaint.crop(cell_box),orig,removal_small)
        # Restore the original hand and all original pixels outside removal.
        assert np.array_equal(np.asarray(clean)[np.asarray(removal_small)==0],np.asarray(orig)[np.asarray(removal_small)==0])
        orig4=orig.resize((CELL,CELL),Image.Resampling.NEAREST)
        body=clean.resize((CELL,CELL),Image.Resampling.NEAREST)
        opaque=(body.getchannel("A").point(lambda v:255 if v>=128 else 0))
        head_selector=polygon(p["head"])
        hand_selector=hand_small.resize((CELL,CELL),Image.Resampling.NEAREST)
        head=ImageChops.multiply(head_selector,body.getchannel("A"))
        hand=ImageChops.multiply(hand_selector,orig4.getchannel("A"))
        body_core=clip(body,ImageChops.invert(ImageChops.lighter(head_selector,hand_selector)))
        hand_layer=clip(orig4,hand_selector)
        head_layer=clip(body,head_selector)
        # Disjoint body/hand/head layers preserve antialiased edges exactly once.
        reconstructed=body_core.copy()
        reconstructed.alpha_composite(hand_layer)
        reconstructed.alpha_composite(head_layer)
        a_body=np.asarray(body);a_rebuilt=np.asarray(reconstructed)
        assert np.array_equal(a_body[a_body[:,:,3]>0],a_rebuilt[a_body[:,:,3]>0])
        x0=round(i%4*hat_source.width/4);x1=round((i%4+1)*hat_source.width/4)
        y0=round(i//4*hat_source.height/2);y1=round((i//4+1)*hat_source.height/2)
        hat,bbox_hat=largest_item(hat_source,[x0,y0,x1,y1])
        width=p["hat_width"]*SCALE;height=round(hat.height*width/hat.width)
        headgear=Image.new("RGBA",(CELL,CELL))
        headgear.alpha_composite(uniform_resize(hat,(width,height)),(round(p["head_x"]*SCALE-width/2),p["hat_top"]*SCALE))
        # Crown and near brim in front; the far brim is a true rear layer.
        left,right=p["brim_y"]
        front_selector=polygon([[0,left],[192,right],[192,192],[0,192]])
        crown=polygon([[p["head_x"]-10,12],[p["head_x"]+10,12],[p["head_x"]+12,44],[p["head_x"]-12,44]])
        front_selector=ImageChops.lighter(front_selector,crown)
        hat_rear,hat_front=split(headgear,front_selector)
        weapon,bbox_weapon=largest_item(weapon_source,p["window"])
        weapon,transform=mount_weapon(weapon,bbox_weapon,p)
        weapon_selector=Image.new("L",(CELL,CELL),255 if p["depth"]=="front" else 0)
        weapon_rear,weapon_front=split(weapon,weapon_selector)

        composite=Image.new("RGBA",(CELL,CELL))
        composite.alpha_composite(hat_rear)
        composite.alpha_composite(weapon_rear)
        composite.alpha_composite(body_core)
        composite.alpha_composite(weapon_front)
        # Hands are always drawn over the held hilt. Body is the occluder for
        # rear weapons; the complete unclipped item is still exported.
        composite.alpha_composite(hand_layer)
        composite.alpha_composite(hat_front)
        composite.alpha_composite(head_layer)

        tinted=body.copy()
        for mask,color in [(opaque,(63,131,191)),(head,(193,81,173)),(hand,(245,170,54))]:
            color_layer=Image.new("RGBA",(CELL,CELL),(*color,0))
            color_layer.putalpha(mask.point(lambda v:round(v*.48)))
            tinted.alpha_composite(color_layer)
        # Show the actual visible weapon; hidden hilt pixels remain hidden.
        visible_weapon=clip(weapon,ImageChops.invert(hand))
        if p["depth"]=="rear":
            visible_weapon=clip(visible_weapon,ImageChops.invert(body.getchannel("A")))
        tinted.alpha_composite(visible_weapon)
        review_masks.append(tinted)
        values={"base-original":orig4,"base-without-weapon":body,"body-core":body_core,"right-hand":hand_layer,"head-foreground":head_layer,"headgear-full":headgear,"headgear-rear":hat_rear,"headgear-front":hat_front,"weapon-full":weapon,"weapon-rear":weapon_rear,"weapon-front":weapon_front,"preview-equipped":composite}
        mask_values={"head-occluder":head,"hand-occluder":hand,"body-occluder":body.getchannel("A"),"old-weapon-remove":removal_small.resize((CELL,CELL),Image.Resampling.NEAREST),"headgear-front-selector":front_selector,"weapon-front-selector":weapon_selector,"head-foreground-selector":head_selector,"right-hand-selector":hand_selector}
        for key,value in values.items():layers[key].append(value)
        for key,value in mask_values.items():masks[key].append(value)
        partition={}
        for label,full,rear,front in [("headgear",headgear,hat_rear,hat_front),("weapon",weapon,weapon_rear,weapon_front)]:
            aa=np.asarray(full.getchannel("A"),dtype=int);ar=np.asarray(rear.getchannel("A"),dtype=int);af=np.asarray(front.getchannel("A"),dtype=int)
            assert np.array_equal(aa,ar+af)
            assert not np.any((ar>0)&(af>0))
            b=full.getbbox();assert b and min(b[0],b[1],CELL-b[2],CELL-b[3])>=4*SCALE,(name,label,b)
            partition[label]=dict(alpha_partition_exact=True,alpha_overlap_pixels=0,margin_base_px=min(b[0],b[1],CELL-b[2],CELL-b[3])/SCALE)
        # An actual composite readback checks hand colors (excluding headgear,
        # which is spatially disjoint) and unchanged original body regions.
        protected=(np.asarray(hand)==255)&(np.asarray(orig4.getchannel("A"))==255)
        hand_changed=int(np.any(np.asarray(composite)!=np.asarray(orig4),axis=2)[protected].sum())
        assert hand_changed==0,(name,hand_changed)
        changed=np.any(np.asarray(clean)!=np.asarray(orig),axis=2)
        outside=int((changed&(np.asarray(removal_small)==0)).sum())
        metrics.append(dict(direction=name,partition=partition,body_parts_visible_rgba_reconstruction_exact=True,protected_hand_changed_pixels=hand_changed,base_changes_outside_removal=outside,reconstructed_base_pixels=int(changed.sum()),weapon_depth=p["depth"],mask_visual_approval=False))
        pose_records.append(dict(direction=name,**p,hat_source_bbox=bbox_hat,weapon_source_bbox=bbox_weapon,weapon_transform=transform))

    for group,folder in [(layers,OUT/"layers"),(masks,OUT/"masks")]:
        folder.mkdir(exist_ok=True)
        for key,images in group.items():
            stitch(images).save(folder/(key+"-4x.png"))
    review_grid(layers["preview-equipped"],"분리 장비 예시 · 패랭이 + 환도","정지 8방향 검토용 | 방향별 독립 원화 · 오른손 고정 · 게임 미적용","preview-equipped-8dir.png",overlay=["등 / 칼 뒤","등 대각 / 칼 뒤","오른쪽 / 칼 뒤","앞 대각 / 칼 앞","정면 / 칼 앞","앞 대각 / 칼 앞","왼쪽 / 칼 뒤","등 대각 / 칼 뒤"])
    parts=[]
    for hat,weapon in zip(layers["headgear-full"],layers["weapon-full"]):
        im=hat.copy();im.alpha_composite(weapon);parts.append(im)
    review_grid(parts,"장비와 무기 · 몸체 없이 따로","슬롯별 RGBA 원화 | 칼·모자를 같은 좌표계에 개별 보존","preview-parts-8dir.png")
    review_grid(review_masks,"방향별 가림 마스크","파랑: 몸체 / 자주: 얼굴·뒷머리 / 주황: 오른손 | 원화 승인 전 검토용","preview-masks-8dir.png")
    # Four panels show actual layer inputs, not an illustrative substitute.
    panel=Image.new("RGB",(1536,840),(239,237,227));d=ImageDraw.Draw(panel)
    d.text((30,18),"SE 예시 · 부위 원화와 가림 순서",font=font(32),fill=(28,42,46))
    for col,(title,im) in enumerate([("기존 몸체",layers["base-original"][3]),("장비·무기 분리",parts[3]),("방향별 가림",review_masks[3]),("조합 미리보기",layers["preview-equipped"][3])]):
        d.text((col*384+24,84),title,font=font(24),fill=(37,61,66))
        tile=im.resize((384,384),Image.Resampling.LANCZOS);panel.paste(tile,(col*384,126),tile)
    d.text((30,562),"뒤 장비 → 몸체 → 앞 무기 → 오른손 → 앞 모자 → 얼굴·뒷머리",font=font(25),fill=(37,61,66))
    d.text((30,613),"N·NE·E·W·NW: 칼을 몸 뒤에 / SE·S·SW: 칼을 몸 앞에",font=font(22),fill=(66,76,75))
    d.text((30,660),"정지 예시 1종씩입니다. 걷기·공격 포즈에는 새 원화와 마스크가 필요합니다.",font=font(21),fill=(66,76,75))
    d.text((30,707),"기존 몸체의 칼 제거 부위는 검토용 복원입니다. 화풍·착용감·가림의 시각 승인은 대기입니다.",font=font(20),fill=(66,76,75))
    panel.save(OUT/"preview-se-breakdown.png")
    json_write(OUT/"registration.json",dict(schema_version=1,pose="idle",cell_px=192,art_scale=4,atlas_grid=[4,2],directions=NAMES,foot_root=[96,176],annotations="Manual source-image annotations; mounting targets are not independent alignment validation",pose_records=pose_records))
    json_write(OUT/"layer-contract.json",dict(schema_version=1,status="review_only_not_runtime",direction_order=NAMES,anatomical_weapon_hand="right",mirroring_allowed=False,draw_order=["headgear-rear","weapon-rear","body-core","weapon-front","right-hand","headgear-front","head-foreground"],mask_convention="L PNG, 0 transparent/255 occluding. Selectors are binary item-part selectors; occluders are body alpha at that pose. Body-core/right-hand/head-foreground are disjoint RGBA layers: do not also draw base-without-weapon or apply the occluder a second time.",body_occlusion="Rear items are naturally occluded by the disjoint body parts; front weapons are occluded by the original right hand. Crown/near brim are front; far brim is rear. Original face/nape pixels are drawn once on top.",slots=[dict(id="paeraengi-example",slot="head",front="layers/headgear-front-4x.png",rear="layers/headgear-rear-4x.png",full="layers/headgear-full-4x.png"),dict(id="hwando-indigo-example",slot="weapon_right",front="layers/weapon-front-4x.png",rear="layers/weapon-rear-4x.png",full="layers/weapon-full-4x.png")],scope="Idle only. No walking/attack transfer. New poses require individually painted equipment, anchors and occlusion review; no automatic hand swapping or horizontal flips."))
    json_write(OUT/"validation.json",dict(status="static_checks_passed_visual_approval_pending",generated_with="built-in image_gen",generation_reproducible=False,checks=metrics,game_connected=False,game_execution_tested=False,animation_tested=False,anatomical_alignment_certified=False,underpaint_note="Generated underpaint is used only inside old-weapon-remove, with original hand/cuff protected. Pixels outside that selection are exactly preserved in the review base. It is a draft reconstructed base, not an approved unarmed body.",source_sha256={str(p.relative_to(ROOT)):sha(p) for p in [ROOT/"assets/warrior-idle-8dir-v1.png",OUT/"paeraengi-generated-source.png",OUT/"hwando-generated-source.png",OUT/"weapon-underpaint-generated-source.png"]}))
    print("Prepared 8 idle directions; 2 independent item slots; 8/8 hand protection, mask partitions and margins passed.")


if __name__ == "__main__":
    main()
