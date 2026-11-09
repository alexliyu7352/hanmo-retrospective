"""新税政作者的最小权限门；不把声明的未来接任Grant当今日付款许可。"""
import copy
import unittest
from content.political_occupation_fiscal_links import check_political_occupation_fiscal, occupation_fiscal_grant_declares_assets


class OccupationFiscalAuthorTests(unittest.TestCase):
    def setUp(self):
        """构造原方公库与另一机构Office的明确未来接任声明。"""
        self.grant = {'id': 'tax', 'office_id': 'office', 'allowed_command_kinds': ['political_set_occupation_tax'], 'wallet_ids': ['cash'], 'inventory_ids': ['grain'], 'scopes': [{'kind': 'site', 'id': 'seat'}]}
        self.scenario = {'initial': {'wallets': [{'id': 'cash', 'owner': {'kind': 'organization', 'id': 'old'}}], 'inventories': [{'id': 'grain', 'owner': {'kind': 'organization', 'id': 'old'}}]}, 'dynamic_world': {'profile': 'local_society_v1', 'political_occupation_fiscal': {'enabled': True, 'version': 1}, 'political_fiscal': {'enabled': True}, 'fiscal_settlement': {'version': 1}, 'political_takeover': {'enabled': True, 'rules': {'rule': {'jurisdiction_id': 'district', 'site_id': 'seat', 'successor_office_id': 'office'}}}, 'politics': {'jurisdictions': {'district': {'public_wallet_id': 'cash', 'public_inventory_ids': ['grain']}}}, 'organization': {'organizations': {'old': {}, 'new': {}}, 'offices': {'office': {'id': 'office', 'organization_id': 'new', 'grant_ids': ['tax']}}, 'grants': {'tax': self.grant}}}}
        self.pack = {'required_capabilities': ['occupation_fiscal_v1'], 'scenarios': [self.scenario]}

    def test_exact_source(self):
        """完整原公库与现有Office目录声明可编入作者包。"""
        self.assertEqual(check_political_occupation_fiscal(self.pack, None), [])
        self.assertTrue(occupation_fiscal_grant_declares_assets(self.scenario, None, self.grant))

    def test_extra_or_private_asset(self):
        """多夹公库和私人财物不能混在专项Grant中。"""
        self.grant['inventory_ids'].append('other')
        self.assertFalse(occupation_fiscal_grant_declares_assets(self.scenario, None, self.grant))
        self.grant['inventory_ids'].pop()
        self.scenario['initial']['wallets'][0]['owner'] = {'kind': 'actor', 'id': 'person'}
        self.assertFalse(occupation_fiscal_grant_declares_assets(self.scenario, None, self.grant))

    def test_office_binding(self):
        """仅在Grant写office_id不能冒该Office真实目录授予。"""
        self.scenario['dynamic_world']['organization']['offices']['office']['grant_ids'] = []
        self.assertFalse(occupation_fiscal_grant_declares_assets(self.scenario, None, self.grant))
        self.assertTrue(check_political_occupation_fiscal(self.pack, None))

    def test_profile_pair_and_command(self):
        """旧财政/显式能力/专项命令缺失均拒，未知配置也不默认开启。"""
        for key in ['political_fiscal', 'fiscal_settlement', 'political_takeover']:
            candidate = copy.deepcopy(self.pack)
            del candidate['scenarios'][0]['dynamic_world'][key]
            self.assertTrue(check_political_occupation_fiscal(candidate, None))
        self.grant['allowed_command_kinds'].append('trade')
        self.assertFalse(occupation_fiscal_grant_declares_assets(self.scenario, None, self.grant))
        self.pack['required_capabilities'] = []
        self.assertTrue(check_political_occupation_fiscal(self.pack, None))


if __name__ == '__main__':
    unittest.main()
