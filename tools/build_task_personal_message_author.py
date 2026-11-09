"""原私人送信的独立Q01内容；真实收件人外出时当面接信，不预置受领/信件/同意事实。"""
import json,subprocess
from pathlib import Path
BASE='a7e37936e1718d5673d0d7fa1c78ffd09a8ae679'
def build(root):
 folder='content/packs/authored/task_extensions_1_0/'
 content=json.loads(subprocess.check_output(['git','show',BASE+':'+folder+'content.json']))
 pack=json.loads(subprocess.check_output(['git','show',BASE+':'+folder+'pack.json']))
 content['id']='task_personal_messages';pack['id']='hanmo.task_personal_messages';pack['namespace']='personal_letter'
 caps=[c for c in content['required_capabilities'] if c!='task_extensions_v1']
 for cap in ['task_personal_message_deliveries_v1','known_personal_dialogues_v1','family_life_v1']:
  if cap not in caps:caps.append(cap)
 pack['required_capabilities']=caps.copy();content['required_capabilities']=caps
 scene=content['scenarios'][0];scene.pop('task_extensions');scene['id']='scenario.qingchuan_personal_letter';scene['enabled']['scenarios']=[scene['id']];scene['name_key']='personal_letter.name';scene['family_life']={'profile':'verified_military_deaths_v1','player_death':'await_explicit_continuation'};scene['task_message_handoff_profile']='known_personal_delivery_and_report_v3'
 scene['dynamic_world']['known_personal_dialogues']={'version':1,'templates':[{'id':'known.recipient','dialogue_id':'dialog.private_recipient','listener':{'kind':'installed_actor','source_id':'society::actor.apprentice'},'provider':{'kind':'installed_actor','source_id':'society::actor.sect_steward'}}]}
 scene['dynamic_world']['task_personal_message_deliveries']={'version':1,'profile':'known_private_recipient_v1','policies':[{'id':'policy.original_letter','offer_id':'offer.letter','personal_template_id':'known.recipient','recipient_say_node_id':'receive'}]}
 # 正常初态只声明本人当天人在城院，原接待岗位仍留剑院；不是运行期迁移或伪造交付。
 for actor in scene['initial']['actors']:
  if actor['actor_id']=='society::actor.sect_steward':actor['place_id']='society::place.courtyard'
 for talk in content['dialogues']:
  if talk['id']=='dialog.sender':
   talk['nodes']=[n for n in talk['nodes'] if n['id']!='extension_consent']
   for node in talk['nodes']:
    if node['kind']=='choice':node['options']=[o for o in node['options'] if o['id']!='ask_extension']
 content['dialogues'].append({'id':'dialog.private_recipient','context_role':'','entry':'hello','nodes':[{'id':'hello','kind':'say','speaker':'$provider','text_key':'personal_letter.hello','next':'menu'},{'id':'menu','kind':'choice','text_key':'personal_letter.menu','options':[{'id':'ask_receive','text_key':'personal_letter.ask','conditions':[],'next':'receive'},{'id':'leave','text_key':'personal_letter.leave','conditions':[],'next':'end'}]},{'id':'receive','kind':'say','speaker':'$provider','text_key':'personal_letter.receive','next':'menu'},{'id':'end','kind':'end'}]})
 scene['enabled']['dialogues'].append('dialog.private_recipient')
 content['texts'].update({'personal_letter.name':'青川·当面寻人交信','personal_letter.hello':'我今日外出，暂不在剑院接待处。若有给我的私信，可以当面核明。','personal_letter.menu':'听取收件人自己的答复','personal_letter.ask':'询问是否愿意现在接收先生托送的原信','personal_letter.leave':'结束交谈','personal_letter.receive':'我愿在这里亲自核封接收先生托你送来的那封原信。请留半日当面交付；只听见这句话还不算已经交到我手里。','line.sender.letter_terms':'请把这封信亲手交给剑院执事本人。他今日外出，若在城中见到，可以先听他本人答允，再当面交付原信。仍须回来向我报告，依原期限办理，报酬一百二十钱。'})
 for key in ['option.ask_extension','line.extension.consent']:content['texts'].pop(key,None)
 target=root/'content/packs/authored/task_personal_messages_1_0';target.mkdir(parents=True,exist_ok=True)
 for name,obj in [('pack.json',pack),('content.json',content)]: (target/name).write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n')
 selection={'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':pack['id'],'version':'1.0.0'}]};(root/'content/packs/selections/task-personal-messages-1.0.0.json').write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
 (target/'README.md').write_text('''# 青川·当面寻人交信

独立 hanmo.task_personal_messages 1.0.0，namespace personal_letter。正常场景沿原先生私人委托，原收件者在城院，旧接待岗位仍在剑院。玩家先从先生本人受领原信，再在城院真实看见收件者、打开 Known 本人对白，听到 receive 后由原任务页开始半日交信；真实原件转到收件者后，回先生原岗位返报结酬。

不预置 Task、信件批次、Known opening、同意、实交或完成历史。原私人期限不延期，也不改变往返回书、死讯、政治继位和身后清店合同。handoff3 与 task_personal_message_deliveries/1 必须同选；旧 handoff1/2 字节和岗位门保留。

当前为源码与作者检查候选，完整 World、自然命令链和 GUI 运行尚待同代正式产物。
''')
if __name__=='__main__':build(Path(__file__).resolve().parents[1])
