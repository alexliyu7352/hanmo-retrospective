"""固定救济成本与实际家庭来源门；不模拟政治权力或转移资产。"""
from copy import deepcopy
from pathlib import Path
import importlib.util
import unittest

SPEC = importlib.util.spec_from_file_location('public_relief_links', Path(__file__).resolve().parents[2] / 'tools/content/public_relief_links.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture():
    """明确家庭、同地cohort和固定120/10安民规则，份额不依赖测试生成公式。"""
    owner = {'kind': 'household', 'id': 'family'}
    home = {'wallet_id': 'money', 'inventory_id': 'food'}
    cohort = {'household_group_id': 'family', 'site_id': 'town', 'private_wallet_id': 'money', 'private_inventory_id': 'food'}
    policy = {'id': 'aid', 'site_id': 'town', 'organization_id': 'city', 'item_id': 'grain', 'recipients': [{'household_group_id': 'family', 'cohort_id': 'workers', 'cash': 120, 'grain': 10}]}
    governance = {'enabled': True, 'appeasement_rules': {'appease': {'grain_item_id': 'grain', 'cash_amount': 120, 'grain_amount': 10}}, 'commission_policies': {'commission': {'rule_id': 'appease', 'relief_policy_id': 'aid'}}}
    scenario = {'enabled': {'places': ['town']}, 'initial': {'wallets': [{'id': 'money', 'owner': owner}], 'inventories': [{'id': 'food', 'owner': owner}]}, 'dynamic_world': {'local_economy': {'enabled': True, 'households': {'family': home}, 'cohorts': {'workers': cohort}}, 'politics_governance': governance, 'public_relief': {'version': 1, 'policies': {'aid': policy}}}}
    return {'scenarios': [scenario]}, {'organizations': {'city': {'id': 'city'}}, 'items': {'grain': {'id': 'grain'}}}


class PublicReliefAuthoring(unittest.TestCase):
    """少额、重复和错产权都指向作者原字段，读取不会改写输入。"""
    def test_real_fixed_plan_and_legacy_do_not_mutate(self):
        pack, index = fixture(); before = deepcopy(pack)
        self.assertEqual(MODULE.check_public_relief(pack, index), [])
        self.assertEqual(pack, before)
        self.assertEqual(MODULE.check_public_relief({'scenarios': [{'dynamic_world': None}]}, {}), [])

    def test_underpayment_cannot_keep_full_outcome(self):
        pack, index = fixture(); plan = pack['scenarios'][0]['dynamic_world']['public_relief']['policies']['aid']
        plan['recipients'][0]['cash'] = 119
        self.assertIn('RELIEF_FIXED_COST', {i[0] for i in MODULE.check_public_relief(pack, index)})

    def test_foreign_site_and_private_actor_are_rejected(self):
        pack, index = fixture(); scenario = pack['scenarios'][0]
        scenario['initial']['wallets'][0]['owner'] = {'kind': 'actor', 'id': 'player'}
        self.assertIn('RELIEF_PRIVATE_OWNER', {i[0] for i in MODULE.check_public_relief(pack, index)})
        scenario['dynamic_world']['local_economy']['cohorts']['workers']['site_id'] = 'elsewhere'
        self.assertIn('RELIEF_HOUSEHOLD_SOURCE', {i[0] for i in MODULE.check_public_relief(pack, index)})

    def test_duplicate_plan_is_not_one_household_paid_twice(self):
        pack, index = fixture(); recipients = pack['scenarios'][0]['dynamic_world']['public_relief']['policies']['aid']['recipients']
        recipients.append(deepcopy(recipients[0]))
        self.assertIn('RELIEF_DUPLICATE_RECIPIENT', {i[0] for i in MODULE.check_public_relief(pack, index)})

    def test_missing_political_rules_and_boolean_version_fail(self):
        pack, index = fixture(); dynamic = pack['scenarios'][0]['dynamic_world']
        dynamic['politics_governance']['enabled'] = False
        self.assertEqual(MODULE.check_public_relief(pack, index)[0][0], 'RELIEF_DEPENDENCY')
        dynamic['public_relief']['version'] = True
        self.assertEqual(MODULE.check_public_relief(pack, index)[0][0], 'RELIEF_VERSION')


if __name__ == '__main__':
    unittest.main()
