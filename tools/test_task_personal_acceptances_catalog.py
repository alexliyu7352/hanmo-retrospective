"""私人受领目录限定原委托与原sender本人SAY；不将来源门当作真实Task行为验收。"""
import copy,json,unittest
from pathlib import Path
from tools.content.task_personal_acceptance_links import check_task_personal_acceptances
ROOT=Path(__file__).resolve().parents[1]
def fixture():
 p=json.loads((ROOT/'content/packs/authored/task_personal_acceptances_1_0/content.json').read_text());return p,{k:{r['id']:r for r in p[k]} for k in ['offers','items','dialogues']}
class Acceptances(unittest.TestCase):
 def test_normal_author(self):
  p,i=fixture();self.assertEqual(check_task_personal_acceptances(p,i),[]);self.assertNotIn('tasks',p['scenarios'][0]['initial'])
 def test_old_empty(self):self.assertEqual(check_task_personal_acceptances({'scenarios':[{}]},{}),[])
 def test_original_q01_without_report(self):
  p,i=fixture();s=p['scenarios'][0];s['task_message_handoff_profile']='known_personal_delivery_and_report_v3';del s['dynamic_world']['task_personal_message_reports'];self.assertEqual(check_task_personal_acceptances(p,i),[])
 def test_handoff4_needs_report(self):
  p,i=fixture();del p['scenarios'][0]['dynamic_world']['task_personal_message_reports'];self.assertTrue(check_task_personal_acceptances(p,i))
 def test_no_supplied_amount(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['task_personal_message_acceptances']['policies'][0]['refund']=900;self.assertTrue(check_task_personal_acceptances(p,i))
 def test_wrong_original_sender(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['known_personal_dialogues']['templates'][-1]['provider']['source_id']='society::actor.sect_steward';self.assertTrue(check_task_personal_acceptances(p,i))
 def test_no_q01_policy(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['task_personal_message_deliveries']['policies']=[];self.assertTrue(check_task_personal_acceptances(p,i))
 def test_real_sender_say(self):
  for change in [{'kind':'choice'},{'speaker':'$actor'},{'command':{}}]:
   p,i=fixture();next(n for n in i['dialogues']['dialog.private_sender']['nodes'] if n['id']=='accept').update(change);self.assertTrue(check_task_personal_acceptances(p,i))
 def test_same_offer_extension_only(self):
  p,i=fixture();s=p['scenarios'][0];s['task_extensions']={'policies':[{'offer_id':'other'}]};self.assertEqual(check_task_personal_acceptances(p,i),[]);s['task_extensions']['policies'][0]['offer_id']='offer.letter';self.assertTrue(check_task_personal_acceptances(p,i))
 def test_original_knowledge_required(self):
  p,i=fixture();i['offers']['offer.letter']['discovery_sources']=[];self.assertTrue(check_task_personal_acceptances(p,i))
 def test_static_payment2_not_implied(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['political_takeover']={'task_execution_version':2};self.assertTrue(check_task_personal_acceptances(p,i))
 def test_original_half_day_only(self):
  p,i=fixture();i['offers']['offer.letter']['accept_ticks']=2;self.assertTrue(check_task_personal_acceptances(p,i))
 def test_unclosed_profiles(self):
  for name in ['task_message_notices','task_message_returns']:
   p,i=fixture();p['scenarios'][0][name]={};self.assertTrue(check_task_personal_acceptances(p,i))
if __name__=='__main__':unittest.main()
