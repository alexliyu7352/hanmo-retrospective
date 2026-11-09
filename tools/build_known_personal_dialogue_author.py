"""原真实生育条款与两个已安装成年人；不生成未来人物、亲缘认可或控制许可。"""
import argparse,copy,json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
 source=repo/'content/packs/authored/family_birth_1_0';target=repo/'content/packs/authored/known_personal_dialogues_1_0'
 pack=json.loads((source/'pack.json').read_text());content=json.loads((source/'content.json').read_text())
 pack.update(id='hanmo.known_personal_dialogues',version='1.0.0',namespace='known_talk');content.update(id='known_personal_dialogues',version='1.0.0')
 for obj in (pack,content):obj['required_capabilities'].append('known_personal_dialogues_v1')
 scene=content['scenarios'][0];old=scene['id'];scene['id']='scenario.qingchuan_known_dialogues';scene['enabled']['scenarios']=[scene['id'] if v==old else v for v in scene['enabled']['scenarios']];scene['name_key']='known_talk.name'
 provider={'kind':'installed_actor','source_id':'society::actor.home_keeper'};listener={'kind':'installed_actor','source_id':'society::actor.city_steward'}
 scene['dynamic_world']['known_personal_dialogues']={'version':1,'templates':[{'id':'installed_adult','dialogue_id':'dialog.known_adult','listener':listener,'provider':provider},{'id':'born_adult','dialogue_id':'dialog.known_born','listener':copy.deepcopy(listener),'provider':{'kind':'born_prototype','source_id':'actor.newborn_prototype'}}]}
 for id in ['dialog.known_adult','dialog.known_born']:
  scene['enabled']['dialogues'].append(id);content['dialogues'].append({'id':id,'context_role':'','entry':'hello','nodes':[
   {'id':'hello','kind':'say','speaker':'$provider','text_key':'known_talk.hello','next':'choose'},
   {'id':'choose','kind':'choice','text_key':'known_talk.choose','options':[{'id':'statement','text_key':'known_talk.ask','conditions':[],'next':'statement'},{'id':'withdraw','text_key':'known_talk.withdraw','conditions':[],'next':'withdraw'},{'id':'leave','text_key':'known_talk.leave','conditions':[],'next':'end'}]},
   {'id':'statement','kind':'say','speaker':'$provider','text_key':'known_talk.statement','next':'choose'},
   {'id':'withdraw','kind':'say','speaker':'$provider','text_key':'known_talk.no','next':'choose'},
   {'id':'end','kind':'end'}]})
 content['texts'].update({'known_talk.name':'青川·当面私人交谈','known_talk.hello':'我在这里，有话请当面对我说。','known_talk.choose':'听取本人当前声明','known_talk.ask':'询问本人是否愿意当面核对明确条款','known_talk.withdraw':'询问本人是否要撤回原声明','known_talk.leave':'结束交谈','known_talk.statement':'我愿意现在当面核对你提出的明确条款；具体事情仍须逐项办理。','known_talk.no':'我撤回之前的声明。请重新听取我的决定。'})
 emit(target/'pack.json',pack);emit(target/'content.json',content)
 emit(repo/'content/packs/selections/known-personal-dialogues-1.0.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.political_economy','version':'1.2.0'},{'id':'hanmo.known_personal_dialogues','version':'1.0.0'}]})
 (target/'README.md').write_text('''# 青川·当面私人交谈

独立 hanmo.known_personal_dialogues 1.0.0，namespace known_talk，场景 known_talk::scenario.qingchuan_known_dialogues。

首个模板是原县仓书佐与家中管事两名已安装成年人实际同地时的交谈。第二模板复用原真实出生条款、540 半日孕期和成年门，日后只选择真正出生实例，不把原型安装为活人。

页面仅显示眼前可见本人和明确模板，不读取旁支关系。statement 与 withdraw 为不同本人 SAY，本包不自动授予亲缘认可、接续控制、财产或任何任务。后继业务必须分别核真实声明与自己的权源。

目前仅源码与作者合同门；未运行完整 World、自然出生成年或 GUI。
''')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
