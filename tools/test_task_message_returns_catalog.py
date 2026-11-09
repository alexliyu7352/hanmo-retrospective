"""作者窄门，不代真实去程/回书/支付或World运行。"""
import copy,unittest,json
from pathlib import Path
from tools.content.task_message_return_links import check_task_message_returns,CAPABILITY,PROFILE
def fixture():
 p={'required_capabilities':[CAPABILITY,'native_dynamic_world','multi_actor','dialogue','tasks.message','task_message_handoffs_v1','task_message_reports_v2','task_static_assets_v1','family_life_v1'],'scenarios':[{'enabled':{'actors':['sender','recipient'],'roles':['s','r'],'places':['town'],'dialogues':['s.talk','r.talk'],'items':['letter','reply'],'offers':['offer']},'dynamic_world':{'profile':'local_society_v1'},'family_life':{'profile':'verified_military_deaths_v1'},'task_message_handoff_profile':'observed_letter_delivery_and_report_v2','task_static_asset_profile':'original_task_assets_v1','task_message_returns':{'version':1,'profile':PROFILE,'policies':[{'id':'policy','offer_id':'offer','reply_item_id':'reply','recipient_role_id':'r','dialogue_id':'r.talk','node_id':'entrust'}]}}]}
 i={'offers':{'offer':{'template':'message','issuer':{'kind':'actor','id':'sender'},'goal':{'receiver_actor':'recipient','message_id':'letter'},'report_role':'s','deadline_ticks':16}},'items':{'letter':{},'reply':{}},'roles':{},'dialogues':{}}
 for role,actor,service,node in [('s','sender','report_offer','report'),('r','recipient','deliver_message','entrust')]:i['roles'][role]={'initial_actor':actor,'place_id':'town','dialogue_id':role+'.talk','services':[service]};i['dialogues'][role+'.talk']={'context_role':role,'nodes':[{'id':node,'kind':'say','speaker':'$provider'}]}
 return p,i
class Returns(unittest.TestCase):
 def test_explicit(self):self.assertEqual(check_task_message_returns(*fixture()),[])
 def test_old_unselected(self):self.assertEqual(check_task_message_returns({'scenarios':[{}]},{}),[])
 def test_dependencies(self):
  p,i=fixture()
  for cap in p['required_capabilities']:
   q=copy.deepcopy(p);q['required_capabilities'].remove(cap);self.assertTrue(check_task_message_returns(q,i))
 def test_real_report_and_assets(self):
  for field in ['task_message_handoff_profile','task_static_asset_profile','family_life']:
   p,i=fixture();del p['scenarios'][0][field];self.assertTrue(check_task_message_returns(p,i))
 def test_private_original(self):
  p,i=fixture();i['offers']['offer']['issuer']['kind']='organization';self.assertTrue(check_task_message_returns(p,i))
 def test_initial_provider(self):
  p,i=fixture();i['roles']['r']['initial_actor']='sender';self.assertTrue(check_task_message_returns(p,i))
 def test_say(self):
  for kind in ['choice','action','end']:
   p,i=fixture();i['dialogues']['r.talk']['nodes'][0]['kind']=kind;self.assertTrue(check_task_message_returns(p,i))
 def test_distinct_letter(self):
  p,i=fixture();p['scenarios'][0]['task_message_returns']['policies'][0]['reply_item_id']='letter';self.assertTrue(check_task_message_returns(p,i))
 def test_same_offer_extension(self):
  p,i=fixture();p['scenarios'][0]['task_extensions']={'policies':[{'offer_id':'offer'}]};self.assertTrue(check_task_message_returns(p,i));p['scenarios'][0]['task_extensions']['policies'][0]['offer_id']='other';self.assertEqual(check_task_message_returns(p,i),[])
 def test_normal_author_has_no_seeded_reply(self):
  root=Path(__file__).resolve().parents[1];p=json.loads((root/'content/packs/authored/task_message_returns_1_0/content.json').read_text());index={k:{row['id']:row for row in p.get(k,[])} for k in ['offers','items','roles','dialogues']};self.assertEqual(check_task_message_returns(p,index),[])
  for scene in p['scenarios']:
   self.assertNotIn('task_message_returns',scene.get('initial',{}));self.assertNotIn('task_extensions',scene)
  for bag in p.get('inventories',[]):
   self.assertTrue(all(lot.get('item_id')!='item.reply' for lot in bag.get('lots',[])))
 def test_duplicate(self):
  p,i=fixture();policies=p['scenarios'][0]['task_message_returns']['policies'];policies.append(copy.deepcopy(policies[0]));self.assertTrue(check_task_message_returns(p,i))
if __name__=='__main__':unittest.main()
