"""普通本人不换控的显式 NPC 审批作者边界。"""
from copy import deepcopy
import unittest,json
from pathlib import Path
from test_political_office_resignation_authoring import fixture as original
from content.political_office_resignation_response_links import *
from content.political_office_resignation_links import check_political_office_resignations

def fixture():
    p=original();p['required_capabilities']+=['political_office_resignations_v2',CAPABILITY];s=p['scenarios'][0];d=s['dynamic_world'];d[RESIGNATIONS]['version']=2;s['enabled']['actors']=['leader','clerk'];d['organization']['offices']['chief']['holder_actor_id']='leader';d['director']={'policies':{'head_policy':{'organization_id':'city','templates':['maintain_commitment',TEMPLATE],FIELD:'response'}},NAME:{'version':1,'intents':{'response':{'id':'response','resignation_policy_id':'resign','approver_actor_id':'leader','director_policy_id':'head_policy','minimum_service_ticks':0}}}};return p

def errors(p):return check_political_office_resignation_responses(p,{'actors':{'leader':{},'clerk':{}}})
class Responses(unittest.TestCase):
    def test_exact(self):self.assertFalse(errors(fixture()));self.assertFalse(check_political_office_resignations(fixture(),{}))
    def test_profile_pair(self):
        for version in [True,1,3]:
            p=fixture();p['scenarios'][0]['dynamic_world'][RESIGNATIONS]['version']=version;self.assertTrue(errors(p))
        p=fixture();del p['scenarios'][0]['dynamic_world']['director'][NAME];self.assertTrue(errors(p))
    def test_reason_is_bounded(self):
        for v in [True,-1,4097,'0']:
            p=fixture();p['scenarios'][0]['dynamic_world']['director'][NAME]['intents']['response']['minimum_service_ticks']=v;self.assertTrue(errors(p))
    def test_real_person_and_policy(self):
        for k,v in [('approver_actor_id','clerk'),('director_policy_id','other'),('resignation_policy_id','other'),('id','other')]:
            p=fixture();p['scenarios'][0]['dynamic_world']['director'][NAME]['intents']['response'][k]=v;self.assertTrue(errors(p))
    def test_no_hidden_producer(self):
        p=fixture();del p['scenarios'][0]['dynamic_world']['director']['policies']['head_policy'];self.assertTrue(errors(p))
    def test_no_capability_default(self):
        p=fixture();p['required_capabilities'].remove(CAPABILITY);self.assertTrue(errors(p))
    def test_normal_no_control_path(self):
        p=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_office_resignations_2_0/content.json').read_text());self.assertEqual(len(p['scenarios']),2)
        for s in p['scenarios']:
            self.assertEqual(s['player_actor'],'actor.retiring_clerk');self.assertNotIn(RESIGNATIONS,s['initial']);self.assertNotIn('family',s['initial']);self.assertNotIn('request.resignation_control',s.get('voluntary_succession',{}).get('request_ids',[]))
        self.assertFalse(any(x.get('id')=='request.resignation_control' for x in p['succession_consent_requests']))
if __name__=='__main__':unittest.main()
