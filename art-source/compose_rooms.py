from pathlib import Path
from PIL import Image,ImageDraw,ImageFilter,ImageFont
import numpy as np, math, json
root=Path(__file__).resolve().parents[1];out=root/'game/assets/art'; W,H=1280,640
rng=np.random.default_rng(104)
# All scenery below is an original rendered blocking study. No existing game's scene or UI is included.
def scene(kind):
 """Compose a temporary empty room with original geometry and lighting."""
 y,x=np.mgrid[:H,:W]; light=np.exp(-(((x-940)/780)**2+((y-160)/410)**2))
 noise=rng.normal(0,2.2,(H,W))
 tones=np.stack([70+75*light+noise,62+62*light+noise,48+43*light+noise],axis=-1).clip(0,255).astype('uint8')
 im=Image.fromarray(tones);d=ImageDraw.Draw(im)
 # Left wall and receding ceiling define a complete room rather than magnifying a photographic fragment.
 d.polygon([(0,0),(250,120),(250,420),(0,610)],fill=(60,58,46));d.polygon([(0,0),(W,0),(1100,110),(250,120)],fill=(54,46,34))
 d.polygon([(0,610),(250,405),(1110,405),(1280,525),(1280,640),(0,640)],fill=(124,99,66))
 for i in range(-8,16):
  a=(630+i*55,405);b=(640+i*150,640);d.line([a,b],fill=(96,75,47),width=2)
 for yy in [430,461,502,555,625]:d.line([(0,yy),(W,yy)],fill=(91,72,49),width=2)
 # Wooden beam grain and several broad support columns.
 for bx in [30,246,1085,1230]:
  d.polygon([(bx,0),(bx+32,4),(bx+28,590),(bx-3,594)],fill=(60,39,23));d.line([(bx+6,8),(bx+3,584)],fill=(118,81,46),width=3)
  for _ in range(10):
   xx=bx+int(rng.integers(3,26));d.line([(xx,8),(xx-2,580)],fill=(77+int(rng.integers(0,15)),52,29),width=1)
 for yy in [83,115]:d.line([(0,yy-55),(246,yy),(1115,yy-3),(1280,yy-44)],fill=(52,35,22),width=17)
 # Translucent slatted window.
 d.rectangle((815,140,1040,354),fill=(180,170,121));d.rectangle((827,153,1028,342),fill=(168,177,141))
 for vx in range(827,1035,28):d.line((vx,151,vx,344),fill=(64,58,38),width=5)
 for vy in range(168,343,33):d.line((825,vy,1029,vy),fill=(69,57,35),width=4)
 # Sunlight across floor.
 layer=Image.new('RGBA',(W,H)); ld=ImageDraw.Draw(layer)
 ld.polygon([(822,353),(1030,353),(721,631),(310,631)],fill=(244,215,140,31));im=Image.alpha_composite(im.convert('RGBA'),layer);d=ImageDraw.Draw(im)
 def table(cx,cy,ww):
  """Place a table in the room perspective."""
  d.ellipse((cx-ww*.55,cy+40,cx+ww*.55,cy+80),fill=(47,39,27,100))
  d.polygon([(cx-ww/2,cy),(cx+ww/2,cy),(cx+ww*.62,cy+25),(cx-ww*.62,cy+25)],fill=(140,98,47))
  d.rectangle((cx-ww*.58,cy+25,cx+ww*.58,cy+36),fill=(83,49,24))
  for xx in [cx-ww*.5,cx+ww*.5]:d.polygon([(xx,cy+30),(xx+14,cy+30),(xx+10,cy+89),(xx-4,cy+89)],fill=(64,40,22))
 def jar(cx,cy,s=1):
  """Place a pottery silhouette."""
  d.ellipse((cx-19*s,cy-34*s,cx+19*s,cy+9*s),fill=(104,79,45));d.rectangle((cx-11*s,cy-42*s,cx+11*s,cy-27*s),fill=(92,70,41));d.ellipse((cx-12*s,cy-45*s,cx+12*s,cy-37*s),fill=(38,38,28));d.arc((cx-16*s,cy-30*s,cx+12*s,cy+4*s),100,245,fill=(167,136,84),width=2)
 def sack(cx,cy,s=1):
  """Place a stylized grain sack."""
  d.ellipse((cx-47*s,cy-24*s,cx+47*s,cy+18*s),fill=(150,126,81));d.arc((cx-41*s,cy-23*s,cx+39*s,cy+15*s),185,355,fill=(194,166,106),width=3)
  d.line((cx-40*s,cy+5*s,cx+40*s,cy+5*s),fill=(122,96,59),width=2)
 if kind=='hall':
  d.rectangle((490,151,629,327),fill=(166,147,107));d.line((485,148,637,148),fill=(65,48,28),width=8);d.line((485,330,637,330),fill=(65,48,28),width=8)
  # Decorative bamboo-scroll lines, not fake readable historical text.
  for xx in range(514,617,19):d.line((xx,169,xx,302),fill=(116,109,77),width=2)
  table(627,452,480)
  for xx in [430,459,486]:d.rectangle((xx,425,xx+19,452),fill=(179,159,109))
  jar(758,437,.75)
 elif kind=='shop':
  d.rectangle((366,161,771,407),fill=(45,40,27))
  for yy in [203,276,355,405]:d.rectangle((356,yy,779,yy+11),fill=(97,69,34))
  for xx in [354,555,770]:d.rectangle((xx,155,xx+13,411),fill=(73,48,23))
  for yy in [189,260,339]:
   for xx in [410,505,610,712]:sack(xx,yy,.87)
  for yy in [468,513]:
   for xx in [948,1046,1150]:sack(xx,yy,1.08)
  table(520,518,435);d.ellipse((435,500,484,514),fill=(61,44,25));jar(700,502,.8)
 elif kind=='tavern':
  d.rectangle((350,150,670,333),fill=(42,38,25))
  for yy in [224,319]:
   d.rectangle((340,yy,678,yy+9),fill=(115,80,41))
   for xx in [378,449,522,598]:jar(xx,yy-12,.83)
  for cx,cy,ww in [(390,458,240),(796,440,180),(1025,530,200)]:
   table(cx,cy,ww);jar(cx+14,cy-4,.65);d.ellipse((cx-54,cy-9,cx-20,cy+1),fill=(191,176,119))
 elif kind in ['inn','house']:
  d.polygon([(695,460),(1044,435),(1202,555),(771,577)],fill=(103,62,27))
  d.polygon([(711,449),(1032,432),(1170,539),(772,556)],fill=(163,148,106))
  d.polygon([(729,454),(1053,445),(1082,479),(754,497)],fill=(204,193,151))
  for xx in range(795,1100,17):d.line([(xx,507),(xx+15,546)],fill=(131,129,90),width=3)
  table(382,486,200);jar(389,477,.8)
  if kind=='house':
   d.rectangle((358,148,598,267),fill=(174,154,115))
   for j in range(4):d.line((372,172+j*21,568,172+j*21),fill=(139,129,96),width=2)
 # Grain creates material texture, not a platform-specific filter.
 ar=np.asarray(im.convert('RGB')).astype(np.float32); grain=rng.normal(0,1.4,(H,W,1)); ar=np.uint8(np.clip(ar+grain,0,255))
 Image.fromarray(ar).save(out/(kind+'.webp'),quality=92)
