"""补任原目录、专项Grant、普通NPC路线与不预置结果的必要作者门。"""
import unittest,json
from copy import deepcopy
from pathlib import Path
from content.political_subordinate_appointment_links import NAME,INTENTS,CAPABILITY,check_political_subordinate_appointments,with_political_subordinate_appointment_schema
ROOT=Path(__file__).resolve().parents[1]
def fixture():return json.loads((ROOT/'content/packs/authored/political_subordinate_appointments_1_0/content.json').read_text())
class Authoring(unittest.TestCase):
    def test_normal(self):self.assertEqual(check_political_subordinate_appointments(fixture(),{}),[])
    def test_absent(self):
        p=fixture();d=p['scenarios'][0]['dynamic_world'];del d[NAME];del d[INTENTS];self.assertEqual(check_political_subordinate_appointments(p,{}),[])
    def test_explicit_capability(self):
        for cap in [CAPABILITY,INTENTS+'_v1','political_successions_v1']:
            p=fixture();p['required_capabilities'].remove(cap);self.assertTrue(check_political_subordinate_appointments(p,{}))
    def test_no_preinstalled_source(self):
        s=fixture()['scenarios'][0];self.assertEqual(s['player_actor'],'subappoint::actor.candidate_clerk');self.assertNotIn(NAME,s['initial']);o=s['dynamic_world']['organization']['offices']['office.grain_clerk'];self.assertEqual((o['holder_actor_id'],o['epoch'],o['status']),(None,0,'vacant'))
    def test_grant_exact(self):
        for k,v in [('may_delegate',True),('amount_limit',1),('wallet_ids',['public']),('allowed_command_kinds',['political_appoint']),('scopes',[{'kind':'site','id':'society::place.grain'}])]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['grant.grain_office_appointment'][k]=v;self.assertTrue(check_political_subordinate_appointments(p,{}))
    def test_explicit_original_staffing(self):
        p=fixture();p['scenarios'][0]['dynamic_world']['director']['policies']['qingchuan_steward']['political_career']['staffing_posts']=[];self.assertTrue(check_political_subordinate_appointments(p,{}))
    def test_original_candidate_preference(self):
        for response in ['decline','defer','accept_qualified']:
            p=fixture();p['scenarios'][0]['dynamic_world']['director']['policies']['grain_office_candidate']['political_career']['response']=response;self.assertEqual(check_political_subordinate_appointments(p,{}),[])
    def test_corrupt_pair_and_actor(self):
        for key,value in [('id','wrong'),('candidate_actor_id','missing'),('issuer_actor_id','actor.candidate_clerk')]:
            p=fixture();p['scenarios'][0]['dynamic_world'][INTENTS]['intents']['intent.grain_office_appointment'][key]=value;self.assertTrue(check_political_subordinate_appointments(p,{}))
    def test_version_and_capture(self):
        for bad in [True,0,2]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['version']=bad;self.assertTrue(check_political_subordinate_appointments(p,{}))
        p=fixture();p['scenarios'][0]['dynamic_world']['politics']['jurisdictions']['jurisdiction.grain_office']['revision']=1;self.assertTrue(check_political_subordinate_appointments(p,{}))
    def test_schema_independent(self):
        s={'$defs':{'dynamic_world':{'properties':{}}}};before=deepcopy(s);out=with_political_subordinate_appointment_schema(s);self.assertEqual(before,s);self.assertEqual(out['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'],[1]);self.assertEqual(out['$defs']['dynamic_world']['properties'][INTENTS]['properties']['intents']['maxProperties'],64)
    def test_panel_sources(self):
        s=(ROOT/'game/ui/political_subordinate_appointment_panel.gd').read_text();self.assertIn('source_identity()',s);self.assertIn('observer_actor_id',s);self.assertIn('own_arrival',s);self.assertIn('witnessed_acceptance',s);self.assertNotIn('witnessed_answer',s)
if __name__=='__main__':unittest.main()
