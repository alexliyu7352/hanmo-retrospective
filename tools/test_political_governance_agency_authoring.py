"""正规作者只声明原Office/Grant与有限模板，坏权限及隐式启用须被拒。"""
import copy,json,unittest
from pathlib import Path
from content.political_governance_agency_links import *
def fixture():
    """使用实际新包原件，不在测试中伪造受领或签发成功。"""
    return json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_governance_agencies_1_0/content.json').read_text())
class AgencyAuthor(unittest.TestCase):
    def test_normal(self):
        p=fixture();self.assertFalse(check_political_governance_agencies(p,{}));self.assertNotIn(NAME,p['scenarios'][0]['initial']);self.assertNotIn('family',p['scenarios'][0]['initial'])
    def test_absent(self):
        p=fixture();del p['scenarios'][0]['dynamic_world'][NAME];self.assertFalse(check_political_governance_agencies(p,{}))
    def test_version_fields(self):
        for field,value in [('version',True),('version',2),('policies',{})]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME][field]=value;self.assertTrue(check_political_governance_agencies(p,{}))
    def test_limits(self):
        for field,value in [('response_ticks',0),('response_ticks',49),('maximum_issues',False),('mandate_ticks',4097),('id','other')]:
            p=fixture();next(iter(p['scenarios'][0]['dynamic_world'][NAME]['policies'].values()))[field]=value;self.assertTrue(check_political_governance_agencies(p,{}))
    def test_office(self):
        for field,value in [('holder_actor_id',None),('status','vacant'),('deputy_actor_id','society::actor.apprentice')]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['offices']['office.city_steward'][field]=value;self.assertTrue(check_political_governance_agencies(p,{}))
    def test_specific_grant(self):
        for field,value in [('amount_limit',1),('wallet_ids',['society::wallet.city']),('may_delegate',True),('allowed_command_kinds',['political_commission_governance'])]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['grant.agency_appointment'][field]=value;self.assertTrue(check_political_governance_agencies(p,{}))
    def test_full_original_grant(self):
        for field,value in [('amount_limit',119),('inventory_ids',[]),('may_delegate',False),('valid_until',-1)]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['grant.agency_commission'][field]=value;self.assertTrue(check_political_governance_agencies(p,{}))
    def test_dependencies(self):
        p=fixture();p['required_capabilities'].remove(CAPABILITY);self.assertTrue(check_political_governance_agencies(p,{}))
        p=fixture();del p['scenarios'][0]['family_life'];self.assertTrue(check_political_governance_agencies(p,{}))
    def test_schema(self):
        s={'$defs':{'dynamic_world':{'properties':{}}}};old=copy.deepcopy(s);out=with_political_governance_agency_schema(s);self.assertEqual(s,old);props=out['$defs']['dynamic_world']['properties'][NAME]['properties'];self.assertEqual(props['version']['enum'],[1]);self.assertEqual(props['policies']['additionalProperties']['properties']['public_inventory_id'],{'$ref':'#/$defs/reference_inventories'})
if __name__=='__main__':unittest.main()
