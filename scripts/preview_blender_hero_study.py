"""Label/composite actual Blender renders for review; never redraw the actor."""
from pathlib import Path
import hashlib,json
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];SRC=ROOT/'build-blender-study/renders';ART=ROOT/'art/blender-hero-study';OUT=ART/'renders';OUT.mkdir(exist_ok=True)
font_path=Path('C:/Windows/Fonts/malgun.ttf')
font=ImageFont.truetype(str(font_path),24) if font_path.exists() else ImageFont.load_default()
small=ImageFont.truetype(str(font_path),18) if font_path.exists() else font
def paste(canvas,im,xy,size=None):
    im=im.convert('RGBA')
    if size:im=im.convert('RGBa').resize(size,Image.Resampling.LANCZOS).convert('RGBA')
    canvas.paste(im,xy,im)
report={}
for name in ['front','clay-front','side-diagnostic']:
    p=SRC/(name+'.png');im=Image.open(p)
    assert im.mode=='RGBA' and im.size==(768,768)
    assert im.getchannel('A').getbbox() is not None,(name,'empty render')
    assert im.info.get('File','').endswith('hero-front-study.blend')
    assert im.info.get('Camera') in ['CAM_approved_front','CAM_side_diagnostic']
    assert 'RenderTime' in im.info and im.info.get('cycles.ViewLayer.samples')=='12'
    bounds=im.getchannel('A').getbbox();assert min(bounds[0],bounds[1],768-bounds[2],768-bounds[3])>=4,(name,bounds)
    report[name]=dict(raw_render_sha256=hashlib.sha256(p.read_bytes()).hexdigest(),blender_project=Path(im.info['File']).name,camera=im.info['Camera'],frame=im.info['Frame'],size=list(im.size),alpha=list(im.getchannel('A').getextrema()),bounds=list(bounds))
    # Public review copies keep identical RGBA pixels without the local
    # machine path stamped by Blender. Original renders remain in build/.
    im.save(OUT/p.name)
    assert Image.open(OUT/p.name).tobytes()==im.tobytes()
    report[name]['public_sha256']=hashlib.sha256((OUT/p.name).read_bytes()).hexdigest()
board=Image.new('RGB',(1536,860),(31,46,51));draw=ImageDraw.Draw(board)
draw.text((30,16),'기준 원화',font=font,fill='#ede2cb');draw.text((798,16),'Blender 앞면 투사 시험 · 실제 렌더',font=font,fill='#ede2cb')
paste(board,Image.open(ROOT/'art/hero/warrior-clarity-master.png'),(22,65),(724,724))
paste(board,Image.open(SRC/'front.png'),(768,43))
draw.text((30,820),'앞면 그림을 입힌 볼륨 초안입니다. 옆·뒷면과 가려진 부분은 완성 전이며 게임에는 적용하지 않았습니다.',font=small,fill='#d8ccaf')
board.save(ROOT/'docs/blender-style-comparison.png')
diagnosis=Image.new('RGB',(1536,835),(31,46,51));d=ImageDraw.Draw(diagnosis)
d.text((30,16),'진단용 구조 초안 · 재질 숨김',font=font,fill='#ede2cb');d.text((798,16),'옆면 진단 · 투사 그림의 늘어짐/중복 미해결',font=font,fill='#ede2cb')
paste(diagnosis,Image.open(SRC/'clay-front.png'),(0,50));paste(diagnosis,Image.open(SRC/'side-diagnostic.png'),(768,50));diagnosis.save(ROOT/'docs/blender-volume-diagnostic.png')
frames=[];mechanics_frames=[];hashes=[];mechanic_hashes=[]
for index in range(1,25):
    p=SRC/f'walk-{index:02d}.png'
    im=Image.open(p);assert im.mode=='RGBA' and im.size==(512,512)
    bbox=im.getchannel('A').getbbox();assert bbox is not None
    assert min(bbox[0],bbox[1],512-bbox[2],512-bbox[3])>=4,(p.name,bbox)
    hashes.append(hashlib.sha256(im.tobytes()).hexdigest())
    assert im.info.get('Camera')=='CAM_approved_front' and int(im.info['Frame'])==index
    mp=SRC/f'mechanics-{index:02d}.png';mechanics=Image.open(mp)
    assert mechanics.mode=='RGBA' and mechanics.size==(512,512)
    assert mechanics.info.get('Camera')=='CAM_walk_mechanics' and int(mechanics.info['Frame'])==index
    mb=mechanics.getchannel('A').getbbox();assert mb and min(mb[0],mb[1],512-mb[2],512-mb[3])>=4,(mp.name,mb)
    mechanic_hashes.append(hashlib.sha256(mechanics.tobytes()).hexdigest())
    frame=Image.new('RGB',(512,565),(31,46,51));paste(frame,im,(0,40))
    ImageDraw.Draw(frame).text((15,7),'보행 수정 · 실제 Blender 렌더',font=small,fill='#ede2cb');frames.append(frame)
    diagnosis_frame=Image.new('RGB',(512,565),(31,46,51));paste(diagnosis_frame,mechanics,(0,40))
    ImageDraw.Draw(diagnosis_frame).text((15,7),'옆면 구조 · 팔/다리 교차와 옷 변형 확인',font=small,fill='#ede2cb');mechanics_frames.append(diagnosis_frame)
assert len(frames)==24 and len(set(hashes))==24 and len(set(mechanic_hashes))==24
frames[0].save(ROOT/'docs/blender-walk-study.gif',save_all=True,append_images=frames[1:],duration=[40,40,40,40,40,50]*4,loop=0,disposal=2)
mechanics_frames[0].save(ROOT/'docs/blender-walk-mechanics.gif',save_all=True,append_images=mechanics_frames[1:],duration=[40,40,40,40,40,50]*4,loop=0,disposal=2)
report['walk']=dict(sampled_frames=24,timeline_frames=24,fps=24,frame_rgba_sha256=hashes,mechanics_frame_rgba_sha256=mechanic_hashes,gif_total_ms=1000)
(ART/'render-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('Validated 3 actual Blender RGBA views and 24 front + 24 mechanics walking renders')
