"""私人发信人返报只选择原委托/原sender，目录门不冒实际Task或Known运行。"""
import copy,json,unittest
from pathlib import Path
from tools.content.task_personal_report_links import check_task_personal_reports
from tools.content.task_personal_message_links import check_task_personal_messages
from tools.content.task_message_handoff_links import check_task_message_handoffs
ROOT=Path(__file__).resolve().parents[1]
def fixture():
 p=json.loads((ROOT/'content/packs/authored/task_personal_reports_1_0/content.json').read_text());return p,{k:{r['id']:r for r in p[k]} for k in ['offers','items','dialogues']}
class Reports(unittest.TestCase):
 def test_normal_author(self):
  p,i=fixture();self.assertEqual(check_task_personal_reports(p,i),[]);self.assertEqual(check_task_personal_messages(p,i),[]);self.assertEqual(check_task_message_handoffs(p,i),[]);self.assertNotIn('tasks',p['scenarios'][0]['initial']);self.assertNotIn('known_personal_dialogues',p['scenarios'][0]['initial'])
 def test_old_unchanged(self):self.assertEqual(check_task_personal_reports({'scenarios':[{}]},{}),[])
 def test_missing_report(self):
  p,i=fixture();del p['scenarios'][0]['dynamic_world']['task_personal_message_reports'];self.assertTrue(check_task_personal_reports(p,i));self.assertTrue(check_task_personal_messages(p,i));self.assertTrue(check_task_message_handoffs(p,i))
 def test_old_versions_do_not_gain_report(self):
  for v in ['observed_letter_delivery_v1','observed_letter_delivery_and_report_v2','known_personal_delivery_and_report_v3']:
   p,i=fixture();p['scenarios'][0]['task_message_handoff_profile']=v;self.assertTrue(check_task_personal_reports(p,i))
 def test_exact_sender(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['known_personal_dialogues']['templates'][-1]['provider']['source_id']='society::actor.sect_steward';self.assertTrue(check_task_personal_reports(p,i))
 def test_no_automatic_recipient_grant(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['task_personal_message_reports']['policies'][0]['personal_template_id']='known.recipient';self.assertTrue(check_task_personal_reports(p,i))
 def test_original_delivery_required(self):
  p,i=fixture();p['scenarios'][0]['dynamic_world']['task_personal_message_deliveries']['policies']=[];self.assertTrue(check_task_personal_reports(p,i))
 def test_provider_say(self):
  for change in [{'kind':'choice'},{'speaker':'$actor'},{'command':{}}]:
   p,i=fixture();i['dialogues']['dialog.private_sender']['nodes'][2].update(change);self.assertTrue(check_task_personal_reports(p,i))
 def test_no_public_or_new_amount(self):
  p,i=fixture();i['offers']['offer.letter']['issuer']['kind']='organization';self.assertTrue(check_task_personal_reports(p,i));p,i=fixture();p['scenarios'][0]['dynamic_world']['task_personal_message_reports']['policies'][0]['reward']=999;self.assertTrue(check_task_personal_reports(p,i))
 def test_unclosed_combinations(self):
  for name in ['task_message_returns','task_message_notices']:
   p,i=fixture();p['scenarios'][0][name]={};self.assertTrue(check_task_personal_reports(p,i))
 def test_extension_exact_offer(self):
  p,i=fixture();p['scenarios'][0]['task_extensions']={'policies':[{'offer_id':'unrelated'}]};self.assertEqual(check_task_personal_reports(p,i),[]);p['scenarios'][0]['task_extensions']['policies'][0]['offer_id']='offer.letter';self.assertTrue(check_task_personal_reports(p,i))
if __name__=='__main__':unittest.main()
