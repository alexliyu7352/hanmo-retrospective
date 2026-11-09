from pathlib import Path
from urllib.parse import urlencode
import subprocess, re
root=Path(__file__).parent
letters='眉眼锐长舒展鼻型直鼻圆鼻唇形薄唇丰唇表情平静微笑发冠束发布巾胡须无须短须衣身日常行旅查看合成人物头脸底层眉眼层鼻部层口部层胡须层发冠层衣身层五官方案恢复初始像素正在载入素材当前组合已更新图层保持原始位置失败共享状态暂未保存汉末人物组合样板'
url='https://fonts.googleapis.com/css2?'+urlencode({'family':'Noto Sans SC:wght@400','text':''.join(sorted(set(letters))),'display':'swap'})
r=subprocess.run(['curl','-fsSL','--max-time','25',url],capture_output=True,text=True,check=True)
(root/'font-source.css').write_text(r.stdout)
urls=re.findall(r'url\((https[^)]+)\)',r.stdout)
assert len(urls)==1
subprocess.run(['curl','-fsSL','--max-time','25',urls[0],'-o',str(root/'hanmo-labels.woff2')],check=True)
print('Label font saved', (root/'hanmo-labels.woff2').stat().st_size)
