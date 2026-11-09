"""复用原生育/照护作者的独立本人对白情景；不缩短成年、预置出生或生成接受事实。"""
import argparse,json
from pathlib import Path
from build_personal_appointment_author import emit

def build(repo):
    source=repo/'content/packs/authored/family_birth_1_0';target=repo/'content/packs/authored/born_personal_dialogues_1_0'
    pack=json.loads((source/'pack.json').read_text());content=json.loads((source/'content.json').read_text())
    pack.update(id='hanmo.born_personal_dialogues',version='1.0.0',namespace='born_talk');content.update(id='born_personal_dialogues',version='1.0.0')
    for obj in (pack,content):obj['required_capabilities'].append('born_personal_dialogues_v1')
    scene=content['scenarios'][0];old=scene['id'];scene['id']='scenario.qingchuan_born_dialogues';scene['enabled']['scenarios']=[scene['id'] if v==old else v for v in scene['enabled']['scenarios']]
    scene['name_key']='born_talk.name';scene['enabled']['dialogues'].append('dialog.adult_child')
    scene['dynamic_world']['born_personal_dialogues']={'version':1,'templates':[{'id':'adult_child','prototype_id':'actor.newborn_prototype','dialogue_id':'dialog.adult_child'}]}
    content['dialogues'].append({'id':'dialog.adult_child','context_role':'','entry':'hello','nodes':[
        {'id':'hello','kind':'say','speaker':'$provider','text_key':'born_talk.hello','next':'choose'},
        {'id':'choose','kind':'choice','text_key':'born_talk.choose','options':[{'id':'accept','text_key':'born_talk.ask_accept','conditions':[],'next':'accept'},{'id':'revoke','text_key':'born_talk.ask_revoke','conditions':[],'next':'revoke'},{'id':'leave','text_key':'born_talk.leave','conditions':[],'next':'end'}]},
        {'id':'accept','kind':'say','speaker':'$provider','text_key':'born_talk.accept','next':'choose'},
        {'id':'revoke','kind':'say','speaker':'$provider','text_key':'born_talk.revoke','next':'choose'},
        {'id':'end','kind':'end'}]})
    content['texts'].update({'born_talk.name':'青川·出生本人私人交谈','born_talk.hello':'我已成年。有什么事，请当面对我说。','born_talk.choose':'听取本人明确的答复','born_talk.ask_accept':'询问本人是否愿意承接将来的控制接续','born_talk.ask_revoke':'询问本人是否要撤回原答允','born_talk.leave':'结束交谈','born_talk.accept':'我本人愿意按明确条款接受将来的控制接续；若要办理，请现在让我当面确认原范围。','born_talk.revoke':'我本人要撤回原来的答允。请按原凭据办理，不要把以前的答话当作现在仍同意。'})
    emit(target/'pack.json',pack);emit(target/'content.json',content)
    emit(repo/'content/packs/selections/born-personal-dialogues-1.0.0.json',{'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':'hanmo.political_economy','version':'1.2.0'},{'id':'hanmo.born_personal_dialogues','version':'1.0.0'}]})
    (target/'README.md').write_text('''# 青川·出生本人私人交谈

独立 pack hanmo.born_personal_dialogues 1.0.0，namespace born_talk，场景 born_talk::scenario.qingchuan_born_dialogues。复用原 family_birth_1_0 生育、540 半日孕期和有限照护来源。没有预种孩子、成年、亲缘、许可或成功事实，也不缩短原成年年龄。

真实出生者日后成年且双方实际同地可用时，由原亲生父母当前本人从「本人私人交谈」选择该人。模板 adult_child 绑定原 actor.newborn_prototype 和 dialog.adult_child，真实 $provider 仍是本次出生实例，不能拿原型人物或岗位替代。

accept 与 revoke 是两处独立本人 say。此 S3 包只打开原会话并保精确来源，本身不启用 Legacy2，不因说话自动交接控制、转钱或改亲缘。S4 的后继具名政策须显式引用模板和两个节点并完整核验许可。

当前仅作者源码与定点结构门，未执行 compose/Installed/多年自然人生或 GUI。没有新造银行或以等待年限测试替代真实来源。
''')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);build(p.parse_args().repo)
