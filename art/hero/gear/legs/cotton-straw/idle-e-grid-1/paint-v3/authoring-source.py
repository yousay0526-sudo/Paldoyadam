"""Direct E-only paint on an absolute native grid. No guide/image fitting.

The user approved direct grid redraw after the two generative review rounds.
Every contour below is freshly authored; no previous foot is moved or warped.
"""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageChops

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'art/hero/gear/legs/cotton-straw/idle-e-grid-1/paint-v3'
S=16
COL={'outline':'#695b45','cotton':'#dfd1b2','cotton_dark':'#ad9876','cotton_mid':'#c8b591',
     'cotton_light':'#f4ead2','indigo':'#29354e','indigo_dark':'#182338','indigo_mid':'#475575',
     'sock':'#eeeade','sock_shadow':'#bbb7b3','sock_light':'#fffdf4',
     'straw':'#b98a45','rope_dark':'#79552c','rope_light':'#ead096','rope_mid':'#d1a563'}

def points(commands):
    out=[];current=None
    for cmd in commands:
        if cmd[0]=='M':current=(cmd[1],cmd[2]);out.append(current)
        elif cmd[0]=='L':current=(cmd[1],cmd[2]);out.append(current)
        elif cmd[0]=='C':
            p0=current;p1=cmd[1:3];p2=cmd[3:5];p3=cmd[5:7]
            for i in range(1,25):
                t=i/24;u=1-t
                out.append((u*u*u*p0[0]+3*u*u*t*p1[0]+3*u*t*t*p2[0]+t*t*t*p3[0],
                            u*u*u*p0[1]+3*u*u*t*p1[1]+3*u*t*t*p2[1]+t*t*t*p3[1]))
            current=p3
    return out

def path(im,cmd,fill=None,stroke=None,width=.35,closed=False):
    xy=[(round(x*S),round(y*S)) for x,y in points(cmd)]
    d=ImageDraw.Draw(im)
    if fill:d.polygon(xy,fill=COL.get(fill,fill))
    if stroke:
        if closed:xy.append(xy[0])
        d.line(xy,fill=COL.get(stroke,stroke),width=max(1,round(width*S)),joint='curve')

def shade(im,cmd,color):
    # Clip decorative paint only to this already drawn shape, never to a guide.
    overlay=Image.new('RGBA',im.size)
    path(overlay,cmd,fill=color)
    overlay.putalpha(ImageChops.multiply(overlay.getchannel('A'),im.getchannel('A')))
    im.alpha_composite(overlay)

def rope(im,cmd,width=.85):
    path(im,cmd,stroke='rope_dark',width=width+.32)
    path(im,cmd,stroke='straw',width=width)
    path(im,cmd,stroke='rope_mid',width=width*.56)
    pts=points(cmd);dist=0;last=pts[0]
    for p in pts[1:]:
        dx,dy=p[0]-last[0],p[1]-last[1];dist+=math.hypot(dx,dy)
        if dist>=.64:
            length=max(math.hypot(dx,dy),.001);nx,ny=-dy/length,dx/length
            d=ImageDraw.Draw(im);r=width*.33
            d.line((round((p[0]-nx*r-.12)*S),round((p[1]-ny*r-.06)*S),round((p[0]+nx*r+.12)*S),round((p[1]+ny*r+.06)*S)),fill=COL['rope_light'],width=max(1,round(.19*S)))
            dist=0
        last=p

def far_pants():
    im=Image.new('RGBA',(192*S,192*S))
    contour=[('M',86.9,139.8),('L',98.4,140.1),('C',97.1,147.7,96.0,154.4,95.2,158.7),('C',95.0,160.6,95.6,161.9,94.5,163.5),('C',93.4,164.0,89.8,164.0,88.5,163.3),('C',86.4,161.1,86.1,159.6,86.3,157.2),('C',86.6,150.9,86.8,145.5,86.9,139.8)]
    path(im,contour,'cotton','outline',.42,True)
    shade(im,[('M',87.0,140),('L',90.0,140),('C',88.1,150.0,90.5,155.7,89.6,162.2),('L',88.4,163.4),('L',86.4,158.3)],'cotton_mid')
    shade(im,[('M',92.0,141),('L',96.9,141),('C',96.0,148.0,92.8,156.8,91.5,162.7),('L',89.6,161.2),('C',92.5,152.4,91.5,148.1,92.0,141)],'cotton_light')
    path(im,[('M',88.0,147),('C',88.2,153.5,90.2,158.1,89.4,161.3)],stroke='cotton_dark',width=.32)
    path(im,[('M',95.2,146.4),('C',94.8,153.0,91.6,158.5,91.6,162.8)],stroke='cotton_mid',width=.38)
    path(im,[('M',88.7,161.7),('L',90.5,163.1),('L',92.2,162.7),('L',94.0,163.3)],stroke='cotton_dark',width=.3)
    return im

