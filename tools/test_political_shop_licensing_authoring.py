"""原作者声明可完整准入，但未知历史、外城铺位、缺专项权源均不得悄悄补齐。"""
import copy
import json
from pathlib import Path
import unittest
from content.political_shop_licensing_links import check_political_shop_licensing
from content.political_shop_licensing_schema import with_political_shop_licensing_schema
from content.political_shop_licensing_author import build_political_shop_court_declaration
from jsonschema import Draft202012Validator


def fixture():
    """只含作者目录，没有Common通缉、许可或已开店事实。"""
    court = {'id':'court','jurisdiction_id':'county','office_id':'office','role_id':'counter',
             'dialogue_id':'dialogue','request_node_id':'declaration','premises_ids':['shop'],
             'warrant_reason_codes':['authored_case'],'initial_wanted':[], 'complete_registry':True,'permit_ticks':20}
    grant = {'id':'grant','office_id':'office','allowed_command_kinds':['political_issue_shop_permit','political_record_shop_clearance'],
             'wallet_ids':[],'inventory_ids':[],'amount_limit':0,'scopes':[{'kind':'site','id':'hall'}]}
    dynamic = {'profile':'local_society_v1','private_shops':{'enabled':True,'version':1,'premises':{'shop':{'id':'shop','place_id':'market','lessor_actor_id':'lessor'}}},
               'political_shop_licensing':{'version':1,'courts':{'court':court}},
               'organization':{'enabled':True,'offices':{'office':{'id':'office','organization_id':'county.org','grant_ids':['grant']}},'grants':{'grant':grant}},
               'politics':{'enabled':True,'jurisdictions':{'county':{'seat_place_id':'hall','political_organization_id':'county.org','controlling_organization_id':'county.org','revision':0}}}}
    pack = {'required_capabilities':['political_shop_licensing_v1','private_shops_v1'],
            'scenarios':[{'dynamic_world':dynamic,'enabled':{'actors':['lessor','applicant','magistrate'],'places':['hall','market'],'roles':['counter'],'dialogues':['dialogue']}}]}
    index = {'actors':{name:{} for name in ['lessor','applicant','magistrate']},
             'places':{'hall':{'settlement_id':'town'},'market':{'settlement_id':'town'}},
             'roles':{'counter':{'place_id':'hall','organization_id':'county.org','dialogue_id':'dialogue','initial_actor':'magistrate'}},
             'dialogues':{'dialogue':{'context_role':'counter','nodes':[{'id':'declaration','kind':'say','speaker':'$provider'}]}}}
    return pack,index


