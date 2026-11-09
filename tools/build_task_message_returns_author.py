"""从固定正规私人信件作者生成独立Q03；不预写Task、实交、回书物件或同意历史。"""
import json,subprocess
from pathlib import Path
BASE='a52488b202a1e23af6c23bb964083449cacbf765'
def build(root):
 prefix='content/packs/authored/task_extensions_1_0/'
 content=json.loads(subprocess.check_output(['git','show',BASE+':'+prefix+'content.json']))
 pack=json.loads(subprocess.check_output(['git','show',BASE+':'+prefix+'pack.json']))
 content['id']='task_message_returns';pack['id']='hanmo.task_message_returns';pack['namespace']='returns'
 caps=[c for c in content['required_capabilities'] if c!='task_extensions_v1']
 for c in ['task_message_returns_v1','family_life_v1']:
  if c not in caps:caps.append(c)
 content['required_capabilities']=caps;pack['required_capabilities']=caps.copy()
 scene=content['scenarios'][0];scene.pop('task_extensions');scene['id']='scenario.qingchuan_round_trip_letter';scene['enabled']['scenarios']=[scene['id']];scene['name_key']='name.scenario.round_trip';scene['family_life']={'profile':'verified_military_deaths_v1','player_death':'await_explicit_continuation'}
 scene['task_message_returns']={'version':1,'profile':'private_round_trip_letter_v1','policies':[{'id':'policy.letter_reply','offer_id':'offer.letter','reply_item_id':'item.reply','recipient_role_id':'role.recipient','dialogue_id':'dialog.recipient','node_id':'entrust_return'}]}
 content['items'].append({'id':'item.reply','name_key':'name.item.reply','kind':'message','base_price':0,'weight':1});scene['enabled']['items'].append('item.reply')
 for offer in content['offers']:
  if offer['id']=='offer.letter':offer['deadline_ticks']=32
 for talk in content['dialogues']:
  if talk['id']=='dialog.sender':
   talk['nodes']=[n for n in talk['nodes'] if n['id']!='extension_consent']
   for node in talk['nodes']:
    if node['kind']=='choice':node['options']=[o for o in node['options'] if o['id']!='ask_extension']
  if talk['id']=='dialog.recipient':
   for node in talk['nodes']:
    if node['id']=='recipient_menu':node['options'].insert(1,{'id':'ask_return','text_key':'option.return.ask','conditions':[{'op':'task_state','offer_id':'offer.letter','state':'ready'}],'next':'entrust_return'})
    if node['id']=='recipient_done':node['next']='entrust_return'
   talk['nodes'].append({'id':'entrust_return','kind':'say','speaker':'$provider','text_key':'line.return.entrust','next':'recipient_menu'})
 content['texts'].update({'name.scenario.round_trip':'青川往返信件','name.item.reply':'剑院回书','option.return.ask':'请执事托付回书','line.return.entrust':'来信我已收到。我愿将这封回书托你亲手送回书院先生；请留下半日，核明封缄后当面交给你。','line.sender.letter_terms':'请亲手将来信交给剑院执事，并取得他本人托回的回书，带回交到我手里才算往返办妥。受领办完后三十二个半日内回来，报酬一百二十钱；只有口头回报不能结酬。','line.sender.accepted_letter':'去程与回书都是这一次委托；请保管原件，到剑院当面请执事托回，再把回书亲手交还。'})
 for key in ['option.ask_extension','line.extension.consent']:
  content['texts'].pop(key,None)
 folder=root/'content/packs/authored/task_message_returns_1_0';folder.mkdir(parents=True,exist_ok=True)
 for name,data in [('pack.json',pack),('content.json',content)]:
  (folder/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n')
 selection={'format':'HM-PACK-SELECTION','format_version':1,'packs':[{'id':'hanmo.local_society','version':'1.11.0'},{'id':pack['id'],'version':'1.0.0'}]}
 selected=root/'content/packs/selections/task-message-returns-1.0.0.json';selected.parent.mkdir(parents=True,exist_ok=True);selected.write_text(json.dumps(selection,ensure_ascii=False,indent=2)+'\n')
if __name__=='__main__':build(Path(__file__).resolve().parents[1])
