"""生成汉末示范城域的精确结构图；所有坐标与变化均为设计示意。"""
from pathlib import Path
from html import escape
import json

ROOT = Path(__file__).parent
P = dict(paper='#f2ebdc', ink='#343c35', muted='#727263', road='#ded0b6', edge='#b8a88c',
         roof='#8a8d80', wall='#aea087', land='#e5dfcc', green='#bdc8a4', water='#a9c4c0',
         official='#94715c', private='#738776', commercial='#b88b52', military='#657b84', planned='#a35640')
LOCATIONS = [
    dict(id='office', name='郡府与官舍', point=[494,380], category='公事',
         detail='前堂办公，官舍位于同一院落后部。办理公事从府门通报；官署与任职者的私宅分开记录。',
         route='M650 746 V450 H494 V380'),
    dict(id='chen', name='陈宅 · 武将私宅', point=[775,318], category='私人拜访',
         detail='陈宅位于东里，与其他住户混居。这里只是已知住所，不保证主人当前在家；可通报、询问或留话。',
         route='M650 746 V450 H691 V386 H812 V334 H775 V318'),
    dict(id='doctor', name='医者居所', point=[861,354], category='专门服务',
         detail='医者在住宅中接诊。服务与经营者相连，不要求全城医者集中到一个功能分区。',
         route='M650 746 V450 H691 V386 H861 V354'),
    dict(id='du', name='杜宅 · 文士私宅', point=[440,578], category='私人拜访',
         detail='杜宅位于西里，与陈宅不在同一街。找人索引指向已知地址，移动与访问条件仍然生效。',
         route='M650 746 V616 H440 V578'),
    dict(id='tavern', name='市中酒肆', point=[765,551], category='商业',
         detail='市门进入市场后到达具体酒肆。这里可衔接已确认的酒肆内景；市场是一组场所，不是一个统一交易按钮。',
         route='M650 746 V650 H818 V557 H765 V551'),
    dict(id='warehouse', name='官仓与武库', point=[844,429], category='公有设施',
         detail='官仓与武库分别承担储粮和军械保管。位置靠近主要道路，使用权限、管理者和库存独立记录。',
         route='M650 746 V450 H844 V429'),
    dict(id='school', name='学舍与祠社', point=[390,685], category='专门场所',
         detail='以独立入口组织学习与地方活动。是否开放、由谁主持以及可进行何种活动，取决于该地点的实际内容。',
         route='M650 746 V616 H470 V694 H390 V685'),
    dict(id='dock', name='东郊码头', point=[1140,489], category='交通商业',
         detail='东门外道路连接河岸码头。城外仍属于同一城域；水运、货栈与集散活动围绕这里展开。',
         route='M650 746 V450 H1040 V489 H1140'),
    dict(id='camp', name='南郊军营', point=[493,790], category='军务',
         detail='军营与相邻练兵场位于南郊，城内仍保留守备与武库。进入营门需要相应条件。',
         route='M650 746 V756 H493 V790'),
    dict(id='workshop', name='西郊作坊', point=[180,486], category='生产',
         detail='作坊与住户、集散道路相联系。图中位置是示范选址，不表示汉代所有工坊必须设在城外。',
         route='M650 746 V450 H228 V486 H180'),
]
PROJECTS = [
    dict(id='P1', name='旧仓改作商铺', box=[489,631,110,43],
         before='城内旧仓', after='商铺院落',
         detail='沿用已有地块和入口，改变建筑用途。需处理原用途、产权与施工，不能只增加一项商业数值。'),
    dict(id='P2', name='军营扩建', box=[696,791,164,109],
         before='南郊扩建候选地', after='新增营舍',
         detail='从城南道路接入新营舍，保留原军营与练兵场。用地与建设条件满足后才能动工，建成后还需人员和供给。'),
    dict(id='P3', name='码头货栈与商铺', box=[1042,585,98,91],
         before='河岸候选地', after='货栈与商铺',
         detail='利用现有码头通路扩展商业。保留河岸装卸通道；需考虑用地、洪水与运输条件。'),
]


def text(x, y, value, size=22, fill=None, anchor='start', cls=''):
    """输出不依赖自动换行的可编辑文字，确保标签位置明确。"""
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{fill or P["ink"]}" text-anchor="{anchor}" class="{cls}">{escape(value)}</text>'


