"""仅原作者及显式选择的必要源门，未跑NPC实际行旅或World。"""
import copy,json,unittest
from pathlib import Path
from tools.content.visit_appointment_links import check_visit_appointments
from tools.content.personal_appointment_links import check_personal_appointments
from tools.content.personal_appointment_attendance_links import check_personal_appointment_attendance
from tools.content.visit_appointment_attendance_links import check_visit_appointment_attendance
class VisitAppointmentAttendanceAuthorTest(unittest.TestCase):
    def setUp(self):
        self.repo=Path(__file__).resolve().parents[1];self.pack=json.loads((self.repo/'content/packs/authored/visit_appointments_1_1/content.json').read_text());self.scene=self.pack['scenarios'][0];self.index={k:{r['id']:r for r in self.pack[k]} for k in ['roles','dialogues']}
    def errors(self):return sum((fn(self.pack,self.index) for fn in [check_visit_appointments,check_personal_appointments,check_personal_appointment_attendance,check_visit_appointment_attendance]),[])
    def test_original_real_author_and_pure(self):
        before=copy.deepcopy(self.pack);self.assertEqual([],self.errors());self.assertEqual(before,self.pack)
    def test_old_author_unchanged(self):
        old=json.loads((self.repo/'content/packs/authored/visit_appointments_1_0/content.json').read_text());self.assertEqual([],check_visit_appointment_attendance(old,{}));self.assertNotIn('visit_appointment_attendance',old['scenarios'][0])
    def test_explicit_pair(self):
        self.pack['required_capabilities'].remove('visit_appointment_attendance_v1');self.assertTrue(self.errors())
    def test_original_template_required(self):
        self.scene['dynamic_world']['director']['subjects']['society::actor.study_swordswoman']['templates'].remove('personal_appointment_attendance');self.assertTrue(self.errors())
    def test_policy_not_invented(self):
        self.scene['visit_appointment_attendance']['invitation_policy_ids']=['unknown'];self.assertTrue(self.errors())
    def test_duplicate_or_boolean_version(self):
        self.scene['visit_appointment_attendance']['invitation_policy_ids']*=2;self.assertTrue(self.errors());self.scene['visit_appointment_attendance']['invitation_policy_ids']=['invitation.swordswoman'];self.scene['visit_appointment_attendance']['version']=True;self.assertTrue(self.errors())
    def test_original_advance_bound(self):
        self.scene['dynamic_world']['director']['personal_appointment_attendance']['policies'][0]['depart_before_ticks']=721;self.assertTrue(self.errors())
if __name__=='__main__':unittest.main()
