#!/usr/bin/env python3
"""Select authored still-image gear by alpha masks; never paint or recolor art.

All coordinates stay in the shared 1254px source canvas. JSON records cropped
PNG offsets, avoiding independent part fitting or body-part rotations.
"""
import hashlib
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter
import numpy as np
from collections import deque

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'web/equipment-lab/assets'
ART = ROOT / 'art/equipment-lab'
SIZE = (1254, 1254)
FACE = [(586,132),(695,132),(706,177),(700,212),(677,237),(643,259),
        (608,237),(585,212),(574,183)]
RIGHT_HAND = [(345,570),(425,573),(452,620),(426,668),(344,668)]
LEFT_HAND = [(813,663),(829,659),(835,672),(863,679),(890,679),
             (892,718),(873,737),(831,738),(807,719),(805,690)]
SPECS = {
    'general': {
        'name': '장군', 'source': ROOT/'art/hero/warrior-clarity-master.png',
        'weapon': [(188,1090),(208,1090),(399,704),(422,677),(425,646),
                   (444,615),(446,583),(428,567),(389,573),(369,597),
                   (362,623),(344,642),(344,668),(360,698),(302,832)]
    },
    'light': {
        'name': '경장', 'source': ART/'light-master.png',
        'weapon': [(185,1090),(210,1090),(391,721),(418,668),(430,642),
                   (445,615),(446,581),(425,565),(390,576),(367,601),
                   (355,627),(345,648),(354,678),(324,755)]
    },
    'heavy': {
        'name': '중장', 'source': ART/'heavy-master.png',
        'weapon': [(160,1113),(246,1080),(374,790),(421,694),(429,665),
                   (440,637),(451,604),(448,574),(419,557),(387,571),
                   (371,597),(348,621),(335,651),(353,698),(303,839)]
    }
}


def polygon(points):
    mask = Image.new('L', (SIZE[0]*4,SIZE[1]*4))
    ImageDraw.Draw(mask).polygon([(x*4,y*4) for x,y in points], fill=255)
    return mask.resize(SIZE,Image.Resampling.LANCZOS)


def region(points):
    # Hard, complementary boundaries at the shared waist; antialias only
    # silhouette cutouts, never the seam between two independently worn items.
    mask = Image.new('L',SIZE)
    ImageDraw.Draw(mask).polygon(points,fill=255)
    return mask


def remove_specks(alpha, minimum=200, largest=False):
    pixels=np.asarray(alpha).copy()
    seen=np.zeros(pixels.shape,dtype=bool)
    biggest=[]
    for y,x in zip(*np.nonzero(pixels>16)):
        if seen[y,x]: continue
        queue=deque([(y,x)]);seen[y,x]=True;component=[]
        while queue:
            cy,cx=queue.popleft();component.append((cy,cx))
            for ny,nx in [(cy-1,cx),(cy+1,cx),(cy,cx-1),(cy,cx+1)]:
                if 0<=ny<SIZE[1] and 0<=nx<SIZE[0] and pixels[ny,nx]>16 and not seen[ny,nx]:
                    seen[ny,nx]=True;queue.append((ny,nx))
        if len(component)<minimum:
            for cy,cx in component: pixels[cy,cx]=0
        if len(component)>len(biggest): biggest=component
    if largest:
        support=np.zeros(pixels.shape,dtype='uint8')
        for cy,cx in biggest: support[cy,cx]=255
        support=Image.fromarray(support).filter(ImageFilter.MaxFilter(3))
        return ImageChops.multiply(Image.fromarray(pixels),support)
    return Image.fromarray(pixels)


def union(polygons):
    result = Image.new('L', SIZE)
    for points in polygons:
        result = ImageChops.lighter(result, polygon(points))
    return result


def subtract(a, *masks):
    for b in masks:
        a = ImageChops.subtract(a, b)
    return a