def path(d, stroke, width=2, fill='none', extra=''):
    """输出统一线条，调用方负责道路和院落的拓扑关系。"""
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round" {extra}/>'


def rect(x, y, w, h, fill, stroke=None, sw=1, extra=''):
    """输出地块或建筑外框，不为规划对象隐式添加功能。"""
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="{fill}" stroke="{stroke or fill}" stroke-width="{sw}" {extra}/>'


def roof(x, y, w, h, fill=None):
    """用平面屋顶符号表示建筑体量，避免把规划图冒充正式场景画。"""
    c=fill or P['roof']
    return rect(x,y,w,h,c,P['ink'],0.6)+path(f'M{x} {y+h/2} H{x+w}',P['paper'],1.2)+path(f'M{x} {y} L{x+8} {y+h/2} L{x} {y+h} M{x+w} {y} L{x+w-8} {y+h/2} L{x+w} {y+h}',P['ink'],0.6)


def road(d, width=18):
    """绘制有连续路面的道路；院落入口与访问路线使用同一组坐标。"""
    return path(d,P['edge'],width+2)+path(d,P['road'],width)


def marker(x,y,label,c=None):
    """输出编号地标；编号配合文字使类别不只依赖颜色辨认。"""
    return f'<circle cx="{x}" cy="{y}" r="16" fill="{c or P["ink"]}"/>'+text(x,y+6,label,18,P['paper'],'middle','map-number')


