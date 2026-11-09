"""只核新增作者真实引用与授权配对；不把结构门冒为玩家流程。"""
import copy,json,unittest
from pathlib import Path
from tools.content.political_military_appointment_links import check_political_military_appointments
ROOT=Path(__file__).resolve().parents[1]
class AppointmentAuthoring(unittest.TestCase):
    def setUp(self):
        self.pack=json.loads((ROOT/'content/packs/authored/military_appointments_1_0/content.json').read_text())
        self.index={'garrisons':{'qingxi::garrison.north':{'organization_id':'qingxi::org.north','place_id':'qingxi::place.north.camp'}}}
    def issues(self):return check_political_military_appointments(self.pack,self.index)
    def test_normal(self):self.assertEqual(self.issues(),[])
    def test_partial_pair(self):del self.pack['scenarios'][0]['military_officer_assignments'];self.assertTrue(self.issues())
    def test_wrong_camp(self):self.pack['scenarios'][0]['dynamic_world']['political_military_appointments']['policies']['appointment.north_camp']['site_id']='qingxi::place.south.camp';self.assertTrue(self.issues())
    def test_missing_grant(self):self.pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant.north_military_appointment']['allowed_command_kinds']=[];self.assertTrue(self.issues())
    def test_cash_grant_is_not_officer_permission(self):self.pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant.north_military_appointment']['wallet_ids']=['qingxi::wallet.north.public'];self.assertTrue(self.issues())
    def test_npc_wrong_org(self):self.pack['scenarios'][0]['dynamic_world']['director']['policies']['policy.officer']['organization_id']='qingxi::org.south';self.assertTrue(self.issues())
    def test_npc_requires_registry(self):self.pack['scenarios'][0]['dynamic_world']['director'].pop('registry_version');self.assertTrue(self.issues())
if __name__=='__main__':unittest.main()
