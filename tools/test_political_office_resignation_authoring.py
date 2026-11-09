"""辞任只能有明确原辖区和首长专项批准，不能在作者中预写完成结果。"""
import unittest,json
from copy import deepcopy
from pathlib import Path
from test_political_succession_authoring import fixture as original
from content.political_office_resignation_links import NAME,CAPABILITY,COMMAND,check_political_office_resignations,with_political_office_resignation_schema

def fixture():
    """下属座所与首府分开，审批权仍严格位于原首府。"""
    p=original();p['required_capabilities'].append(CAPABILITY);s=p['scenarios'][0];s['enabled']={'places':['hall','grain']};d=s['dynamic_world'];o=d['organization'];o['offices']['ordinary']={'id':'ordinary','organization_id':'city','holder_actor_id':'clerk','deputy_actor_id':None,'grant_ids':[]};o['offices']['chief']['deputy_actor_id']=None
    d['politics']['jurisdictions']['county']={'id':'county','office_id':'ordinary','seat_place_id':'grain','political_organization_id':'city','controlling_organization_id':'city','revision':0}
    o['grants']['approve']={'id':'approve','office_id':'chief','allowed_command_kinds':[COMMAND],'scopes':[{'kind':'site','id':'hall'}],'wallet_ids':[],'inventory_ids':[],'amount_limit':0,'may_delegate':False,'valid_from':0,'valid_until':None};o['offices']['chief']['grant_ids'].append('approve')
    d[NAME]={'version':1,'policies':{'resign':{'id':'resign','jurisdiction_id':'county','approval_succession_policy_id':'succession','approval_grant_id':'approve','response_ticks':8}}};return p

class Authoring(unittest.TestCase):
    def test_explicit(self):self.assertFalse(check_political_office_resignations(fixture(),{}))
    def test_old(self):
        p=fixture();del p['scenarios'][0]['dynamic_world'][NAME];self.assertFalse(check_political_office_resignations(p,{}))
    def test_capability(self):
        p=fixture();p['required_capabilities'].remove(CAPABILITY);self.assertTrue(check_political_office_resignations(p,{}))
    def test_profile(self):
        for bad in [True,0,2]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['version']=bad;self.assertTrue(check_political_office_resignations(p,{}))
    def test_exact_terms(self):
        for key,value in [('id','other'),('response_ticks',True),('response_ticks',0),('jurisdiction_id','capital'),('approval_succession_policy_id','unknown')]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['policies']['resign'][key]=value;self.assertTrue(check_political_office_resignations(p,{}))
    def test_dedicated_grant(self):
        for key,value in [('allowed_command_kinds',[COMMAND,'political_dismiss']),('amount_limit',1),('amount_limit',False),('may_delegate',True),('wallet_ids',['city']),('scopes',[{'kind':'site','id':'grain'}]),('valid_from',-1),('valid_until',-1)]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['approve'][key]=value;self.assertTrue(check_political_office_resignations(p,{}))
    def test_deputy_or_capture(self):
        for role in ['chief','ordinary']:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['offices'][role]['deputy_actor_id']='deputy';self.assertTrue(check_political_office_resignations(p,{}))
        p=fixture();p['scenarios'][0]['dynamic_world']['politics']['jurisdictions']['county']['revision']=1;self.assertTrue(check_political_office_resignations(p,{}))
    def test_schema_copy(self):
        s={'$defs':{'dynamic_world':{'properties':{}}}};old=deepcopy(s);out=with_political_office_resignation_schema(s);self.assertEqual(old,s);self.assertEqual(out['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'],[1])
    def test_normal(self):
        p=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_office_resignations_1_0/content.json').read_text());s=p['scenarios'][0];self.assertFalse(check_political_office_resignations(p,{}));self.assertNotIn(NAME,s['initial']);self.assertNotIn('family',s['initial']);self.assertEqual(s['player_actor'],'actor.retiring_clerk');self.assertEqual(s['dynamic_world']['organization']['offices']['office.grain_clerk']['epoch'],1)
        self.assertTrue(any(x['successor_actor_id']=='society::actor.city_steward' and x['predecessor_actor_id']==s['player_actor'] and x['personal']=={'money':0,'goods':[]} for x in p['succession_consent_requests']))
if __name__=='__main__':unittest.main()