def make_map(built=False, interactive=False):
    """构造共用地形、路网与地块；建成态只替换三个规划地块的内容。"""
    out=[]
    out.append('<defs><clipPath id="map-clip"><rect width="1320" height="1010"/></clipPath><pattern id="field" width="15" height="15" patternUnits="userSpaceOnUse"><path d="M0 14H15" stroke="#a4ae8e" stroke-width="1"/></pattern><pattern id="plan-hatch" width="12" height="12" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><path d="M0 0V12" stroke="#a35640" stroke-width="1" opacity=".35"/></pattern></defs>')
    out.append('<g clip-path="url(#map-clip)">')
    out.append(rect(0,0,1320,1000,P['paper']))
    for d in ['M24 250 Q85 130 214 140 Q277 144 265 230','M40 270 Q105 170 207 176 Q245 180 250 220','M55 286 Q126 210 216 211']:
        out.append(path(d,P['green'],2))
    out.append(text(50,119,'北部缓坡',21,P['muted'],cls='minor-label'))
    river='M1187 -30 C1105 95 1160 230 1209 345 S1250 560 1185 715 S1150 910 1280 1040'
    out.append(path(river,P['water'],110))
    out.append(path(river,'#7fa6a2',1.5))
    out.append(text(1239,324,'河',25,'#557d78','middle','major-label'))
    out.append(text(1272,552,'道',25,'#557d78','middle','major-label'))
    for x,y,w,h in [(909,762,190,90),(924,865,179,98),(48,588,172,85),(46,686,182,66)]:
        out.append(rect(x,y,w,h,'#d5dcc2',P['green'],1));out.append(rect(x,y,w,h,'url(#field)'))
    out.append(text(992,830,'近郊田地',23,P['muted'],'middle','major-label'))
    out.append(text(120,641,'田圃',23,P['muted'],'middle','minor-label'))
    outline='M315 190 H875 L965 250 V670 L360 710 L280 620 V300 Z'
    out.append(path(outline,P['edge'],1,P['land']))
    # 主路连接四门；城外道路连续连接码头、作坊、军营和示范起点。
    for d,w in [('M650 84 V972',25),('M38 450 H1040 V489 H1170',25),('M1040 450 V715',18),('M650 756 H493 V790',18),('M300 842 H390',15),('M228 450 V486 H180',15),('M650 930 H923',16),('M650 756 H778 V791',16),('M1040 630 H1042',14)]:
        out.append(road(d,w))
    for d,w in [('M494 450 V380',17),('M650 450 H691 V386 H910',17),('M812 386 V244',12),('M812 334 H775 V318',10),('M861 386 V354',10),('M844 450 V429',12),('M650 616 H334',16),('M440 616 V578',12),('M470 616 V694 H390 V685',12),('M544 616 V631',12),('M650 650 H918',17),('M818 650 V557 H765 V551',13)]:
        out.append(road(d,w))
    # 郡府：前堂与后部官舍在同一院内，但用途分开。
    out.append(rect(365,246,247,134,'#e6d3b8',P['official'],3))
    out.append(roof(384,260,84,28,P['official'])+roof(508,260,84,28,P['official']))
    out.append(roof(401,320,178,31,P['official']))
    out.append(text(489,305,'官舍',18,P['ink'],'middle','minor-label'))
    out.append(text(489,370,'前堂 · 通报',18,P['ink'],'middle','minor-label'))
    out.append(rect(483,376,23,8,P['road']))
    out.append(marker(372,218,'01',P['official'])+text(396,226,'郡府与官舍',25,cls='major-label'))
    # 东里：武将与医者分属具体住户，不设置统一官员住宅区。
    out.append(rect(704,247,216,122,'#dde1cc',P['private'],1))
    out.append(road('M812 386 V256',12)+road('M812 334 H775 V318',10)+road('M861 386 V354',10))
    out.append(roof(739,272,72,28)+roof(829,306,66,29))
    out.append(roof(841,259,55,25)+roof(722,333,40,22))
    out.append(text(775,319,'陈宅',18,P['ink'],'middle','minor-label'))
    out.append(text(861,357,'医者居所',17,P['ink'],'middle','minor-label'))
    out.append(marker(718,218,'02',P['private'])+text(742,226,'东里',25,cls='major-label'))
    # 仓廪与军械储藏在城内，军事用途并非全部集中于城外。
    out.append(rect(755,397,167,34,'#d0d7d1',P['military'],1))
    out.append(roof(765,399,59,27,P['military'])+roof(859,399,52,27,P['military']))
    out.append(text(839,390,'03 官仓 · 武库',21,P['ink'],'middle','major-label'))
    # 西里、专门场所与旧仓共用可步行到达的街巷。
    out.append(rect(333,498,265,107,'#e0e3d2',P['private'],1))
    for x,y,w in [(350,518,57),(422,518,62),(508,518,66),(350,566,50),(519,565,56)]:out.append(roof(x,y,w,25))
    out.append(roof(414,548,53,27))
    out.append(road('M440 616 V578',12))
    out.append(text(440,598,'杜宅',18,P['ink'],'middle','minor-label'))
    out.append(marker(346,483,'04',P['private'])+text(370,490,'西里 · 居民与文士',23,cls='major-label'))
    out.append(rect(337,635,116,51,'#d8d4ba',P['edge'],1)+roof(345,640,43,24)+roof(400,640,44,24))
    out.append(text(392,679,'学舍／祠社',17,P['ink'],'middle','minor-label'))
    # 市内有多经营点，南侧市门和酒肆入口保持固定。
    out.append(rect(710,490,213,138,'#ebd8b2',P['commercial'],3))
    for x,y,w in [(733,515,62),(831,515,70),(733,565,45),(850,565,48)]:out.append(roof(x,y,w,27,P['commercial']))
    out.append(road('M818 650 V557 H765 V551',13))
    out.append(text(765,551,'酒肆',18,P['ink'],'middle','minor-label'))
    out.append(text(866,551,'布帛／器物',16,P['ink'],'middle','minor-label'))
    out.append(rect(807,624,24,8,P['road']))
    out.append(text(878,617,'市门',17,P['ink'],'middle','minor-label'))
    out.append(marker(725,475,'05',P['commercial'])+text(749,482,'市 · 多店铺经营',23,cls='major-label'))
    # 城墙使用四处真实缺口表达城门，访问路线不能穿墙。
    walls='M315 190 H623 M677 190 H875 L965 250 V425 M965 475 V670 L677 689 M623 693 L360 710 L280 620 V475 M280 425 V300 L315 190'
    out.append(path(walls,P['wall'],15)+path(walls,'#736e5d',2))
    for x,y,label in [(650,184,'北门'),(279,441,'西门'),(966,442,'东门'),(650,688,'南门')]:
        out.append(text(x,y-20 if label=='北门' else y+36 if label=='南门' else y-25,label,20,P['ink'],'middle','major-label'))
    # 城外设施与城内共用一套地图，不另造一座“军事城”。
    out.append(rect(90,378,121,144,'#e5d5bb',P['edge'],1))
    for x,y in [(105,395),(105,438),(105,480)]:out.append(roof(x,y,57,24,'#9d8871'))
    out.append(marker(96,352,'06',P['commercial'])+text(120,359,'西郊作坊',24,cls='major-label'))
    out.append(rect(1096,470,77,39,'#d7bd8c',P['edge'],1))
    out.append(path('M1156 474 H1220 M1156 488 H1220 M1156 502 H1220','#97764b',8))
    out.append(marker(1050,417,'07',P['commercial'])+text(1074,425,'东郊码头',24,cls='major-label'))
    out.append(rect(390,790,200,113,'#d1d9d4',P['military'],3))
    for x,y in [(406,809),(486,809),(406,860),(486,860)]:out.append(roof(x,y,66,25,P['military']))
    out.append(rect(481,786,24,8,P['road'])+rect(386,832,8,20,P['road']))
    out.append(marker(400,768,'08',P['military'])+text(425,775,'南郊军营',24,cls='major-label'))
    out.append(rect(99,790,201,112,'#d9d0b6',P['military'],2))
    for y in [816,842,868]:out.append(path(f'M125 {y} H274','#afa68d',1))
    out.append(text(199,859,'练兵场',27,P['ink'],'middle','major-label'))
    out.append(text(470,947,'前往乡聚与区域道路',23,P['muted'],'end','minor-label'))
    out.append(text(653,997,'城域边缘 · 路线接入世界地图',22,P['muted'],'middle','major-label'))
    # 规划态与建成示意态共用范围；切换只改变可变地块。
    for pr in PROJECTS:
        x,y,w,h=pr['box'];out.append(f'<g id="project-{pr["id"]}" class="project">')
        out.append(rect(x,y,w,h,'#eee1cb',P['planned'],2,extra='stroke-dasharray="7 5"'))
        out.append(f'<g class="before"'+(' style="display:none"' if built else '')+'>')
        if pr['id']=='P1':out.append(roof(x+10,y+8,85,25,'#a39c85'))
        else:out.append(rect(x+3,y+3,w-6,h-6,'url(#plan-hatch)'))
        out.append('</g><g class="after"'+('' if built else ' style="display:none"')+'>')
        if pr['id']=='P1':out.append(roof(x+6,y+7,43,27,P['commercial'])+roof(x+57,y+7,43,27,P['commercial']))
        elif pr['id']=='P2':
            for xx,yy in [(x+13,y+15),(x+92,y+15),(x+13,y+63),(x+92,y+63)]:out.append(roof(xx,yy,55,25,P['military']))
        else:
            out.append(roof(x+8,y+12,w-16,28,P['commercial'])+roof(x+8,y+57,34,22,P['commercial'])+roof(x+54,y+57,34,22,P['commercial']))
        out.append('</g>')
        label_x=x+w+6 if pr['id']!='P3' else x+8
        label_y=y+h/2+7 if pr['id']!='P3' else y-10
        out.append(text(label_x,label_y,pr['id'],20,P['planned'],'start','plan-label'))
        out.append('</g>')
    out.append('<g id="route-layer"></g>')
    out.append(f'<circle cx="650" cy="746" r="8" fill="{P["planned"]}" stroke="{P["paper"]}" stroke-width="3"/>')
    out.append(text(677,746,'示范来路',18,P['planned'],cls='minor-label'))
    out.append(path('M61 83 V32 M51 47 L61 32 L71 47',P['ink'],2)+text(61,20,'北',21,P['ink'],'middle','major-label'))
    out.append(text(864,110,'示范布局 · 不对应某座历史实城',21,P['muted'],'middle','major-label'))
    out.append('</g>')
    return ''.join(out)


