"""新日期邀约正规作者与必要坏声明；不产生实际拜帖、承诺或会面。"""
import copy,json,unittest
from pathlib import Path
from tools.content.visit_appointment_links import check_visit_appointments
from tools.content.personal_appointment_links import check_personal_appointments
class VisitAppointmentAuthorTest(unittest.TestCase):
    def setUp(self):
        self.pack=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/visit_appointments_1_0/content.json').read_text())
        self.scene=self.pack['scenarios'][0];self.policy=self.scene['visit_appointment_invitations']['policies'][0]
        self.index={k:{v['id']:v for v in self.pack[k]} for k in ['roles','dialogues']}
    def errors(self):return check_visit_appointments(self.pack,self.index)+check_personal_appointments(self.pack,self.index)
    def test_real_author_pure(self):
        before=copy.deepcopy(self.pack);self.assertEqual([],self.errors());self.assertEqual(before,self.pack)
        self.assertEqual([],check_visit_appointments({'scenarios':[{}]},{}))
        self.assertNotIn('visit_message_replies',self.scene)
        self.assertTrue(any(r['role_id']=='role.appointment_swordswoman' and r['actor_id']=='society::actor.study_swordswoman' and r['enabled'] for r in self.scene['initial']['roles']))
        for d in self.pack['dialogues']:
            for n in d['nodes']:
                if n['kind']=='choice':self.assertLessEqual(len(n['options']),8)
    def test_explicit_fixed_self_promise(self):
        for field,value in [('inviter_commits_fixed_window',False),('accept_before_offset',0),('accept_before_offset',9),('earliest_offset',17),('latest_offset',721),('latest_offset',True)]:
            old=self.policy[field];self.policy[field]=value;self.assertTrue(self.errors());self.policy[field]=old
    def test_original_pair(self):
        for key in ['visit_appointment_profile','visit_message_profile','personal_appointment_profile']:
            old=self.scene.pop(key);self.assertTrue(self.errors());self.scene[key]=old
        self.pack['required_capabilities'].remove('visit_appointments_v1');self.assertTrue(self.errors())
    def test_no_ordinary_reinterpretation(self):
        self.scene['visit_message_replies']={'version':1,'policies':[{'message_policy_id':self.policy['message_policy_id']}]};self.assertTrue(self.errors())
    def test_exact_recipient_and_private_person(self):
        old=self.scene['personal_appointments']['policies'][0]['provider_actor_id'];self.scene['personal_appointments']['policies'][0]['provider_actor_id']='society::actor.tutor';self.assertTrue(self.errors());self.scene['personal_appointments']['policies'][0]['provider_actor_id']=old
        self.index['roles']['role.appointment_swordswoman']['initial_actor']='society::actor.tutor';self.assertTrue(self.errors())
    def test_actual_custodian_say(self):
        node=next(n for n in self.index['dialogues']['dialog.visit_reception']['nodes'] if n['id']=='offer_invitation')
        node['command']={};self.assertTrue(self.errors());del node['command'];node['speaker']='$actor';self.assertTrue(self.errors())
    def test_explicit_npc_source(self):
        i=self.scene['visit_appointment_intents']['intents'][0];i['reception_intent_id']='unknown';self.assertTrue(self.errors());i['reception_intent_id']='reception.receiver';self.pack['required_capabilities'].remove('visit_appointment_intents_v1');self.assertTrue(self.errors())
    def test_finite_real_route(self):
        route=self.scene['visit_appointment_intents']['intents'][0]['route'];route[1]['option_id']='leave';self.assertTrue(self.errors())
if __name__=='__main__':unittest.main()
