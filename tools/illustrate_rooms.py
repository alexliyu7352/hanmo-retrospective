#!/usr/bin/env python3
"""Build original layered 2D room illustrations; no portraits or interface pixels are painted into them."""
from pathlib import Path
import math, json, hashlib
import numpy as np
from PIL import Image,ImageDraw,ImageFilter,ImageEnhance
from compose_f05_art import texture,wood,jar,sack,crop,desk
ROOT=Path(__file__).resolve().parents[1]; OUT=ROOT/'game/assets/art/f05'; W,H=1600,1000
RNG=np.random.default_rng(5052)

def surface(im, points, color):
    """Clip pigment noise to an architectural plane, with a restrained ink contour."""
    mask=Image.new('L',im.size);ImageDraw.Draw(mask).polygon(points,fill=255)
    im.paste(texture(color,im.size),(0,0),mask)
    ImageDraw.Draw(im).line(points+[points[0]],fill=(53,43,32),width=3)

def landscape(size):
    """Use only the unlettered landscape region of the project's original generated cover painting."""
    return crop('a_wide_promotional_game_ui_collage_in_a_hand_paint.png',(3,264,605,383),size)

def lattice(im, box, open_view=True):
    """Paint a traditional window with scenery, joinery and a broad recessed frame."""
    x0,y0,x1,y1=map(int,box); d=ImageDraw.Draw(im,'RGBA')
    d.rectangle((x0-9,y0-9,x1+9,y1+9),fill=(51,37,26,255))
    inner=landscape((x1-x0,y1-y0)) if open_view else texture((159,151,120),(x1-x0,y1-y0))
    im.paste(inner,(x0,y0));d=ImageDraw.Draw(im,'RGBA')
    for x in range(x0,x1+1,38):d.line((x,y0,x,y1),fill=(64,48,32,240),width=6)
    for y in range(y0,y1+1,48):d.line((x0,y,x1,y),fill=(69,52,34,240),width=6)
    d.rectangle(box,outline=(112,81,45,255),width=8)

def architecture(kind):
    """Lay a fully independent timber room with a coherent vanishing point and no baked inhabitants."""
    im=texture((181,163,126),(W,H));d=ImageDraw.Draw(im,'RGBA')
    surface(im,[(0,0),(W,0),(1340,163),(260,163)],(79,59,41))
    surface(im,[(0,0),(260,163),(260,610),(0,887)],(126,109,78))
    surface(im,[(W,0),(1340,163),(1340,610),(W,887)],(121,98,66))
    surface(im,[(0,887),(260,610),(1340,610),(W,887),(W,H),(0,H)],(131,124,100))
    d=ImageDraw.Draw(im,'RGBA')
    for y in [628,655,695,752,830,934]:d.line((0,y,W,y),fill=(75,74,60,110),width=3)
    for x in range(-1600,3200,250):d.line((800+(x-800)*.26,610,x,H),fill=(81,78,62,130),width=2)
    for y in range(190,593,115):wood(im,(260,y,1340,y+12),(97,69,41))
    for x in [260,498,795,1092,1309]:wood(im,(x,153,x+30,627),(93,62,37))
    for x in [72,1490]:wood(im,(x,0,x+39,914),(75,49,32))
    for y in [35,79,122]:wood(im,(125,y,1475,y+20),(85,55,32))
    d=ImageDraw.Draw(im,'RGBA')
    for x in range(175,1500,145):
        d.polygon([(x,0),(x+25,0),(800+(x-800)*.72,163),(781+(x-800)*.72,163)],fill=(51,38,27,235))
    d.polygon([(260,610),(1340,610),(1450,690),(150,690)],fill=(194,166,110,18))
    # Tall framed panels are separate from scene text and never contain UI labels.
    lattice(im,(291,216,476,526),kind!='shop')
    if kind!='hall':lattice(im,(1122,216,1278,526),kind in ['inn','house'])
    return im

def scroll(im, x, y, width=160, height=240):
    """Paint a silk landscape hanging as an object, with rollers and restrained ink mountain silhouettes."""
    d=ImageDraw.Draw(im,'RGBA');d.rectangle((x-8,y-6,x+width+8,y+height+6),fill=(75,76,59,255))
    im.paste(texture((200,184,145),(width,height)),(x,y));d=ImageDraw.Draw(im,'RGBA')
    d.polygon([(x,y+height*.77),(x+width*.32,y+height*.34),(x+width*.51,y+height*.67),(x+width*.70,y+height*.23),(x+width,y+height*.73)],fill=(101,111,95,180))
    for yy in [y-4,y+height]:d.line((x-15,yy,x+width+15,yy),fill=(59,41,27,255),width=9)

