"""作者语义只验证显式新配置，旧pair和旧Need无需添加投影。"""
import copy
import unittest
from tools.content.occupation_fiscal_need_links import check_occupation_fiscal_needs


class OccupationFiscalNeedsTest(unittest.TestCase):
    """变异必要版本、能力和真实旧Need依赖，防止仅有新税账就隐式启用。"""
    def test_explicit_profile(self):
        world = {'profile': 'local_society_v1',
                 'political_occupation_fiscal': {'enabled': True, 'version': 1},
                 'fiscal_settlement': {'version': 1},
                 'affairs': {'fiscal_need_policies': {'version': 1, 'policies': {'cash': {}}}}}
        pack = {'required_capabilities': ['occupation_fiscal_needs_v1'], 'scenarios': [{'dynamic_world': world}]}
        self.assertEqual(check_occupation_fiscal_needs(pack, {}), [])
        world['occupation_fiscal_needs'] = {'enabled': True, 'version': 1}
        self.assertEqual(check_occupation_fiscal_needs(pack, {}), [])
        for config in ({'enabled': False, 'version': 1}, {'enabled': True},
                       {'enabled': True, 'version': True}, {'enabled': True, 'version': 2},
                       {'enabled': True, 'version': 1, 'extra': 0}):
            bad = copy.deepcopy(pack);bad['scenarios'][0]['dynamic_world']['occupation_fiscal_needs'] = config
            self.assertTrue(check_occupation_fiscal_needs(bad, {}))
        for field in ('political_occupation_fiscal', 'fiscal_settlement', 'affairs'):
            bad = copy.deepcopy(pack);del bad['scenarios'][0]['dynamic_world'][field]
            self.assertTrue(check_occupation_fiscal_needs(bad, {}))
        bad = copy.deepcopy(pack);bad['required_capabilities'] = []
        self.assertTrue(check_occupation_fiscal_needs(bad, {}))


if __name__ == '__main__':
    unittest.main()
