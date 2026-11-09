"""未知版本、缺实际来源和布尔冒整数必须在正常作者边界拒绝。"""
import copy
import unittest
from tools.content.fiscal_taxpayer_continuity_links import check_fiscal_taxpayer_continuity


class FiscalTaxpayerContinuityTest(unittest.TestCase):
    def test_explicit_and_complete_profile(self):
        world = {'profile': 'local_society_v1',
                 'political_fiscal': {'enabled': True},
                 'political_takeover': {'enabled': True, 'version': 1, 'rules': {'capture': {}}},
                 'fiscal_settlement': {'version': 1, 'districts': {'north': {}}}}
        pack = {'required_capabilities': ['fiscal_taxpayer_continuity_v1'], 'scenarios': [{'dynamic_world': world}]}
        self.assertEqual(check_fiscal_taxpayer_continuity(pack, {}), [])
        world['fiscal_taxpayer_continuity'] = {'enabled': True, 'version': 1}
        self.assertEqual(check_fiscal_taxpayer_continuity(pack, {}), [])
        for config in ({'enabled': False, 'version': 1}, {'enabled': True},
                       {'enabled': True, 'version': True}, {'enabled': True, 'version': 2},
                       {'enabled': 1, 'version': 1}, {'enabled': True, 'version': 1, 'extra': 0}):
            bad = copy.deepcopy(pack);bad['scenarios'][0]['dynamic_world']['fiscal_taxpayer_continuity'] = config
            self.assertTrue(check_fiscal_taxpayer_continuity(bad, {}))
        for key in ('political_fiscal', 'political_takeover', 'fiscal_settlement'):
            bad = copy.deepcopy(pack);del bad['scenarios'][0]['dynamic_world'][key]
            self.assertTrue(check_fiscal_taxpayer_continuity(bad, {}))
        bad = copy.deepcopy(pack);bad['required_capabilities'] = []
        self.assertTrue(check_fiscal_taxpayer_continuity(bad, {}))


if __name__ == '__main__':
    unittest.main()