def near_pants():
    im=Image.new('RGBA',(192*S,192*S))
    contour=[('M',91.0,139.8),('L',108.0,140.1),('C',106.6,148.0,105.6,155.4,104.8,160.9),('C',104.5,163.5,104.0,166.9,100.3,169.2),('C',98.6,169.7,95.4,169.5,93.5,168.9),('C',92.6,166.4,93.4,164.6,92.9,161.9),('C',92.1,155.6,92.4,149.1,91.0,139.8)]
    path(im,contour,'cotton','outline',.42,True)
    shade(im,[('M',91.2,140.1),('L',94.4,140.1),('C',92.9,150.9,96.3,157.4,94.4,164.2),('L',96.7,168.7),('L',93.4,169.1),('L',91.9,161.7)],'cotton_mid')
    shade(im,[('M',97.0,140.6),('L',106.9,140.6),('C',104.5,147.2,99.2,155.0,97.5,161.9),('C',96.9,165.0,99.0,166.8,98.9,168.7),('L',96.7,168.5),('C',94.6,164.8,95.9,160.2,96.5,157.2),('C',98.7,149.1,96.1,145.1,97.0,140.6)],'cotton_light')
    shade(im,[('M',106.8,143.0),('L',109.7,141),('C',106.9,155.1,104.8,165.6,100.1,169.1),('L',98.4,168.6),('C',102.7,164.4,103.0,153.2,106.8,143.0)],'cotton_mid')
    path(im,[('M',95.2,146.3),('C',95.5,154.0,94.0,158.4,94.8,162.6),('L',96.4,166.9)],stroke='cotton_mid',width=.4)
    path(im,[('M',104.9,149.6),('C',102.8,157.4,98.7,161.4,98.1,166.6)],stroke='cotton_mid',width=.38)
    path(im,[('M',94.2,166.5),('L',95.6,168.7),('L',97.0,167.5),('L',98.4,169.0),('L',100.2,168.2)],stroke='cotton_dark',width=.3)
    return im

def far_foot():
    im=Image.new('RGBA',(192*S,192*S))
    # Every path uses absolute coordinates. No translation of another foot image.
    sole=[('M',83.3,166.5),('C',83.1,164.2,84.6,162.7,87.1,163.1),('C',91.8,164.6,98.4,164.5,102.7,165.0),('C',104.9,165.3,106.6,166.5,106.5,168.2),('C',106.4,170.0,104.7,171.1,102.1,171.2),('L',89.3,171.2),('C',86.0,170.8,83.3,169.0,83.3,166.5)]
    path(im,sole,'straw','rope_dark',.4,True)
    sock=[('M',88.5,163.8),('C',89.9,164.5,92.9,164.6,94.3,163.9),('L',94.6,165.0),('C',97.0,164.4,100.6,164.6,103.5,165.5),('C',105.2,165.9,106.2,166.7,105.8,168.0),('C',104.7,169.4,99.8,169.8,96.4,169.5),('L',87.6,169.3),('C',85.4,168.9,84.0,167.7,84.2,165.3),('C',84.6,163.1,87.3,163.5,88.5,163.8)]
    path(im,sock,'sock','outline',.3,True)
    shade(im,[('M',85.4,167.5),('C',90.4,169.1,99.6,168.9,105.6,167.3),('L',105.1,169.2),('L',85.1,169.5)],'sock_shadow')
    shade(im,[('M',87.0,166.0),('C',88.6,165.1,89.7,165.7,90.0,166.5),('L',88.2,168.0),('L',86.3,167.3)],'sock_light')
    shade(im,[('M',100.5,166.3),('C',102.1,165.7,104.7,166.2,104.9,167.3),('L',103.7,168.0),('L',100.6,167.7)],'sock_light')
    # Retention at the heel is separate from the two transverse instep ropes.
    rope(im,[('M',84.1,168.5),('C',83.7,165.8,84.8,163.5,86.9,163.5),('C',88.0,163.6,88.6,165.0,88.0,166.1)],.65)
    rope(im,[('M',95.7,164.8),('C',94.8,166.0,94.1,168.0,93.8,169.6)],.82)
    rope(im,[('M',102.5,165.4),('C',101.4,166.6,100.9,168.1,100.6,169.6)],.8)
    rope(im,[('M',84.6,168.3),('C',88.9,171.2,101.4,171.4,106.0,168.6)],.82)
    band=[('M',88.5,163.4),('C',89.8,163.7,92.9,163.8,94.6,163.5),('L',94.5,165.2),('C',92.8,165.8,89.7,165.7,88.4,165.2),('L',88.5,163.4)]
    path(im,band,'indigo','indigo_dark',.28,True)
    path(im,[('M',88.8,164.0),('C',90.4,164.5,93.0,164.6,94.1,164.1)],stroke='indigo_mid',width=.23)
    path(im,[('M',88.7,164.9),('C',90.4,165.3,92.5,165.4,94.2,164.9)],stroke='#384762',width=.2)
    return im