for kind in ['hall','shop','tavern','inn','house']:scene(kind)
# Only this crop is a large generated street illustration; all poster UI remains outside the crop.
src=root/'art-source/wide_cinematic_game_ui_scene_of_a_historical_chine.png'
im=Image.open(src).convert('RGB').crop((362,1,1218,514)).resize((1280,640),Image.Resampling.LANCZOS)
d=ImageDraw.Draw(im); # Replace the generated gate-name patch, not factual historical geography.
d.rectangle((552,258,668,292),fill=(104,107,107))
fontpath='/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc'
if Path(fontpath).exists():d.text((581,257),'青川',font=ImageFont.truetype(fontpath,23),fill=(224,215,186))
im.save(out/'city.webp',quality=93)
# Use silhouette-like, soft alpha edges for temporary busts rather than rectangular cards.
for path in out.glob('*.png'):
 im=Image.open(path).convert('RGBA');mask=Image.new('L',im.size);d=ImageDraw.Draw(mask)
 d.polygon([(137,7),(246,7),(297,64),(299,181),(350,263),(383,383),(0,383),(30,269),(82,187),(87,67)],fill=255)
 mask=mask.filter(ImageFilter.GaussianBlur(11));im.putalpha(mask);im.save(path)
m=json.loads((out/'manifest.json').read_text())
for bg in m['backgrounds']:
 if bg['id']!='city':bg.update(source='original programmatic room blocking, hand-composed geometry',crop=None,note='Temporary spatial blocking study, not a final generated painting. No NPC baked into room backdrop.')
 else:bg.update(source=src.name,crop=[362,1,1218,514],note='Generated street illustration crop; original poster UI excluded. Generic street extras are background scenery, not named interactable actors.')
(out/'manifest.json').write_text(json.dumps(m,ensure_ascii=False,indent=2)+'\n')
