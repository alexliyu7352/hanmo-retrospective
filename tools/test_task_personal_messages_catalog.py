"""作者声明与新旧选择边界；不把结构检查当真实会话/Task运行。"""
import copy,unittest,json
from pathlib import Path
from tools.content.task_personal_message_links import check_task_personal_messages,CAPABILITY,PROFILE
def fixture():
 caps=[CAPABILITY,'native_dynamic_world','multi_actor','dialogue','tasks.message','task_message_handoffs_v1','task_message_reports_v2','task_static_assets_v1','family_life_v1','known_personal_dialogues_v1']
 scene={'enabled':{'actors':['sender','recipient','carrier'],'items':['letter'],'offers':['offer'],'dialogues':['private']},'family_life':{'profile':'verified_military_deaths_v1'},'task_message_handoff_profile':'known_personal_delivery_and_report_v3','task_static_asset_profile':'original_task_assets_v1','dynamic_world':{'profile':'local_society_v1','known_personal_dialogues':{'version':1,'templates':[{'id':'known','listener':{'kind':'installed_actor','source_id':'carrier'},'provider':{'kind':'installed_actor','source_id':'recipient'},'dialogue_id':'private'}]},'task_personal_message_deliveries':{'version':1,'profile':PROFILE,'policies':[{'id':'p','offer_id':'offer','personal_template_id':'known','recipient_say_node_id':'receive'}]}}}
 index={'offers':{'offer':{'template':'message','issuer':{'kind':'actor','id':'sender'},'goal':{'receiver_actor':'recipient','message_id':'letter'},'deadline_ticks':16}},'items':{'letter':{}},'dialogues':{'private':{'nodes':[{'id':'receive','kind':'say','speaker':'$provider'}]}}}
 return {'required_capabilities':caps,'scenarios':[scene]},index
class Personal(unittest.TestCase):
 def test_explicit(self):self.assertEqual(check_task_personal_messages(*fixture()),[])
 def test_old(self):self.assertEqual(check_task_personal_messages({'scenarios':[{}]},{}),[])
 def test_caps(self):
  p,i=fixture()
  for cap in p['required_capabilities']:
   q=copy.deepcopy(p);q['required_capabilities'].remove(cap);self.assertTrue(check_task_personal_messages(q,i))
 def test_old_handoff_does_not_gain_narrative(self):
  for profile in ['observed_letter_delivery_v1','observed_letter_delivery_and_report_v2']:
   p,i=fixture();p['scenarios'][0]['task_message_handoff_profile']=profile;self.assertTrue(check_task_personal_messages(p,i))
 def test_handoff3_cannot_stand_alone(self):
  p,i=fixture();del p['scenarios'][0]['dynamic_world']['task_personal_message_deliveries'];self.assertTrue(check_task_personal_messages(p,i))
 def test_recipient(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['known_personal_dialogues']['templates'][0]['provider']['source_id']='sender';self.assertTrue(check_task_personal_messages(p,i))
 def test_public_contract(self):
  p,i=fixture();i['offers']['offer']['issuer']['kind']='organization';self.assertTrue(check_task_personal_messages(p,i))
 def test_role_not_ignored(self):
  p,i=fixture();i['dialogues']['private']['context_role']='public';self.assertTrue(check_task_personal_messages(p,i))
 def test_provider_say(self):
  for change in [{'kind':'choice'},{'speaker':'$actor'},{'command':{}}]:
   p,i=fixture();i['dialogues']['private']['nodes'][0].update(change);self.assertTrue(check_task_personal_messages(p,i))
 def test_extension_scope(self):
  p,i=fixture();p['scenarios'][0]['task_extensions']={'policies':[{'offer_id':'other'}]};self.assertEqual(check_task_personal_messages(p,i),[]);p['scenarios'][0]['task_extensions']['policies'][0]['offer_id']='offer';self.assertTrue(check_task_personal_messages(p,i))
 def test_normal_author(self):
  root=Path(__file__).resolve().parents[1];p=json.loads((root/'content/packs/authored/task_personal_messages_1_0/content.json').read_text());index={k:{r['id']:r for r in p[k]} for k in ['offers','items','dialogues']};self.assertEqual(check_task_personal_messages(p,index),[])
  scene=p['scenarios'][0];self.assertNotIn('tasks',scene['initial']);self.assertNotIn('task_extensions',scene);self.assertNotIn('task_message_returns',scene);self.assertNotIn('known_personal_dialogues',scene['initial']);person=next(a for a in scene['initial']['actors'] if a['actor_id']=='society::actor.sect_steward');role=next(r for r in p['roles'] if r['id']=='role.recipient');self.assertNotEqual(person['place_id'],role['place_id'])
  self.assertFalse(any(g.get('item_id')=='item.letter' for a in scene['initial']['actors'] for g in a.get('initial_goods',[])))
 def test_notice_scope(self):
  p,i=fixture();p['scenarios'][0]['task_message_notices']={};self.assertTrue(check_task_personal_messages(p,i))
 def test_returns_scope(self):
  p,i=fixture();p['scenarios'][0]['task_message_returns']={};self.assertTrue(check_task_personal_messages(p,i))
if __name__=='__main__':unittest.main()
