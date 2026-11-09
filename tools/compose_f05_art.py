#!/usr/bin/env python3
"""Compose standalone room paintings and alpha characters from owned concept art; no UI pixels are retained."""
from pathlib import Path
import hashlib, json, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageOps, ImageEnhance
import cv2
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'game/assets/art/f05'
SRC = ROOT / 'art-source'
W, H = 1600, 1000
RNG = np.random.default_rng(505)

def crop(name, box, size):
    """Take an explicitly reviewed non-UI region and resample it as a background painting material."""
    return ImageOps.fit(Image.open(SRC/name).convert('RGB').crop(box), size, Image.Resampling.LANCZOS)

def texture(color, size):
    """Build a deterministic layered pigment texture, without any runtime simulation dependencies."""
    w, h = size
    fine = RNG.normal(0, 2.0, (h,w,1))
    coarse = Image.fromarray(RNG.integers(60,190,(max(2,h//30),max(2,w//30)),dtype=np.uint8))
    field = np.asarray(coarse.resize((w,h), Image.Resampling.BICUBIC),dtype=float)[...,None]
    values = np.clip(np.array(color)[None,None,:] + fine + (field-125)*0.12,0,255)
    return Image.fromarray(values.astype('uint8'),'RGB')

def panel(im, box, color):
    """Paint a textured rectangular surface, retaining material variation under the scene lighting."""
    x0,y0,x1,y1 = map(int,box)
    im.paste(texture(color,(max(1,x1-x0),max(1,y1-y0))),(x0,y0))

def wood(im, box, tone=(66,43,27)):
    """Paint wood with edge relief and tapered hand-drawn grain instead of flat blocking rectangles."""
    panel(im,box,tone); d=ImageDraw.Draw(im,'RGBA'); x0,y0,x1,y1=map(int,box)
    d.rectangle(box,outline=(19,16,12,210),width=3)
    d.line((x0+3,y0+2,x1-3,y0+2),fill=(207,156,89,120),width=2)
    for j in range(max(2,(y1-y0)//5)):
        y=y0+3+j*5
        pts=[(x,y+int(math.sin(x*.021+j)*2)) for x in range(x0+3,x1-2,12)]
        if len(pts)>1:d.line(pts,fill=(15,10,8,int(RNG.integers(15,65))),width=1)

def jar(im, x, y, scale=1.0, tone=(101,71,45)):
    """Paint a ceramic jar, with an elliptical rim, curved body, reflected light and contact shadow."""
    s=scale; d=ImageDraw.Draw(im,'RGBA')
    d.ellipse((x-44*s,y+18*s,x+51*s,y+40*s),fill=(8,8,6,90))
    pts=[(x-21*s,y-77*s),(x-18*s,y-57*s),(x-42*s,y-32*s),(x-44*s,y+11*s),(x-32*s,y+31*s),(x+29*s,y+32*s),(x+44*s,y+12*s),(x+42*s,y-32*s),(x+18*s,y-57*s),(x+21*s,y-77*s)]
    d.polygon(pts,fill=(*tone,255),outline=(38,29,21,255))
    d.arc((x-35*s,y-50*s,x+31*s,y+26*s),100,240,fill=(213,177,121,135),width=max(1,int(s*3)))
    d.ellipse((x-23*s,y-84*s,x+23*s,y-70*s),fill=(56,42,29,255),outline=(184,143,89,255),width=2)
    d.ellipse((x-16*s,y-81*s,x+16*s,y-75*s),fill=(26,25,19,255))

def sack(im, x, y, scale=1.0, seed=0):
    """Paint tied cloth grain sacks with woven lines and irregular seams."""
    s=scale; d=ImageDraw.Draw(im,'RGBA'); rng=np.random.default_rng(seed)
    d.ellipse((x-73*s,y+13*s,x+78*s,y+42*s),fill=(7,9,7,105))
    d.polygon([(x-49*s,y-66*s),(x-57*s,y-36*s),(x-72*s,y+8*s),(x-51*s,y+33*s),(x+58*s,y+29*s),(x+73*s,y+3*s),(x+52*s,y-53*s)],fill=(163,132,85,255),outline=(76,57,32,255))
    d.line([(x-48*s,y-57*s),(x-62*s,y+5*s),(x-42*s,y+26*s)],fill=(221,188,132,155),width=3)
    for _ in range(65):
        xx=x+float(rng.uniform(-42,45))*s; yy=y+float(rng.uniform(-39,21))*s
        d.line((xx,yy,xx+5*s,yy+1*s),fill=(92,72,44,60),width=1)

def floor(im, horizon=565):
    """Lay a perspective flagstone floor with painted wear and rafter shadows."""
    panel(im,(0,horizon,W,H),(99,84,59)); d=ImageDraw.Draw(im,'RGBA')
    for x in range(-1700,3400,250):
        end=800+(x-800)*0.30
        d.line((end,horizon,x,H),fill=(34,32,23,130),width=3)
        d.line((end+2,horizon,x+3,H),fill=(179,153,108,95),width=1)
    for y in [592,632,694,783,912]:
        d.line((0,y,W,y),fill=(31,30,22,130),width=3)
        d.line((0,y+3,W,y+3),fill=(188,158,105,80),width=1)
    d.polygon([(0,660),(570,565),(1230,1000),(350,1000)],fill=(233,204,130,24))

def window(im, box):
    """Paint an open lattice window using the project's own outdoor painting as the distant view."""
    x0,y0,x1,y1=box
    view=crop('wide_cinematic_game_ui_scene_of_a_historical_chine.png',(518,20,1085,227),(x1-x0,y1-y0))
    view=ImageEnhance.Color(view.filter(ImageFilter.GaussianBlur(1.1))).enhance(.6)
    im.paste(view,(x0,y0)); d=ImageDraw.Draw(im,'RGBA')
    d.rectangle(box,outline=(37,28,21,255),width=11)
    for x in range(x0+27,x1,31):d.line((x,y0,x,y1),fill=(58,43,27,235),width=4)
    for y in range(y0+33,y1,39):d.line((x0,y,x1,y),fill=(58,43,27,235),width=4)

def desk(im, y=712, left=230, right=1360):
    """Paint a low lacquer counter with perspective surface, joinery, grain and a contact shadow."""
    d=ImageDraw.Draw(im,'RGBA'); d.polygon([(left-30,y+33),(right+40,y+33),(right+110,y+172),(left-65,y+178)],fill=(4,9,8,85))
    wood(im,(left+20,y+19,left+55,y+150));wood(im,(right-57,y+17,right-20,y+145))
    d.polygon([(left+30,y-39),(right-30,y-39),(right,y+7),(left,y+7)],fill=(111,70,38,255),outline=(32,23,16,255))
    wood(im,(left,y+7,right,y+30),(66,40,25))
    d.line((left+15,y+2,right-15,y+2),fill=(220,162,84,170),width=3)

def room(kind):
    """Construct a complete place illustration from reviewed painting details, original furniture and lighting."""
    specs={
        'hall':('a_wide_collage_infographic_image_in_a_game_concept.png',(1046,48,1529,236)),
        'shop':('a_wide_collage_style_promotional_layout_for_a_chi.png',(294,410,488,542)),
        'tavern':('a_wide_collage_infographic_image_in_a_game_concept.png',(817,433,1025,617)),
        'inn':('a_wide_promotional_game_ui_collage_in_a_hand_paint.png',(9,701,163,800)),
        'house':('a_wide_promotional_game_ui_collage_in_a_hand_paint.png',(402,399,505,555))}
    name,box=specs[kind]; im=crop(name,box,(W,660))
    canvas=texture((67,57,43),(W,H));canvas.paste(im,(0,0));im=canvas
    floor(im,610); d=ImageDraw.Draw(im,'RGBA')
    wood(im,(0,0,W,46),(44,33,24));wood(im,(0,147,W,170),(60,39,24))
    for x in [68,1470]:wood(im,(x,0,x+48,850),(56,37,25))
    if kind in ['inn','house']:window(im,(535,178,1040,538))
    if kind=='hall':
        wood(im,(167,243,325,579));window(im,(179,252,313,554))
        for x in [1230,1296,1362]:
            for y in [354,422,490]:
                for j in range(8):d.line((x+j*5,y,x+j*5,y+43),fill=(168-j*3,134-j*2,85,220),width=4)
    if kind=='shop':
        for yy in [294,417,546]:
            wood(im,(150,yy+30,670,yy+47))
            for j in range(5):jar(im,205+j*92,yy,.65,(101+j*5,74+j*3,48))
        for j,(x,y,s) in enumerate([(1110,560,1.1),(1250,555,1.0),(1360,571,1.1),(1190,465,.95),(1328,462,.95),(1246,378,.8)]):sack(im,x,y,s,j)
    if kind=='tavern':
        for x,y,s in [(235,411,.65),(1235,377,.8),(1325,392,.72)]:jar(im,x,y,s,(116,69,39))
        for x in [255,1190]:desk(im,655,x,x+300)
        d.ellipse((288,593,350,615),fill=(191,165,116,255),outline=(53,38,27,255),width=3)
    if kind=='inn':
        wood(im,(155,590,678,765),(66,40,29));panel(im,(179,580,654,655),(75,105,100))
        for x in range(184,649,14):d.line((x,585,x+42,650),fill=(198,185,143,100),width=2)
        d.rounded_rectangle((205,567,353,615),radius=18,fill=(179,164,124,255))
        desk(im,710,1080,1370);jar(im,1265,674,.7)
    elif kind=='house':
        desk(im,706,280,1160)
        d.polygon([(590,649),(859,649),(875,686),(574,686)],fill=(212,186,134,255),outline=(81,60,37,255))
        for x in range(592,861,14):d.line((x,652,x-13,682),fill=(105,75,41,160),width=2)
        jar(im,1040,658,.65,(73,87,73))
    elif kind=='hall':
        desk(im,718,315,1325)
        for j in range(7):d.line((900+j*10,666,907+j*10,697),fill=(199,165,108,245),width=8)
        jar(im,1210,673,.44,(74,66,43))
    else:
        desk(im,754,420,1360)
        if kind=='shop':sack(im,1240,742,1.0,22)
        else:jar(im,1270,711,.5,(130,80,44))
    yy,xx=np.mgrid[:H,:W]; vignette=np.clip(1-.22*((xx-W*.48)/(W*.64))**2-.13*((yy-H*.46)/(H*.75))**2,.52,1)
    im=Image.fromarray(np.clip(np.asarray(im,dtype=float)*vignette[...,None],0,255).astype('uint8'))
    light=Image.new('RGBA',(W,H));ld=ImageDraw.Draw(light)
    ld.polygon([(1120,90),(1240,110),(800,1000),(350,1000)],fill=(233,198,123,22))
    im=Image.alpha_composite(im.convert('RGBA'),light.filter(ImageFilter.GaussianBlur(28))).convert('RGB')
    return im.filter(ImageFilter.GaussianBlur(.30))

# Contours are painting masks only; they do not define any gameplay person or identity.
CONTOURS={
 'official':[(94,0),(145,0),(184,52),(191,125),(177,205),(243,245),(295,384),(0,384),(0,268),(73,220),(73,158),(78,51)],
 'merchant':[(134,0),(200,0),(258,54),(270,116),(241,193),(279,225),(368,384),(0,384),(40,285),(100,216),(106,157),(108,70)],
}

def character(kind):
    """Isolate existing original character paint into a soft alpha cutout, retaining its source provenance."""
    source=ROOT/'game/assets/art'/f'{kind}.png'
    im=Image.open(source).convert('RGB').resize((384,384),Image.Resampling.LANCZOS)
    points=CONTOURS.get(kind,[(155,0),(236,0),(280,70),(279,188),(252,230),(340,286),(380,383),(8,383),(49,282),(119,232),(106,172),(120,55)])
    rough=Image.new('L',(384,384)); ImageDraw.Draw(rough).polygon(points,fill=255)
    alpha=np.asarray(rough).copy(); mask=np.where(alpha>0,cv2.GC_PR_FGD,cv2.GC_BGD).astype('uint8')
    # The central facial region is fixed foreground; no arbitrary opaque rectangle is shipped.
    mask[105:205,145:212]=cv2.GC_FGD
    bg=np.zeros((1,65),np.float64);fg=np.zeros((1,65),np.float64)
    cv2.grabCut(np.asarray(im),mask,None,bg,fg,3,cv2.GC_INIT_WITH_MASK)
    final=np.where((mask==cv2.GC_FGD)|(mask==cv2.GC_PR_FGD),255,0).astype('uint8')
    final=np.minimum(final,alpha)
    a=Image.fromarray(final).filter(ImageFilter.GaussianBlur(.65))
    im.putalpha(a)
    result=Image.new('RGBA',(420,440)); result.alpha_composite(im,(18,34))
    result.save(OUT/f'{kind}.png',optimize=True)

def main():
    """Create reproducible standalone assets and an explicit derivation manifest, never game screenshots."""
    OUT.mkdir(parents=True,exist_ok=True)
    city=crop('wide_cinematic_game_ui_scene_of_a_historical_chine.png',(362,1,1218,514),(W,H))
    city.save(OUT/'city.webp',quality=92,method=6)
    for kind in ['hall','shop','tavern','inn','house']:
        room(kind).save(OUT/f'{kind}.webp',quality=92,method=6)
    for kind in ['official','merchant','host','innkeeper','carter','guard','sender','recipient','player']:
        character(kind)
    records=[]
    for path in sorted(OUT.iterdir()):
        if path.suffix not in ['.png','.webp']:continue
        with Image.open(path) as image:
            records.append({'id':path.stem,'path':str(path.relative_to(ROOT/'game')),'size':list(image.size),'mode':image.mode,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    manifest={'version':'f05-art-1','method':'Original generated concept painting details composited with authored perspective furniture, pigment, light and alpha masks. Not raw screenshots, not historical evidence. Source resolution varies; output dimensions do not imply native source resolution.','generator_script':'tools/compose_f05_art.py','source_directory':'art-source (retained in Git, excluded from player build)','runtime_ui_baked_into_backgrounds':False,'assets':records}
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('ART_RESOURCES_WRITTEN',len(records))

if __name__=='__main__':main()
