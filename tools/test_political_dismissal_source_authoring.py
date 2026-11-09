"""原撤职专项权、普通首长自然路径与旧profile独立作者门。"""
import unittest,json
from pathlib import Path
from copy import deepcopy
from content.political_dismissal_source_links import *
ROOT=Path(__file__).resolve().parents[1]
def fixture():return json.loads((ROOT/'content/packs/authored/political_dismissal_sources_1_0/content.json').read_text())
class Authoring(unittest.TestCase):
    def test_normal(self):self.assertEqual(check_political_dismissal_sources(fixture(),{}),[])
    def test_absent(self):
        p=fixture();del p['scenarios'][0]['dynamic_world'][NAME];self.assertEqual(check_political_dismissal_sources(p,{}),[])
    def test_explicit(self):
        for cap in [CAPABILITY,'family_life_v1']:
            p=fixture();p['required_capabilities'].remove(cap);self.assertTrue(check_political_dismissal_sources(p,{}))
    def test_original_lifecycle(self):
        s=fixture()['scenarios'][0];self.assertEqual(s['player_actor'],'society::actor.city_steward');office=s['dynamic_world']['organization']['offices']['office.qingchuan_governance'];self.assertIsNone(office['holder_actor_id']);self.assertEqual(office['epoch'],0);self.assertNotIn(NAME,s['initial']);self.assertIn('political_career',s['dynamic_world']['director']['policies']['private_adult_life']['templates'])
    def test_no_resources_or_delegation(self):
        for key,value in [('may_delegate',True),('wallet_ids',['public']),('amount_limit',1),('allowed_command_kinds',['political_appoint']),('valid_until',-1)]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['grant.qingchuan_dismissal'][key]=value;self.assertTrue(check_political_dismissal_sources(p,{}))
    def test_exact_policy_and_version(self):
        for key,value in [('id','wrong'),('issuer_office_id','missing'),('jurisdiction_id','missing')]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['policies']['dismissal.qingchuan'][key]=value;self.assertTrue(check_political_dismissal_sources(p,{}))
        for version in [True,0,2]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['version']=version;self.assertTrue(check_political_dismissal_sources(p,{}))
    def test_no_deputy_and_no_capture(self):
        p=fixture();p['scenarios'][0]['dynamic_world']['organization']['offices']['office.qingchuan_governance']['deputy_actor_id']='society::actor.apprentice';self.assertTrue(check_political_dismissal_sources(p,{}))
        p=fixture();p['scenarios'][0]['dynamic_world']['politics']['jurisdictions']['jurisdiction.qingchuan']['revision']=1;self.assertTrue(check_political_dismissal_sources(p,{}))
    def test_no_cross_jurisdiction_exception(self):
        p=fixture();d=p['scenarios'][0]['dynamic_world'];d['organization']['organizations']['society::org.city']['headquarters_site']='society::place.grain';self.assertTrue(check_political_dismissal_sources(p,{}))
        p=fixture();d=p['scenarios'][0]['dynamic_world'];d['politics']['jurisdictions']['other']={'id':'other','office_id':'office.city_steward'};self.assertTrue(check_political_dismissal_sources(p,{}))
    def test_schema(self):
        schema={'$defs':{'dynamic_world':{'properties':{}}}};old=deepcopy(schema);selected=with_political_dismissal_source_schema(schema);self.assertEqual(schema,old);self.assertEqual(selected['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'],[1])
if __name__=='__main__':unittest.main()
