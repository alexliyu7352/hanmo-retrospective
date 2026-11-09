"""独立作者选择与权限边界源门，不生成占领、批准或接册Common记录。"""
import copy
import unittest
from test_political_shop_licensing_authoring import fixture as legal_fixture
from content.political_shop_registry_links import check_political_shop_registry_successions, CAPABILITY, COMMAND
from content.political_shop_registry_schema import with_political_shop_registry_schema
from content.political_shop_registry_author import build_political_shop_registry_declaration


def fixture():
    pack, index = legal_fixture()
    dynamic = pack['scenarios'][0]['dynamic_world']
    dynamic['political_takeover'] = {'enabled': True, 'version': 1, 'rules': {}}
    dynamic['political_shop_registry_successions'] = {'enabled': True, 'version': 1, 'policies': {'successor': {
        'id': 'successor', 'court_id': 'court', 'organization_id': 'county.org', 'office_id': 'office',
        'role_id': 'counter', 'dialogue_id': 'dialogue', 'request_node_id': 'declaration'}}}
    dynamic['organization']['grants']['grant']['allowed_command_kinds'].append(COMMAND)
    pack['required_capabilities'].append(CAPABILITY)
    return pack, index


class RegistryAuthoring(unittest.TestCase):
    def test_explicit_profile(self):
        self.assertEqual(check_political_shop_registry_successions(*fixture()), [])
        self.assertEqual(check_political_shop_registry_successions(*legal_fixture()), [])
    def test_missing_dependencies(self):
        for name in ['political_shop_licensing', 'private_shops', 'political_takeover']:
            pack, index = fixture(); del pack['scenarios'][0]['dynamic_world'][name]
            self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_explicit_format(self):
        for key, value in [('version', True), ('version', 2), ('enabled', False), ('approved', True)]:
            pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['political_shop_registry_successions'][key] = value
            self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_no_inherited_ordinary_grant(self):
        pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['allowed_command_kinds'].remove(COMMAND)
        self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_no_relocating_the_registry(self):
        for field, value in [('place_id', 'market'), ('organization_id', 'other')]:
            pack, index = fixture(); index['roles']['counter'][field] = value
            self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_duplicate_successor_rejected(self):
        pack, index = fixture(); policies = pack['scenarios'][0]['dynamic_world']['political_shop_registry_successions']['policies']
        policies['other'] = dict(policies['successor'], id='other')
        self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_runtime_fields_not_authoring(self):
        pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['political_shop_registry_successions']['policies']['successor']['acquitted_actors'] = ['applicant']
        self.assertTrue(check_political_shop_registry_successions(pack, index))
    def test_helper_keeps_original_book_and_requires_real_office(self):
        pack,index=fixture();dynamic=pack['scenarios'][0]['dynamic_world'];dynamic.pop('political_shop_registry_successions');pack['scenarios'][0]['id']='scene';pack['texts']={'registry.accept':'接收原司法册'};saved=copy.deepcopy(pack)
        result=build_political_shop_registry_declaration(pack,index,'scene',policy_id='next',court_id='court',organization_id='county.org',office_id='office',role_id='counter',dialogue_id='dialogue',request_node_id='declaration',grant_id='registry.grant',text_key='registry.accept')
        self.assertEqual(pack,saved);self.assertEqual(result['grant']['allowed_command_kinds'],[COMMAND]);self.assertEqual(result['grant']['wallet_ids'],[])
        self.assertEqual(result['policy']['court_id'],'court');self.assertNotIn('initial_wanted',result)
        with self.assertRaises(ValueError):build_political_shop_registry_declaration(pack,index,'scene',policy_id='next',court_id='court',organization_id='other',office_id='office',role_id='counter',dialogue_id='dialogue',request_node_id='declaration',grant_id='registry.grant',text_key='registry.accept')
    def test_schema_preserves_original(self):
        source = {'$defs': {'dynamic_world': {'properties': {'old': {'const': 1}}}}}; before = copy.deepcopy(source)
        result = with_political_shop_registry_schema(source)
        self.assertEqual(source, before); self.assertEqual(result['$defs']['dynamic_world']['properties']['old'], {'const': 1})
        self.assertEqual(with_political_shop_registry_schema(result), result)
        result['$defs']['dynamic_world']['properties']['political_shop_registry_successions']['required'] = []
        with self.assertRaises(ValueError): with_political_shop_registry_schema(result)

if __name__ == '__main__':
    unittest.main()