def board(built=False):
    """生成可单独评审的地图与右侧图例，正文仍在 Page 中保持可编辑。"""
    state='建设后示意' if built else '现状与规划用地'
    body=[rect(0,0,1800,1240,P['paper']),text(50,64,'汉末城域结构规划',38),text(52,102,'示范郡城  /  v0.1  /  '+state,23,P['muted'])]
    body.append(text(1750,67,'城内外一体 · 访问与建设共用布局',22,P['muted'],'end'))
    body.append(path('M50 124 H1750',P['edge'],1))
    body.append('<g transform="translate(25 137)">'+make_map(built)+'</g>')
    body.append(path('M1373 156 V1120',P['edge'],1))
    x=1410
    body.append(text(x,193,'空间与用途',27))
    notes=[('01 郡府与官舍','公事到官署，私人拜访到住所。'),('02 / 04 居住里','武将、文士与普通住户分散居住。'),('03 官仓与武库','城内保留储粮、军械与守备设施。'),('05 市','一个市场容纳多个经营场所。'),('06 / 07 城外生产与交通','作坊、码头与城内道路连续。'),('08 南郊军营','训练空间与城内军务相互联系。')]
    yy=243
    for a,b in notes:
        body.extend([text(x,yy,a,23),text(x,yy+31,b,18,P['muted'])]);yy+=81
    body.extend([path(f'M{x} 725 H1750',P['edge'],1),text(x,769,'三处建设示例',27)])
    for yy,pr in zip([813,885,957],PROJECTS):
        body.extend([text(x,yy,pr['id']+' '+pr['name'],22,P['planned']),text(x,yy+28,pr['before']+' → '+pr['after'],18,P['muted'])])
    body.extend([text(x,1041,'实线：现有道路与建筑',20,P['muted']),text(x,1075,'朱砂虚线：规划改建或扩建范围',20,P['planned'])])
    body.extend([path('M50 1162 H1750',P['edge'],1),text(50,1199,'用途与位置为玩法设计提案；非历史复原、非最终场景美术。比例示意，未规定实际距离与移动耗时。',20,P['muted']),text(1750,1199,'待评审',22,P['planned'],'end')])
    return '<svg xmlns="http://www.w3.org/2000/svg" width="1800" height="1240" viewBox="0 0 1800 1240" font-family="Noto Sans SC, sans-serif"><title>汉末城域结构规划 '+state+'</title>'+''.join(body)+'</svg>'