def cloth_bag(im,x,y,s=1.0):
    """Render a rounded woven sack using convex edges, folds and light, not a flat hexagonal marker."""
    layer=Image.new('RGBA',im.size);d=ImageDraw.Draw(layer,'RGBA')
    d.ellipse((x-81*s,y+10*s,x+90*s,y+44*s),fill=(21,20,14,80))
    d.rounded_rectangle((x-78*s,y-60*s,x+73*s,y+29*s),radius=int(27*s),fill=(174,145,96,255),outline=(86,68,39,255),width=2)
    d.arc((x-68*s,y-53*s,x+67*s,y+19*s),180,300,fill=(222,193,140,180),width=max(2,int(s*3)))
    for n in range(8):
        xx=x-58*s+n*16*s
        d.line([(xx,y-41*s),(xx+8*s,y-3*s),(xx+3*s,y+18*s)],fill=(96,80,48,45),width=1)
    for n in range(5):d.arc((x-63*s,y-44*s+n*12*s,x+58*s,y+1*s+n*12*s),5,168,fill=(107,90,54,40),width=1)
    d.ellipse((x-28*s,y-78*s,x+27*s,y-38*s),fill=(161,127,79,255),outline=(83,65,38,255),width=2)
    d.line((x-26*s,y-46*s,x+24*s,y-48*s),fill=(76,54,30,235),width=4)
    im.paste(layer,(0,0),layer)

def bowl(im,x,y,s=1.0):
    """Paint an earthenware bowl with a rim, inner glaze and grounded shadow."""
    d=ImageDraw.Draw(im,'RGBA');d.ellipse((x-30*s,y+6*s,x+33*s,y+24*s),fill=(19,21,14,100))
    d.pieslice((x-28*s,y-17*s,x+28*s,y+25*s),0,180,fill=(88,95,67,255))
    d.ellipse((x-29*s,y-13*s,x+29*s,y+9*s),fill=(151,161,119,255),outline=(48,60,48,255),width=2)
    d.ellipse((x-21*s,y-8*s,x+21*s,y+4*s),fill=(87,100,78,255))

def lamp(im,x,y):
    """Paint a suspended fabric lantern whose warm glow stays independent of game-time rules."""
    d=ImageDraw.Draw(im,'RGBA');d.line((x,80,x,y-38),fill=(53,40,25,255),width=3)
    d.rounded_rectangle((x-31,y-43,x+31,y+47),radius=17,fill=(205,158,79,255),outline=(86,62,29,255),width=3)
    for dx in [-20,-9,9,20]:d.line((x+dx,y-38,x+dx,y+42),fill=(147,105,44,160),width=2)
    d.line((x-23,y-41,x+23,y-41),fill=(66,46,25,255),width=5);d.line((x-23,y+45,x+23,y+45),fill=(66,46,25,255),width=5)

