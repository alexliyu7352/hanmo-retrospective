"""作者原岗位/原首职/专项Grant的窄门，不生成任职或控制成功。"""
import copy,json,unittest
from pathlib import Path
from content.political_office_reception_links import *

def fixture():
    """固定正常新作者同时给其真实本包Role和Dialogue索引。"""
    pack=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_office_receptions_1_0/content.json').read_text());return pack,{kind:{x['id']:x for x in pack[kind]} for kind in ('roles','dialogues')}
class ReceptionAuthoring(unittest.TestCase):
    def test_normal(self):
        p,i=fixture();self.assertFalse(check_political_office_receptions(p,i));self.assertNotIn(NAME,p['scenarios'][0]['initial']);self.assertNotIn('family',p['scenarios'][0]['initial'])
    def test_original_counter_initial(self):
        p,i=fixture();s=p['scenarios'][0];rows=[x for x in s['initial']['roles'] if x['role_id']=='role.public_reception'];self.assertEqual(rows,[{'role_id':'role.public_reception','actor_id':i['roles']['role.public_reception']['initial_actor'],'enabled':True}]);self.assertIn('role.public_reception',s['enabled']['roles'])
    def test_absent(self):
        p,i=fixture();del p['scenarios'][0]['dynamic_world'][NAME];self.assertFalse(check_political_office_receptions(p,i))
    def test_versions(self):
        for value in (False,2,0):
            p,i=fixture();p['scenarios'][0]['dynamic_world'][NAME]['version']=value;self.assertTrue(check_political_office_receptions(p,i))
    def test_original_role(self):
        for k,v in [('initial_actor','unrelated'),('organization_id',None),('services',['information','report_offer']),('place_id','other'),('dialogue_id','missing')]:
            p,i=fixture();i['roles']['role.public_reception'][k]=v;self.assertTrue(check_political_office_receptions(p,i))
    def test_duty(self):
        p,i=fixture();p['scenarios'][0]['dynamic_world']['organization']['duties']['fixed']={'role_id':'role.public_reception'};self.assertTrue(check_political_office_receptions(p,i))
    def test_grant(self):
        for k,v in [('amount_limit',False),('amount_limit',1),('may_delegate',True),('wallet_ids',['public']),('valid_until',-1),('allowed_command_kinds',[COMMAND,'political_appoint'])]:
            p,i=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['grant.office_reception'][k]=v;self.assertTrue(check_political_office_receptions(p,i))
    def test_duplicate_role(self):
        p,i=fixture();rows=p['scenarios'][0]['dynamic_world'][NAME]['policies'];row=next(iter(rows.values()));rows['second']=dict(row,id='second');self.assertTrue(check_political_office_receptions(p,i))
    def test_capability(self):
        p,i=fixture();p['required_capabilities'].remove(CAPABILITY);self.assertTrue(check_political_office_receptions(p,i))
    def test_schema(self):
        s={'$defs':{'dynamic_world':{'properties':{}}}};old=copy.deepcopy(s);out=with_political_office_reception_schema(s);self.assertEqual(s,old);self.assertEqual(out['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'],[1]);self.assertEqual(out['$defs']['dynamic_world']['properties'][NAME]['properties']['policies']['additionalProperties']['properties']['role_id'],{'$ref':'#/$defs/reference_roles'})
if __name__=='__main__':unittest.main()
