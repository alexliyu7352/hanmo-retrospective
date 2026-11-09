"""财政作者门验证真实来源与开发参数，未启用政策也不默认制造收入。"""
from copy import deepcopy
from pathlib import Path
import importlib.util
import unittest

SPEC = importlib.util.spec_from_file_location('fiscal_settlement_links', Path(__file__).resolve().parents[2] / 'tools/content/fiscal_settlement_links.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture():
    """一个同地家庭的原生产者与真实初态容器，税额和留粮都显式声明。"""
    owner = {'kind': 'household', 'id': 'family'}
    source = {'id': 'family.tax', 'taxpayer': {'kind': 'cohort', 'id': 'workers'}, 'wallet_id': 'private.money', 'inventory_id': 'private.food', 'grain_item_id': 'grain', 'production_unit_ids': ['farm'], 'base_cash_due': 20, 'grain_share_bp': 5000, 'civilian_grain_reserve': 4}
    district = {'id': 'district', 'site_id': 'town', 'period_ticks': 2, 'first_due_tick': 2, 'initial_tax_base_bp': 10000, 'normal_recovery_bp': 100, 'sources': {'family.tax': source}}
    economy = {'enabled': True, 'households': {'family': {'wallet_id': 'private.money', 'inventory_id': 'private.food'}}, 'cohorts': {'workers': {'household_group_id': 'family', 'site_id': 'town', 'private_wallet_id': 'private.money', 'private_inventory_id': 'private.food'}}, 'production_units': {'farm': {'owner': owner, 'site_id': 'town', 'output_inventory_id': 'private.food'}}, 'production_bindings': {'farm': {'policy_owner': 'macro'}}}
    scenario = {'enabled': {'places': ['town']}, 'initial': {'wallets': [{'id': 'private.money', 'owner': owner}], 'inventories': [{'id': 'private.food', 'owner': owner}]}, 'dynamic_world': {'local_economy': economy, 'fiscal_settlement': {'version': 1, 'districts': {'district': district}}}}
    return {'scenarios': [scenario]}, {'items': {'grain': {}}}


class FiscalSettlementAuthoring(unittest.TestCase):
    """只诊断已有作者来源，不修改财政日程、政策或实际资产。"""
    def test_valid_and_legacy_do_not_mutate(self):
        pack, index = fixture(); before = deepcopy(pack)
        self.assertEqual(MODULE.check_fiscal_settlement(pack, index), [])
        self.assertEqual(pack, before)
        self.assertEqual(MODULE.check_fiscal_settlement({'scenarios': [{'dynamic_world': None}]}, {}), [])

    def test_actor_wallet_is_never_taxpayer_source(self):
        pack, index = fixture(); scenario = pack['scenarios'][0]
        scenario['dynamic_world']['fiscal_settlement']['districts']['district']['sources']['family.tax']['taxpayer'] = {'kind': 'actor', 'id': 'player'}
        scenario['initial']['wallets'][0]['owner'] = {'kind': 'actor', 'id': 'player'}
        self.assertIn('FISCAL_SOURCE_OWNER', {r[0] for r in MODULE.check_fiscal_settlement(pack, index)})

    def test_source_cannot_be_taxed_by_two_districts(self):
        pack, index = fixture(); districts = pack['scenarios'][0]['dynamic_world']['fiscal_settlement']['districts']
        districts['copy'] = deepcopy(districts['district']); districts['copy']['id'] = 'copy'
        codes = {r[0] for r in MODULE.check_fiscal_settlement(pack, index)}
        self.assertTrue({'FISCAL_DISTRICT_SITE', 'FISCAL_DUPLICATE_SOURCE', 'FISCAL_PRODUCTION_OWNER'} <= codes)

    def test_foreign_production_cannot_become_tax_grain(self):
        pack, index = fixture(); economy = pack['scenarios'][0]['dynamic_world']['local_economy']
        economy['production_units']['farm']['owner'] = {'kind': 'organization', 'id': 'city'}
        self.assertIn('FISCAL_PRODUCTION_OWNER', {r[0] for r in MODULE.check_fiscal_settlement(pack, index)})

    def test_recovery_and_period_are_bounded_declared_parameters(self):
        pack, index = fixture(); district = pack['scenarios'][0]['dynamic_world']['fiscal_settlement']['districts']['district']
        district['normal_recovery_bp'] = 301; district['first_due_tick'] = 1
        self.assertIn('FISCAL_DISTRICT_PARAMETERS', {r[0] for r in MODULE.check_fiscal_settlement(pack, index)})

    def test_boolean_version_and_bad_reference_are_diagnostics(self):
        pack, index = fixture(); section = pack['scenarios'][0]['dynamic_world']['fiscal_settlement']
        section['version'] = True
        self.assertEqual(MODULE.check_fiscal_settlement(pack, index)[0][0], 'FISCAL_SETTLEMENT_VERSION')
        section['version'] = 1; section['districts']['district']['sources']['family.tax']['production_unit_ids'] = [[]]
        self.assertIn('FISCAL_PRODUCTION_LIST', {r[0] for r in MODULE.check_fiscal_settlement(pack, index)})


if __name__ == '__main__':
    unittest.main()