def clean(path):
    im = Image.open(path).convert('RGBA')
    if im.size != SIZE:
        raise ValueError(f'Unexpected source size: {path}: {im.size}')
    im.putalpha(im.getchannel('A').point(lambda a: 0 if a < 5 else a))
    return im


def red_cloth(source, region):
    # Select the requested red cloth item, not everything inside a coarse
    # bounding polygon. Keep original RGB/alpha; this is extraction, not
    # recoloring or removing red from the character.
    rgb = np.asarray(source).astype(np.float32)
    red = (rgb[:,:,0]>65) & (rgb[:,:,0]>rgb[:,:,1]*2) & (rgb[:,:,0]>rgb[:,:,2]*2.3)
    mask = Image.fromarray((red*255).astype('uint8'))
    # Opening removes thin red rivet/coat fragments; closing restores the
    # inked folds inside the cloth. Source RGB remains untouched.
    mask=mask.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(3))
    mask=mask.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(3))
    return ImageChops.multiply(mask, region)


def save(im, mask, name, largest=False):
    part = im.copy()
    part.putalpha(remove_specks(ImageChops.multiply(im.getchannel('A'), mask),largest=largest))
    bounds = part.getbbox()
    if not bounds:
        raise ValueError(f'Empty selection: {name}')
    cropped = part.crop(bounds)
    filename = name + '.png'
    cropped.save(OUT/filename, optimize=True)
    return dict(file=filename, x=bounds[0], y=bounds[1],
                width=cropped.width, height=cropped.height,
                sha256=hashlib.sha256((OUT/filename).read_bytes()).hexdigest())


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    face = polygon(FACE)
    # Hide the entire replaced body region. The old hand-drawn erasers were
    # narrower than the white sleeves and left a bright fringe at the elbows.
    body_head=ImageChops.lighter(region([(0,0),(1254,0),(1254,190),(0,190)]),face)
    below_waist=region([(0,519),(1254,519),(1254,1254),(0,1254)])
    body_arms=ImageChops.lighter(region([(0,518),(455,518),(455,670),(0,670)]),
                     region([(805,518),(1254,518),(1254,752),(805,752)]))
    body_lower=subtract(below_waist,body_arms)
    body_upper=subtract(Image.new('L',SIZE,255),body_head,body_lower)
    masks = {
        'upper': body_upper,
        'lower': body_lower,
        'head': polygon([(520,0),(774,0),(774,135),(520,135)]),
        'right-hand': polygon(RIGHT_HAND),
        'left-hand': polygon([(800,640),(905,640),(905,750),(800,750)]),
    }
    white = Image.new('RGBA', SIZE, (255,255,255,255))
    erase = {key: save(white, mask, 'erase-'+key) for key,mask in masks.items()}
    body = clean(ART/'body-master.png')
    base = save(body, Image.new('L', SIZE, 255), 'body')
    slots = {key: [] for key in ['upper','lower','head','belt','weapon']}
    sources = {}
    for key,spec in SPECS.items():
        source = clean(spec['source'])
        outer_path = ART/(key+'-clean-master.png')
        outer = clean(outer_path)
        lower_path=ART/(key+'-lower-master.png')
        trousers=clean(lower_path)
        sources[key] = dict(path=str(spec['source'].relative_to(ROOT)).replace('\\','/'),
                            sha256=hashlib.sha256(spec['source'].read_bytes()).hexdigest(),
                            clean_path=str(outer_path.relative_to(ROOT)).replace('\\','/'),
                            clean_sha256=hashlib.sha256(outer_path.read_bytes()).hexdigest(),
                            lower_path=str(lower_path.relative_to(ROOT)).replace('\\','/'),
                            lower_sha256=hashlib.sha256(lower_path.read_bytes()).hexdigest())
        # Restrict the red-cloth selection to traced boundaries; loose
        # chroma keying alone retained coat pixels outside the sash.
        belt_paths={
          'general': [(511,443),(543,457),(564,462),(589,459),(624,468),(661,471),(694,461),(720,451),(733,452),(733,495),(716,513),(711,536),(723,559),(744,581),(746,585),(727,599),(711,599),(725,655),(747,721),(777,794),(799,839),(789,846),(769,864),(742,890),(728,862),(702,810),(683,744),(672,672),(662,608),(662,546),(648,544),(628,532),(600,538),(570,532),(551,522),(520,515),(507,501),(505,479),(507,460)],
          'light': [(520,450),(543,459),(579,468),(621,470),(661,471),(692,464),(717,456),(731,452),(734,470),(731,491),(729,510),(716,522),(742,582),(773,640),(799,693),(824,739),(857,778),(898,804),(958,838),(958,857),(952,863),(951,880),(938,894),(928,902),(908,885),(887,864),(872,847),(857,877),(841,854),(814,821),(792,788),(773,798),(757,779),(741,769),(728,758),(725,713),(707,668),(682,660),(664,653),(656,553),(656,532),(625,533),(580,535),(544,528),(520,516)],
          'heavy': [(513,444),(537,457),(575,470),(624,476),(664,477),(698,464),(728,450),(735,452),(738,478),(738,499),(736,519),(712,531),(714,550),(734,583),(748,618),(767,663),(788,710),(810,762),(833,813),(852,856),(835,869),(819,883),(792,901),(775,875),(753,834),(730,792),(712,750),(698,704),(684,662),(678,650),(667,648),(664,578),(663,534),(636,522),(601,523),(565,521),(535,519),(513,509),(508,505),(510,480)]}
        belt=red_cloth(source,polygon(belt_paths[key]))
        belt=ImageChops.multiply(polygon(belt_paths[key]),
             belt.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5)))
        if key == 'general':
            # Preserve the gold buckle as part of the sash item.
            buckle = polygon([(552,479),(562,474),(573,470),(586,473),
                 (601,471),(613,477),(624,477),(631,483),
                 (634,490),(632,507),(622,520),(606,532),
                 (590,536),(574,530),(560,520),(552,506),(548,491),(550,479)])
            belt = ImageChops.lighter(belt,buckle)
        if key=='heavy':
            belt=belt.filter(ImageFilter.MinFilter(5)).filter(ImageFilter.MaxFilter(5))
        weapon = polygon(spec['weapon'])
        # A garment is an item, not a horizontal slice of the character.
        # Keep the entire jacket/coat to its authored hem, including split
        # front panels. Pants are separately authored beneath the long coats.
        hems={
          'general':[(1254,970),(875,970),(860,985),(835,1000),(815,1008),
             (795,1020),(775,1022),(760,1032),(737,1037),(715,1034),
             (695,1027),(675,1022),(655,1030),(645,1027),(645,1009),
             (635,1002),(627,975),(610,977),(594,983),(578,986),(561,992),
             (545,991),(533,997),(518,999),(500,995),(484,987),(477,981),
             (470,970),(461,980),(449,975),(439,966),(0,966)],
          'light':[(1254,650),(818,650),(794,675),(752,690),(701,700),
             (650,708),(605,713),(597,674),(590,674),(566,666),(558,705),
             (524,696),(491,684),(464,670),(454,658),(0,658)],
          'heavy':[(1254,942),(900,942),(850,957),(800,976),(750,987),
             (700,997),(660,1001),(642,788),(613,997),(565,990),(520,984),
             (475,970),(440,957),(0,957)]}
        upper=polygon([(0,180),(1254,180)]+hems[key])
        # Sleeve cuffs can hang lower than a short jacket's hem. Preserve
        # their full contours independently of the central coat boundary.
        upper=ImageChops.lighter(upper,union([
            [(250,518),(451,518),(451,668),(250,668)],
            [(820,518),(1000,518),(1000,650),(903,650),(903,719),
             (884,740),(846,744),(815,730),(798,716),(798,692),
             (812,670),(817,640),(817,600)]]))
        lower=Image.new('L',SIZE,255)
        upper=subtract(upper,face)
        upper=subtract(upper,polygon(RIGHT_HAND))
        if key=='light':
            upper=subtract(upper,polygon([(711,180),(810,180),(810,229),
                                         (761,217),(739,210),(716,196)]))
        layer_masks = {'upper':upper,'lower':lower,'belt':belt,'weapon':weapon}
        if key != 'general':
            if key=='light':
                layer_masks['head']=union([
                  [(530,0),(755,0),(755,125),(716,151),(706,176),(695,174),(699,151),(686,139),(661,131),(638,131),(616,136),(599,145),(590,159),(588,177),(578,176),(570,155),(558,152),(530,135)],
                  [(711,165),(735,172),(752,188),(767,210),(758,207),(739,200),(723,190),(710,187)]])
            else:
                layer_masks['head']=polygon([(535,0),(755,0),(755,119),(746,146),
                    (746,169),(765,192),(751,201),(734,209),(716,212),(706,207),
                    (710,191),(709,166),(702,142),(683,136),(657,132),(631,132),
                    (606,135),(589,140),(581,168),(576,193),(567,210),(552,212),
                    (530,204),(515,193),(532,169),(537,145),(535,119)])
        for slot,mask in layer_masks.items():
            item_source = outer if slot=='upper' else trousers if slot=='lower' else source
            part = save(item_source, mask, key+'-'+slot,largest=slot in ['upper','lower','belt'])
            parts = [part]
            if slot == 'upper':
                hand = save(outer,polygon(LEFT_HAND),key+'-left-hand')
                hand['pass'] = 'hands'
                parts.append(hand)
                right = save(outer,polygon(RIGHT_HAND),key+'-empty-right-hand')
                right.update({'pass':'hands','condition':'no-weapon'})
                parts.append(right)
            slots[slot].append(dict(id=key+'-'+slot,name=spec['name']+
                {'upper':' 상의','lower':' 하의','belt':' 붉은 허리띠','head':' 머리 장비','weapon':' 환도'}[slot],
                layers=parts,erase=[slot,'left-hand','right-hand'] if slot=='upper' else
                       [slot] if slot in ['lower','head'] else
                       ['right-hand'] if slot=='weapon' else []))
    manifest = dict(version=1,canvas=list(SIZE),pose='정지 · 정면 사선 한 자세',
        body=base,erase=erase,slots=slots,
        order=['lower','upper','belt','head','weapon'],
        default=dict(upper='general-upper',lower='general-lower',belt='general-belt',head='',weapon='general-weapon'),
        notes=['정지 한 자세 전용. 모션·8방향·본게임 장착 수치 연결 전.',
               '상의는 소매와 옷자락 전체, 하의는 독립 바지와 신발입니다.',
               '검 레이어에는 고정된 잡는 손 부분을 포함해 손잡이를 가립니다.'])
    (OUT/'catalog.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    report = dict(sources=sources,body_sha256=hashlib.sha256((ART/'body-master.png').read_bytes()).hexdigest(),
        source_canvas=list(SIZE),processing='Contour selection, cloth-colour mask cleanup, connected silhouette selection and crop. Original RGB preserved; no mirror, scaling or bone rotation.',
        source_art_ref='PR #39 ae7ea48: light/heavy source bytes preserved.',
        slot_contract='Upper includes complete jacket/coat and sleeves down to its natural hem; lower contains independently authored complete trousers/greaves/boots, never coat panels. Red cloth belts separate. Weapon includes right gripping-hand patch.',
        catalog_sha256=hashlib.sha256((OUT/'catalog.json').read_bytes()).hexdigest())
    (ART/'preparation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(f'Prepared body, {sum(map(len,slots.values()))} gear choices and {len(erase)} base masks')


if __name__ == '__main__':
    main()