class Authoring(unittest.TestCase):
    """同城不同place正向、旧未启用字节和每项关键拒绝分别覆盖。"""
    def test_local_market(self):
        self.assertEqual(check_political_shop_licensing(*fixture()),[])
    def test_exact_premises_map(self):
        for broken in [[{'id':'shop','place_id':'market','lessor_actor_id':'lessor'}],
                       {'wrong':{'id':'shop','place_id':'market','lessor_actor_id':'lessor'}},
                       {'shop':None}]:
            pack,index=fixture();pack['scenarios'][0]['dynamic_world']['private_shops']['premises']=broken
            self.assertTrue(check_political_shop_licensing(pack,index))
    def test_old_absent(self):
        self.assertEqual(check_political_shop_licensing({'scenarios':[{}]},{}),[])
    def test_registry_and_versions(self):
        mutations=[lambda d:d['political_shop_licensing'].update(version=True),
                   lambda d:d['political_shop_licensing'].update(version=2),
                   lambda d:d['political_shop_licensing'].update(runtime_permits=[]),
                   lambda d:d['political_shop_licensing']['courts']['court'].update(complete_registry=False),
                   lambda d:d['political_shop_licensing']['courts']['court'].pop('initial_wanted'),
                   lambda d:d['political_shop_licensing']['courts']['court'].update(permit_ticks=0),
                   lambda d:d['political_shop_licensing']['courts']['court'].update(initial_wanted=[{'actor_id':'applicant','reason_code':'unknown'}]),
                   lambda d:d.pop('political_shop_licensing'),lambda d:d.pop('private_shops')]
        for mutate in mutations:
            pack,index=fixture();mutate(pack['scenarios'][0]['dynamic_world'])
            with self.subTest(mutate=mutate):self.assertTrue(check_political_shop_licensing(pack,index))
    def test_positive_initial_wanted(self):
        pack,index=fixture();pack['scenarios'][0]['dynamic_world']['political_shop_licensing']['courts']['court']['initial_wanted']=[{'actor_id':'applicant','reason_code':'authored_case'}]
        self.assertEqual(check_political_shop_licensing(pack,index),[])
    def test_every_shop_has_one_court(self):
        pack,index=fixture();dynamic=pack['scenarios'][0]['dynamic_world']
        dynamic['private_shops']['premises']['uncovered']={'id':'uncovered','place_id':'market','lessor_actor_id':'lessor'}
        self.assertTrue(any(row[0]=='POLITICAL_SHOP_PREMISES_COVERAGE' for row in check_political_shop_licensing(pack,index)))
        dynamic['political_shop_licensing']['courts']['court']['premises_ids'].append('uncovered')
        self.assertEqual(check_political_shop_licensing(pack,index),[])
    def test_exact_original_authority(self):
        mutations=[lambda d:d['organization']['grants']['grant'].update(allowed_command_kinds=['political_issue_shop_permit']),
                   lambda d:d['organization']['grants']['grant'].update(wallet_ids=['private_wallet']),
                   lambda d:d['organization']['grants']['grant'].update(scopes=[{'kind':'site','id':'market'}]),
                   lambda d:d['politics']['jurisdictions']['county'].update(controlling_organization_id='invader'),
                   lambda d:d['organization']['offices']['office'].update(organization_id='other')]
        for mutate in mutations:
            pack,index=fixture();mutate(pack['scenarios'][0]['dynamic_world'])
            with self.subTest(mutate=mutate):self.assertTrue(check_political_shop_licensing(pack,index))
    def test_actual_local_sources(self):
        for target,field,value in [('places','settlement_id','elsewhere'),('roles','organization_id','foreign'),('roles','place_id','market')]:
            pack,index=fixture();index[target]['market' if target=='places' else 'counter'][field]=value
            self.assertTrue(check_political_shop_licensing(pack,index))
        pack,index=fixture();index['dialogues']['dialogue']['nodes'][0]['speaker']='magistrate'
        self.assertTrue(check_political_shop_licensing(pack,index))
    def test_enabled_capability(self):
        pack,index=fixture();pack['required_capabilities'].remove('political_shop_licensing_v1');self.assertTrue(check_political_shop_licensing(pack,index))
        pack,index=fixture();pack['scenarios'][0]['enabled']['roles']=[];self.assertTrue(check_political_shop_licensing(pack,index))
    def test_schema_source_and_namespaces(self):
        original=json.loads((Path(__file__).resolve().parents[1]/'schemas/content.schema.json').read_text())
        saved=copy.deepcopy(original);candidate=with_political_shop_licensing_schema(original)
        self.assertEqual(original,saved)
        self.assertEqual(with_political_shop_licensing_schema(candidate),candidate)
        fields=candidate['$defs']['political_shop_court']['properties']
        for field in ('id','jurisdiction_id','office_id','request_node_id'):
            self.assertEqual(fields[field],{'$ref':'#/$defs/entity_id'})
        self.assertEqual(fields['role_id'],{'$ref':'#/$defs/reference_roles'})
        self.assertEqual(fields['dialogue_id'],{'$ref':'#/$defs/reference_dialogues'})
        self.assertEqual(fields['initial_wanted']['items']['properties']['actor_id'],{'$ref':'#/$defs/reference_actors'})
        source=fixture()[0]['scenarios'][0]['dynamic_world']['political_shop_licensing']
        validator=Draft202012Validator({'$ref':'#/$defs/political_shop_licensing_catalog','$defs':candidate['$defs']})
        self.assertFalse(list(validator.iter_errors(source)))
        for key,value in [('version',True),('version',2),('runtime_permits',[])]:
            changed=copy.deepcopy(source);changed[key]=value
            self.assertTrue(list(validator.iter_errors(changed)))
        candidate['$defs']['political_shop_court']['properties']['office_id']={'$ref':'#/$defs/reference_actors'}
        with self.assertRaises(ValueError):with_political_shop_licensing_schema(candidate)
    def test_existing_counter_author_fragment(self):
        pack,index=fixture();scenario=pack['scenarios'][0]
        scenario['dynamic_world'].pop('political_shop_licensing')
        office=scenario['dynamic_world']['organization']['offices']['office']
        office.update(holder_actor_id='magistrate',status='active')
        index['dialogues']['dialogue']['entry']='declaration'
        args=dict(court_id='court',jurisdiction_id='county',office_id='office',counter_actor_id='magistrate',
                  role_id='counter',request_node_id='declaration',premises_ids=['shop'],grant_id='law.full',
                  complete_registry=True,initial_wanted=[],warrant_reason_codes=['authored_case'])
        saved=copy.deepcopy((pack,index));fragment=build_political_shop_court_declaration(scenario,index,**args)
        self.assertEqual((pack,index),saved)
        self.assertEqual(fragment['grant']['wallet_ids'],[])
        self.assertEqual(fragment['grant']['inventory_ids'],[])
        self.assertFalse(fragment['grant']['may_delegate'])
        self.assertEqual(fragment['grant']['allowed_command_kinds'],['political_issue_shop_permit','political_record_shop_clearance'])
        self.assertEqual(fragment['office_grant_ids'],['grant','law.full'])
        scenario['dynamic_world']['political_shop_licensing']={'version':1,'courts':{'court':fragment['court']}}
        scenario['dynamic_world']['organization']['grants']['law.full']=fragment['grant']
        office['grant_ids']=fragment['office_grant_ids']
        self.assertEqual(check_political_shop_licensing(pack,index),[])
        self.assertEqual(build_political_shop_court_declaration(scenario,index,**args),fragment)
    def test_author_fragment_cannot_invent_local_authority(self):
        pack,index=fixture();scenario=pack['scenarios'][0];scenario['dynamic_world'].pop('political_shop_licensing')
        scenario['dynamic_world']['organization']['offices']['office'].update(holder_actor_id='magistrate',status='active')
        index['dialogues']['dialogue']['entry']='declaration'
        args=dict(court_id='court',jurisdiction_id='county',office_id='office',counter_actor_id='magistrate',
                  role_id='counter',request_node_id='declaration',premises_ids=['shop'],grant_id='law.full',
                  complete_registry=True,initial_wanted=[],warrant_reason_codes=['authored_case'])
        for field,value in [('complete_registry',False),('initial_wanted',None),('initial_wanted',[{'actor_id':'unknown','reason_code':'authored_case'}]),('administrative_commands',['political_set_tax']),('valid_until',-1)]:
            changed=copy.deepcopy(args);changed[field]=value
            with self.assertRaises(ValueError):build_political_shop_court_declaration(scenario,index,**changed)
        index['roles']['counter']['initial_actor']='lessor'
        with self.assertRaises(ValueError):build_political_shop_court_declaration(scenario,index,**args)
        index['roles']['counter']['initial_actor']='magistrate';index['dialogues']['dialogue']['entry']='other'
        with self.assertRaises(ValueError):build_political_shop_court_declaration(scenario,index,**args)
        index['dialogues']['dialogue']['entry']='declaration';index['places']['market']['settlement_id']='elsewhere'
        with self.assertRaises(ValueError):build_political_shop_court_declaration(scenario,index,**args)


if __name__=='__main__':unittest.main()