def near_foot():
    im=Image.new('RGBA',(192*S,192*S))
    sole=[('M',87.8,172.0),('C',87.8,169.2,89.0,168.0,91.5,168.6),('C',97.5,170.0,102.1,169.8,107.0,170.5),('C',109.9,170.9,112.1,172.2,112.0,174.0),('C',111.8,175.8,109.2,176.7,106.4,176.7),('L',94.2,176.7),('C',90.3,176.4,87.8,174.5,87.8,172.0)]
    path(im,sole,'straw','rope_dark',.4,True)
    sock=[('M',93.5,169.4),('C',95.2,169.9,98.4,170.0,100.0,169.5),('L',100.3,170.4),('C',102.4,169.1,105.3,169.2,108.1,170.3),('C',110.3,170.8,111.7,172.4,111.1,174.1),('C',109.6,175.4,104.0,175.6,100.0,175.2),('L',92.8,175.0),('C',90.3,174.8,88.7,173.5,88.8,171.0),('C',89.0,168.7,91.5,168.5,93.5,169.4)]
    path(im,sock,'sock','outline',.3,True)
    shade(im,[('M',90.8,173.5),('C',95.5,175.1,105.1,175.1,110.5,173.3),('L',110.0,175.4),('L',90.1,175.2)],'sock_shadow')
    shade(im,[('M',92.2,172.1),('C',93.7,171.5,94.9,171.7,95.0,172.6),('L',93.6,174.0),('L',91.7,173.4)],'sock_light')
    shade(im,[('M',105.7,172.4),('C',107.3,171.8,109.6,172.4,109.9,173.3),('L',108.5,174.1),('L',105.8,173.8)],'sock_light')
    rope(im,[('M',88.7,174.0),('C',88.2,171.5,89.0,168.9,91.0,168.9),('C',92.1,168.9,92.5,170.2,92.2,171.5)],.65)
    rope(im,[('M',100.8,170.55),('C',99.6,172.0,99.0,173.8,98.6,175.3)],.85)
    rope(im,[('M',107.5,170.8),('C',106.3,172.4,105.8,174.0,105.5,175.4)],.82)
    rope(im,[('M',89.7,173.9),('C',94.4,176.8,106.4,176.8,111.6,174.4)],.82)
    band=[('M',93.5,169.2),('C',95.2,169.7,98.5,169.7,100.1,169.3),('L',100.0,170.9),('C',98.4,171.5,95.1,171.4,93.4,170.9),('L',93.5,169.2)]
    path(im,band,'indigo','indigo_dark',.28,True)
    path(im,[('M',93.8,169.9),('C',95.5,170.3,98.0,170.4,99.7,169.9)],stroke='indigo_mid',width=.25)
    path(im,[('M',93.8,170.7),('C',95.4,171.0,98.0,171.1,99.6,170.7)],stroke='#384762',width=.2)
    return im

def draw():
    OUT.mkdir(parents=True,exist_ok=True)
    far=far_pants();far.alpha_composite(far_foot())
    near=near_pants();near.alpha_composite(near_foot())
    legs=Image.alpha_composite(far,near)
    for name,im in [('far-unoccluded',far),('near-unoccluded',near),('painted-legs',legs)]:
        im.save(OUT/(name+'-16x.png'))
        im.resize((768,768),Image.Resampling.LANCZOS).save(OUT/(name+'-4x.png'))
    (OUT/'authoring-source.py').write_bytes(Path(__file__).read_bytes())
    print('Painted new absolute-coordinate paths; no existing artwork sampled or moved.')

if __name__=='__main__':draw()
