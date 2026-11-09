"""条件性供给Grant与原作者产权门同源；不把声明当作当前执行许可。"""
import copy
import unittest
from content.political_supply_links import political_supply_grant_declares_assets


class SupplyGrantTests(unittest.TestCase):
    def setUp(self):
        """新机构原资金与待捕获粮库共同构成一份专项Grant。"""
        self.grant = {'id': 'grant', 'office_id': 'office', 'allowed_command_kinds': ['political_issue_supply_policy'],
                      'amount_limit': 88, 'scopes': [{'kind': 'site', 'id': 'gate'}],
                      'wallet_ids': ['new.cash'], 'inventory_ids': ['old.grain']}
        self.scenario = {'dynamic_world': {'economic_supply_policies': {'version': 1, 'policies': {}},
            'political_takeover': {'enabled': True, 'version': 1, 'rules': {'rule': {
                'id': 'rule', 'successor_office_id': 'office', 'site_id': 'gate',
                'stronghold_id': 'fort', 'garrison_id': 'camp'}}},
            'organization': {'offices': {'office': {'id': 'office', 'organization_id': 'new', 'grant_ids': ['grant']}},
                             'organizations': {'old': {}, 'new': {}}}},
            'initial': {'wallets': [{'id': 'new.cash', 'owner': {'kind': 'organization', 'id': 'new'}},
                                    {'id': 'foreign.cash', 'owner': {'kind': 'organization', 'id': 'old'}}],
                        'inventories': [{'id': 'old.grain', 'owner': {'kind': 'organization', 'id': 'old'}}]}}
        self.index = {'strongholds': {'fort': {'garrison_id': 'camp', 'public_wallet_ids': [], 'public_inventory_ids': ['old.grain']}}}

    def allowed(self):
        """每次均从完整当次作者对象读取，不复用上次判断。"""
        return political_supply_grant_declares_assets(self.scenario, self.index, self.grant)

    def test_captured_store_and_own_money(self):
        """原自有钱包与待捕获公粮可由同一专项声明。"""
        self.assertTrue(self.allowed())

    def test_explicit_pair_required(self):
        """未声明新pair时沿原严格产权规则。"""
        del self.scenario['dynamic_world']['economic_supply_policies']
        self.assertFalse(self.allowed())

    def test_extra_uncaptured_foreign_asset_rejected(self):
        """额外未捕获公库不能借一个合法条目放行。"""
        self.grant['wallet_ids'].append('foreign.cash')
        self.assertFalse(self.allowed())

    def test_private_custody_never_becomes_public(self):
        """私人寄存不因位于公库而成为可授资源。"""
        self.scenario['initial']['inventories'][0]['owner'] = {'kind': 'actor', 'id': 'person'}
        self.assertFalse(self.allowed())

    def test_wrong_scope_or_command_cannot_borrow_exception(self):
        """范围或命令扩张不能复用条件性例外。"""
        old = copy.deepcopy(self.grant)
        self.grant['scopes'][0]['id'] = 'other'
        self.assertFalse(self.allowed())
        self.grant = old
        self.grant['allowed_command_kinds'].append('claim_need')
        self.assertFalse(self.allowed())

    def test_duplicate_and_boolean_version_rejected(self):
        """重复资源和布尔冒充数字版本必须拒绝。"""
        self.grant['inventory_ids'].append('old.grain')
        self.assertFalse(self.allowed())
        self.grant['inventory_ids'].pop()
        self.scenario['dynamic_world']['economic_supply_policies']['version'] = True
        self.assertFalse(self.allowed())


if __name__ == '__main__':
    unittest.main()
