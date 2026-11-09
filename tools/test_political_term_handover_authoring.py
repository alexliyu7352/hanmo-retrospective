"""有限后任交接条款必须有独立公权；作者不能预写双方事实。"""
from copy import deepcopy
import unittest
from test_political_successor_round_authoring import fixture as original_fixture
from content.political_term_handover_links import NAME, CAPABILITY, COMMANDS, check_political_term_handovers, with_political_term_handover_schema

def fixture():
    """只建明确目录和零资金Grant，真实任职留待原Writer。"""
    p=original_fixture();p['required_capabilities'].append(CAPABILITY);d=p['scenarios'][0]['dynamic_world']
    d[NAME]={'version':1,'policies':{'term':{'id':'term','successor_round_policy_id':'live','handover_grant_id':'term_grant','response_ticks':8}}}
    d['organization']['offices']['chief']['grant_ids'].append('term_grant')
    d['organization']['grants']['term_grant']={'id':'term_grant','office_id':'chief','allowed_command_kinds':sorted(COMMANDS),'scopes':[{'kind':'site','id':'hall'}],'wallet_ids':[],'inventory_ids':[],'amount_limit':0}
    return p

class TermHandoverAuthoring(unittest.TestCase):
    def test_explicit(self):self.assertFalse(check_political_term_handovers(fixture(),{}))
    def test_unselected(self):
        p=fixture();del p['scenarios'][0]['dynamic_world'][NAME];self.assertFalse(check_political_term_handovers(p,{}))
    def test_missing_prior(self):
        for name in ('political_successions','political_successor_rounds'):
            p=fixture();del p['scenarios'][0]['dynamic_world'][name];self.assertTrue(check_political_term_handovers(p,{}))
    def test_profile(self):
        for group in ({'version':True,'policies':{}},{'version':2,'policies':{}},{'version':1,'policies':{},'accepted':True}):
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]=group;self.assertTrue(check_political_term_handovers(p,{}))
    def test_dedicated_grant(self):
        for field,value in [('amount_limit',1),('amount_limit',False),('wallet_ids',['public']),('inventory_ids',['stock']),('office_id','other'),('allowed_command_kinds',sorted(COMMANDS)+['political_nominate_next_successor'])]:
            p=fixture();p['scenarios'][0]['dynamic_world']['organization']['grants']['term_grant'][field]=value;self.assertTrue(check_political_term_handovers(p,{}))
    def test_exact_policy(self):
        for field,value in [('id','unknown'),('response_ticks',True),('response_ticks',0),('response_ticks',2147483648),('successor_round_policy_id','unknown'),('handover_grant_id','live_grant')]:
            p=fixture();p['scenarios'][0]['dynamic_world'][NAME]['policies']['term'][field]=value;self.assertTrue(check_political_term_handovers(p,{}))
        p=fixture();policies=p['scenarios'][0]['dynamic_world'][NAME]['policies'];policies['other']=dict(policies['term'],id='other');self.assertTrue(check_political_term_handovers(p,{}))
    def test_normal_input(self):
        import json
        from pathlib import Path
        c=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_term_handovers_1_0/content.json').read_text());s=c['scenarios'][0]
        self.assertFalse(check_political_term_handovers(c,{}));self.assertNotIn(NAME,s['initial']);self.assertNotIn('family',s['initial'])
        self.assertTrue(any(x['successor_actor_id']=='actor.next_clerk' and x['personal']=={'money':0,'goods':[]} for x in c['succession_consent_requests']))
    def test_schema_pure(self):
        s={'$defs':{'dynamic_world':{'properties':{}}}};old=deepcopy(s);new=with_political_term_handover_schema(s);self.assertEqual(old,s);self.assertEqual(new['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'],[1])

if __name__=='__main__':unittest.main()