def decorate(im,kind):
    """Give each place its own readable work objects and spatial purpose, leaving middle ground for NPCs."""
    d=ImageDraw.Draw(im,'RGBA')
    if kind=='hall':
        scroll(im,654,224,290,285)
        for yy in [295,393,490]:
            wood(im,(1112,yy+44,1290,yy+61),(81,56,35))
            for j in range(15):
                x=1123+j*10
                d.rounded_rectangle((x,yy,x+7,yy+43),radius=2,fill=(161+j%4*12,127+j%3*8,74,255),outline=(67,54,30,200),width=1)
        desk(im,738,435,1315)
        for j in range(14):d.line((882+j*8,691,892+j*8,719),fill=(195,165,113,255),width=6)
        bowl(im,1173,700,.75);jar(im,301,696,1.1,(86,100,79))
    elif kind=='shop':
        for yy in [288,400,512]:
            wood(im,(548,yy+34,1045,yy+51),(81,54,29))
            for j in range(6):jar(im,581+j*82,yy,.68,(97+j*9,76+j*5,46+j*3))
        for x,y,s in [(1185,570,1.0),(1330,590,1.1),(1218,476,.95),(1325,491,.85),(1267,393,.85)]:cloth_bag(im,x,y,s)
        desk(im,766,254,982);cloth_bag(im,359,733,.85);bowl(im,819,731,.8)
        wood(im,(621,587,629,708),(99,76,41));d.line((568,607,682,589),fill=(55,59,44,255),width=5)
        for xx,yy in [(568,607),(682,589)]:
            d.line((xx,yy,xx-17,yy+55),fill=(97,91,62,255),width=2);d.line((xx,yy,xx+17,yy+55),fill=(97,91,62,255),width=2)
            d.arc((xx-24,yy+44,xx+24,yy+67),0,180,fill=(116,109,69,255),width=5)
    elif kind=='tavern':
        lamp(im,574,255);lamp(im,1004,247)
        for yy in [392,523]:
            wood(im,(547,yy+30,1048,yy+49),(85,55,33))
            for j in range(5):jar(im,599+j*98,yy,.69,(117,77,43))
        for x,y,s in [(161,780,.9),(1105,768,.9),(495,903,1.0)]:
            desk(im,y,x,x+360);bowl(im,x+115,y-29,s);bowl(im,x+257,y-17,s*.78);jar(im,x+202,y-35,.46,(109,65,36))
    elif kind=='inn':
        wood(im,(545,570,1035,731),(85,58,41));wood(im,(558,730,585,820));wood(im,(989,729,1018,820))
        surface(im,[(545,570),(929,558),(1035,641),(631,657)],(86,123,111))
        for j in range(25):d.line((557+j*16,576,640+j*16,648),fill=(184,178,135,95),width=1)
        d.rounded_rectangle((573,559,716,597),radius=14,fill=(213,199,160,255),outline=(128,119,83,255),width=2)
        for x in [526,1018]:
            d.polygon([(x,179),(x+45,181),(x+34,465),(x+62,672),(x+4,674),(x-7,430)],fill=(117,83,61,220),outline=(60,45,30,210))
            d.line((x+10,191,x+20,427,x+8,659),fill=(169,129,92,190),width=3)
        desk(im,776,1145,1400);jar(im,1260,735,.58,(86,105,89));bowl(im,1340,744,.7)
        scroll(im,717,243,133,206)
    elif kind=='house':
        lattice(im,(550,222,1038,514),True)
        desk(im,759,422,1190);scroll(im,1162,246,111,238)
        for j in range(17):d.line((585+j*8,710,600+j*8,739),fill=(184,154,102,255),width=6)
        jar(im,1089,714,.64,(70,94,75));bowl(im,916,724,.8)
        wood(im,(155,657,332,769),(92,65,39));cloth_bag(im,205,650,.73)
    # A restrained shadow/brush texture makes architecture sit behind the detailed character paintings.
    yy,xx=np.mgrid[:H,:W];light=np.clip(1.06-.20*((xx-710)/1060)**2-.09*((yy-450)/730)**2,.64,1.02)
    im=Image.fromarray(np.clip(np.asarray(im,dtype=float)*light[...,None],0,255).astype('uint8'))
    glaze=Image.new('RGBA',(W,H));gd=ImageDraw.Draw(glaze)
    gd.polygon([(292,520),(475,524),(1120,1000),(672,1000)],fill=(245,212,142,35))
    im=Image.alpha_composite(im.convert('RGBA'),glaze.filter(ImageFilter.GaussianBlur(21))).convert('RGB')
    return im.filter(ImageFilter.GaussianBlur(.5))

def main():
    """Write independently usable room textures and update the asset manifest with truthful provenance."""
    for kind in ['hall','shop','tavern','inn','house']:
        decorate(architecture(kind),kind).save(OUT/(kind+'.webp'),quality=94,method=6)
    path=OUT/'manifest.json';m=json.loads(path.read_text())
    m['room_method']='Original deterministic illustrated timber interiors, authored perspective/material/object layers; only window landscape uses a text-free crop from original generated cover art. No interface or NPC baked into rooms. These are development illustrations, not historical reconstructions or externally commissioned paintings.'
    m['generator_script']='tools/illustrate_rooms.py; tools/refine_f05_art.py'
    for record in m['assets']:
        p=ROOT/'game'/record['path']
        with Image.open(p) as im:record.update(size=list(im.size),mode=im.mode)
        record['sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
    path.write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n')
    print('INDEPENDENT_ROOM_ILLUSTRATIONS=5')
if __name__=='__main__':main()
