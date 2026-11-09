"""显式三方NPC目录必须配2版、原职和三份策略；旧1版无意图保持可用。"""
import copy,json,unittest
from pathlib import Path
from content.political_governance_agency_links import check_political_governance_agencies, with_political_governance_agency_schema
from content.political_governance_agency_intent_links import *


def fixture():
    """实际正常作者原件，不造任何运行成功事实。"""
    p=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_governance_agencies_2_0/content.json').read_text())
    return p,{'actors':{a:{} for a in p['scenarios'][0]['enabled']['actors']}}


class AgencyIntentsAuthor(unittest.TestCase):
    def test_normal_and_pure(self):
        p,i=fixture();old=copy.deepcopy(p);self.assertFalse(check_political_governance_agencies(p,i));self.assertFalse(check_political_governance_agency_intents(p,i));self.assertEqual(p,old)
    def test_pair(self):
        for field,value in [('version',1),('version',True),('version',3)]:
            p,i=fixture();p['scenarios'][0]['dynamic_world'][AGENCY][field]=value;self.assertTrue(check_political_governance_agency_intents(p,i))
        p,i=fixture();p['required_capabilities'].remove(CAPABILITY);self.assertTrue(check_political_governance_agency_intents(p,i))
    def test_no_implicit_intent(self):
        p,i=fixture();del p['scenarios'][0]['dynamic_world']['director'][NAME];self.assertTrue(check_political_governance_agency_intents(p,i));self.assertTrue(check_political_governance_agencies(p,i))
    def test_exact_choices(self):
        for field,value in [('delegate_accepts',1),('delegate_accepts',False),('executor_actor_id','society::actor.apprentice'),('id','other'),('accepted',True)]:
            p,i=fixture();next(iter(p['scenarios'][0]['dynamic_world']['director'][NAME]['intents'].values()))[field]=value;self.assertTrue(check_political_governance_agency_intents(p,i))
    def test_original_office(self):
        p,i=fixture();p['scenarios'][0]['dynamic_world']['organization']['offices']['office.city_steward']['holder_actor_id']='society::actor.apprentice';self.assertTrue(check_political_governance_agency_intents(p,i))
    def test_three_policies(self):
        for field,value in [('templates',['maintain_commitment']),('governance_agency_intent_id','wrong'),('organization_id','other')]:
            p,i=fixture();p['scenarios'][0]['dynamic_world']['director']['policies']['agency_delegate'][field]=value;self.assertTrue(check_political_governance_agency_intents(p,i))
    def test_unknown_or_duplicate(self):
        p,i=fixture();del i['actors']['society::actor.home_keeper'];self.assertTrue(check_political_governance_agency_intents(p,i))
        p,i=fixture();table=p['scenarios'][0]['dynamic_world']['director'][NAME]['intents'];table['other']=dict(next(iter(table.values())),id='other');self.assertTrue(check_political_governance_agency_intents(p,i))
    def test_no_npc_relabel_of_old_world(self):
        from test_political_governance_agency_authoring import fixture as old_fixture
        p=old_fixture();self.assertFalse(check_political_governance_agency_intents(p,{}));self.assertFalse(check_political_governance_agencies(p,{}))
    def test_schema(self):
        s={'$defs':{k:{'properties':{}} for k in ('dynamic_world','dynamic_director','dynamic_director_legacy','dynamic_director_registry','dynamic_director_subject')},'properties':{'required_capabilities':{'items':{'enum':[]}}},'t':{'enum':['maintain_commitment']}}
        s=with_political_governance_agency_schema(s);old=copy.deepcopy(s);out=with_political_governance_agency_intent_schema(s);self.assertEqual(s,old);self.assertEqual(out['$defs']['dynamic_world']['properties'][AGENCY]['properties']['version']['enum'],[1,2]);self.assertIn(TEMPLATE,out['t']['enum']);self.assertIn(NAME,out['$defs']['dynamic_director_registry']['properties'])
if __name__=='__main__':unittest.main()
