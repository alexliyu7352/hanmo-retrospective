#!/usr/bin/env python3
"""Refine owned paintings into complete silhouettes; masks never cut facial features or include UI."""
from pathlib import Path
import json, hashlib
from PIL import Image, ImageDraw, ImageFilter, ImageEnhance, ImageOps
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'game/assets/art/f05'
ART=ROOT/'game/assets/art'
CONTOURS={
'official':[(67,0),(115,0),(125,23),(145,56),(146,131),(135,165),(158,182),(182,200),(219,299),(0,299),(0,209),(43,173),(54,145),(43,110),(45,67),(62,36)],
'merchant':[(153,5),(181,1),(215,15),(233,48),(234,75),(242,101),(238,146),(222,166),(245,177),(276,195),(299,299),(29,299),(42,235),(69,201),(125,169),(124,147),(113,137),(112,92),(124,53)],
'host':[(111,0),(143,0),(162,14),(181,23),(197,43),(205,81),(202,145),(185,193),(205,209),(229,223),(250,299),(5,299),(23,242),(79,202),(85,179),(87,157),(67,141),(61,98),(62,69),(70,35),(102,18)],
'innkeeper':[(73,0),(147,0),(181,23),(192,62),(190,113),(169,145),(160,167),(181,186),(210,201),(241,250),(183,260),(181,299),(0,299),(0,220),(70,189),(77,171),(43,146),(39,120),(35,70),(44,32)],
'carter':[(98,61),(127,53),(145,57),(156,48),(174,55),(187,79),(197,110),(202,148),(188,187),(220,201),(240,248),(266,247),(275,236),(289,240),(298,268),(298,299),(0,299),(9,264),(19,236),(43,207),(93,184),(81,154),(79,116),(91,88)],
'recipient':[(185,0),(256,0),(286,20),(299,49),(299,293),(83,293),(99,240),(123,218),(183,196),(187,177),(171,153),(162,113),(159,69),(167,29)],
'player':[(62,3),(144,12),(178,47),(232,91),(251,104),(272,146),(281,171),(274,200),(266,261),(239,299),(61,299),(62,275),(24,264),(16,222),(5,198),(2,102),(35,72)]}

def cutout(kind, image, points):
    """Use a solid reviewed silhouette, preserving every interior pixel including both eyes and mouth."""
    image=image.convert('RGBA').resize((600,600),Image.Resampling.LANCZOS)
    from rembg import remove
    from scipy import ndimage
    import numpy as np
    cut=remove(image,session=SESSION)
    raw=np.asarray(cut.getchannel('A'))
    connected,n=ndimage.label(raw>90)
    sizes=np.bincount(connected.ravel());sizes[0]=0
    silhouette=ndimage.binary_fill_holes(connected==int(sizes.argmax()))
    mask=Image.fromarray((silhouette.astype('uint8')*255))
    if kind in ['merchant','innkeeper','carter','recipient','player']:
        mask=Image.new('L',(600,600));ImageDraw.Draw(mask).polygon([(x*2,y*2) for x,y in points],fill=255)
    if kind=='innkeeper':ImageDraw.Draw(mask).rectangle((365,515,599,599),fill=0)
    if kind=='recipient':ImageDraw.Draw(mask).rectangle((0,585,599,599),fill=0)
    image.putalpha(mask.filter(ImageFilter.GaussianBlur(.65)))
    box=mask.getbbox(); image=image.crop(box)
    image.thumbnail((520,550),Image.Resampling.LANCZOS)
    canvas=Image.new('RGBA',(560,590));canvas.alpha_composite(image,((560-image.width)//2,580-image.height))
    canvas.save(OUT/(kind+'.png'),optimize=True)

def main():
    """Write reviewed portraits and record their actual source scale, not fictional native-HD claims."""
    for kind,points in CONTOURS.items(): cutout(kind,Image.open(ART/(kind+'.png')),points)
    source=Image.open(ROOT/'art-source/a_wide_promotional_game_ui_collage_in_a_hand_paint.png')
    guard=source.crop((184,405,363,575))
    gp=[(112,0),(188,0),(195,37),(190,80),(188,117),(172,152),(165,179),(208,198),(264,230),(299,299),(0,299),(24,245),(74,218),(118,185),(116,153),(99,124),(103,86),(105,51)]
    cutout('guard',guard,gp)
    sender=source.crop((735,428,930,575))
    sp=[(93,2),(166,1),(187,20),(194,61),(204,80),(197,119),(182,139),(161,162),(193,178),(252,193),(299,249),(299,299),(0,299),(6,258),(25,220),(75,183),(94,165),(84,142),(82,102),(84,58)]
    cutout('sender',sender,sp)
    manifest=json.loads((OUT/'manifest.json').read_text())
    manifest['portrait_method']='U2NetP segmentation of owned original concept images, largest silhouette and filled internal holes, followed by manual exclusion of leftover UI and visual review. Earlier source resolution varies; resized placement is not native-HD detail.'
    for item in manifest['assets']:
        path=ROOT/'game'/item['path']
        with Image.open(path) as im:item.update(size=list(im.size),mode=im.mode)
        item['sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
    (OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n')
    print('PORTRAITS_REFINED',9)
if __name__=='__main__':
    import os
    os.environ.setdefault('U2NET_HOME','<本地路径>')
    os.environ.setdefault('NUMBA_CACHE_DIR','<本地路径>')
    os.environ.setdefault('OMP_NUM_THREADS','2')
    from rembg import new_session
    SESSION=new_session('u2netp',providers=['CPUExecutionProvider'])
    main()