def main():
    """写出两张共享布局的矢量图与预览数据，重复运行不改变城市坐标。"""
    for built,name in [(False,'city-plan'),(True,'city-plan-built')]:
        (ROOT/(name+'.svg')).write_text(board(built),encoding='utf-8')
    data={'locations':LOCATIONS,'projects':PROJECTS,'status':'proposal_not_reviewed','historical_reconstruction':False}
    (ROOT/'city-plan-data.json').write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    inner='<svg xmlns="http://www.w3.org/2000/svg" class="city-map" role="img" aria-labelledby="city-map-title city-map-desc" viewBox="0 0 1320 1015" font-family="Noto Sans SC,sans-serif"><title id="city-map-title">汉末示范城域规划</title><desc id="city-map-desc">城内官署、居住里、市场和仓储连接四门；城外有作坊、码头、军营、练兵场及三处规划用地。使用下方地点选择查看到达路线。</desc>'+make_map()+'</svg>'
    template=(ROOT/'preview.template.html').read_text(encoding='utf-8')
    fragment=template.replace('__MAP__',inner).replace('__DATA__',json.dumps(data,ensure_ascii=False))
    (ROOT/'city-plan.fragment.html').write_text(fragment,encoding='utf-8')
    full='<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>汉末城域结构规划</title><style>body{margin:0;padding:24px;background:#f2ebdc;color:#343c35;font:16px sans-serif}main{max-width:1250px;margin:auto}.viz-controls,.viz-row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}.form-label{display:flex;gap:8px;align-items:center}.btn,.form-select{font:inherit;padding:8px 12px;border:1px solid #b8a88c;background:#f2ebdc;color:#343c35}.btn[aria-pressed=true]{background:#343c35;color:#f2ebdc}.text-small{font-size:14px}p{line-height:1.6}</style><main>'+fragment+'</main></html>'
    (ROOT/'city-plan.html').write_text(full,encoding='utf-8')
    print(json.dumps({'locations':len(LOCATIONS),'projects':len(PROJECTS),'fragment_bytes':len(fragment.encode()),'static_states':2}))


if __name__=='__main__':
    main()
