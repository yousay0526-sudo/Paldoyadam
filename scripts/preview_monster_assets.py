#!/usr/bin/env python3
"""Review prepared PNGs at a readable scale; this is not a game capture."""
from pathlib import Path
import json
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'art/monsters/prepared'
FONT=ImageFont.truetype(str(ROOT/'assets/fonts/NanumMyeongjo-Bold.ttf'),18)
ORDER=('N','NE','E','SE','S','SW','W','NW')
REPORT=json.loads((ROOT/'art/monsters/preparation.json').read_text(encoding='utf-8'))
ROWS=(('boar-walk','돼지 걷기'),('boar-attack','돼지 공격'),
      ('butcher-walk','백정 걷기'),('butcher-attack','백정 공격'),('butcher-skill','백정 스킬'))

def base(labels):
    sheet=Image.new('RGB',(1664,len(labels)*176+44),(159,164,143))
    pen=ImageDraw.Draw(sheet)
    for d,label in enumerate(ORDER):pen.text((128+d*192+80,10),label,font=FONT,fill=(30,36,30))
    for row,label in enumerate(labels):pen.text((6,44+row*176+80),label,font=FONT,fill=(25,31,25))
    return sheet

def add(sheet,atlas,source,row,d,compensation):
    size=round(128*compensation)
    pose=atlas.crop(source).resize((size,size),Image.Resampling.NEAREST)
    x=128+d*192;y=44+row*176
    ImageDraw.Draw(sheet).line((x+40,y+150,x+152,y+150),fill=(204,207,185))
    sheet.paste(pose,(round(x+96-size*.5),round(y+150-size*176/192)),pose)

frames=[]
for frame in range(4):
    sheet=base([label for _,label in ROWS])
    for row,(name,_) in enumerate(ROWS):
        atlas=Image.open(ART/f'{name}-8dir-v1.png')
        for d in range(8):add(sheet,atlas,(frame*192,d*192,(frame+1)*192,(d+1)*192),row,d,REPORT['atlases'][name]['frames'][d*4+frame]['draw_scale_compensation'])
    frames.append(sheet)
frames[0].save(ROOT/'docs/monster-motion-candidates.gif',save_all=True,append_images=frames[1:],duration=220,loop=0,optimize=True)
standing=base(['돼지 정지','백정 정지'])
for row,name in enumerate(('boar','butcher')):
    atlas=Image.open(ART/f'{name}-idle-8dir-v1.png')
    for d in range(8):add(standing,atlas,(d%4*192,d//4*192,(d%4+1)*192,(d//4+1)*192),row,d,REPORT['atlases'][name+'-idle']['frames'][d]['draw_scale_compensation'])
standing.save(ROOT/'docs/monster-direction-candidates.png')
print('Prepared-PNG reviews saved; not gameplay/SDL captures.')
