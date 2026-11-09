"""结果来源作者准入必要门；不把声明当实际玩家请求或补造历史来源。"""
from copy import deepcopy
import unittest
from tools.content.private_shop_command_results_links import (
    CAPABILITY, MODULE, check_private_shop_command_results, with_private_shop_command_results_schema,
)


def sample():
    return {'required_capabilities': [CAPABILITY, 'private_shops_v1', 'native_dynamic_world'],
            'scenarios': [{'dynamic_world': {'profile': 'local_society_v1',
                'private_shops': {'enabled': True, 'version': 1}, MODULE: {'version': 1}}}]}


class ResultAuthor(unittest.TestCase):
    def test_exact_and_readonly(self):
        p = sample(); before = deepcopy(p)
        self.assertEqual(check_private_shop_command_results(p, {}), [])
        self.assertEqual(p, before)

    def test_old_unselected(self):
        self.assertEqual(check_private_shop_command_results({'scenarios': [{}]}, {}), [])

    def test_bad_declarations(self):
        for value in (None, [], {}, {'version': True}, {'version': 2},
                      {'version': 1, 'enabled': True}, {'version': 1, 'bindings': {}}):
            p = sample(); p['scenarios'][0]['dynamic_world'][MODULE] = value
            self.assertTrue(check_private_shop_command_results(p, {}))

    def test_capabilities(self):
        for cap in sample()['required_capabilities']:
            p = sample(); p['required_capabilities'].remove(cap)
            self.assertTrue(check_private_shop_command_results(p, {}))

    def test_original_pair(self):
        for value in (None, {}, {'enabled': False, 'version': 1},
                      {'enabled': True, 'version': True}, {'enabled': True, 'version': 2}):
            p = sample(); p['scenarios'][0]['dynamic_world']['private_shops'] = value
            self.assertTrue(check_private_shop_command_results(p, {}))

    def test_no_initial_result(self):
        p = sample(); p['scenarios'][0]['initial'] = {MODULE: {}}
        self.assertTrue(check_private_shop_command_results(p, {}))

    def test_strict_schema(self):
        source = {'$defs': {'dynamic_world': {'properties': {}}},
                  'properties': {'required_capabilities': {'items': {'enum': []}}}}
        before = deepcopy(source); result = with_private_shop_command_results_schema(source)
        self.assertEqual(source, before)
        self.assertEqual(with_private_shop_command_results_schema(result), result)
        self.assertEqual(result['properties']['required_capabilities']['items']['enum'], [CAPABILITY])
        self.assertFalse(result['$defs'][MODULE + '_catalog']['additionalProperties'])
        result['$defs'][MODULE + '_catalog']['properties']['version']['enum'] = [1, 2]
        with self.assertRaises(ValueError):
            with_private_shop_command_results_schema(result)

    def test_missing_cap_table(self):
        with self.assertRaises(ValueError):
            with_private_shop_command_results_schema({'$defs': {'dynamic_world': {'properties': {}}}})


if __name__ == '__main__':
    unittest.main()
