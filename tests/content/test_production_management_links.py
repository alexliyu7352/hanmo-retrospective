"""作者经济门覆盖真实产权/容量冲突，不运行或仿造原生收成。"""
from copy import deepcopy
from pathlib import Path
import importlib.util
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('production_management_links', ROOT / 'tools/content/production_management_links.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture():
    """同一已启用地点拥有一个真实组织资产和引用它的既有macro生产单元。"""
    owner = {'kind': 'organization', 'id': 'city'}
    management = {'version': 1, 'assets': {'land': {'id': 'land', 'site_id': 'town', 'source_key': 'macro.land', 'kind': 'land', 'owner': owner, 'capacity_units': 1}}, 'seasons': {}, 'plans': {'farm': {'unit_id': 'farm', 'season_profile_id': '', 'asset_uses': [{'asset_id': 'land', 'units': 1, 'batches_per_unit': 5}]}}}
    economy = {'production_management': management, 'production_units': {'farm': {'owner': owner, 'site_id': 'town', 'labor_allocation_ids': ['workers'], 'next_due_tick': 1}}, 'production_bindings': {'farm': {'policy_owner': 'macro'}}}
    return {'scenarios': [{'enabled': {'places': ['town']}, 'dynamic_world': {'local_economy': economy}}]}, {'organizations': {'city': {}}, 'actors': {'player': {}}}


class ProductionManagementAuthoring(unittest.TestCase):
    """坏作者数据被定位到原字段，合法输入的观察不会修改候选。"""
    def test_valid_and_legacy_are_unchanged(self):
        pack, index = fixture(); before = deepcopy(pack)
        self.assertEqual(MODULE.check_production_management(pack, index), [])
        self.assertEqual(pack, before)
        self.assertEqual(MODULE.check_production_management({'scenarios': [{'dynamic_world': None}]}, {}), [])

    def test_two_plans_cannot_duplicate_land(self):
        pack, index = fixture(); economy = pack['scenarios'][0]['dynamic_world']['local_economy']; m = economy['production_management']
        economy['production_units']['other'] = deepcopy(economy['production_units']['farm']); economy['production_bindings']['other'] = {'policy_owner': 'macro'}
        m['plans']['other'] = deepcopy(m['plans']['farm']); m['plans']['other']['unit_id'] = 'other'
        self.assertIn('PRODUCTION_ASSET_OVERALLOCATED', [r[0] for r in MODULE.check_production_management(pack, index)])

    def test_owner_and_source_are_not_relabelled(self):
        pack, index = fixture(); m = pack['scenarios'][0]['dynamic_world']['local_economy']['production_management']
        m['assets']['land']['owner'] = {'kind': 'actor', 'id': 'player'}
        m['assets']['duplicate'] = deepcopy(m['assets']['land']); m['assets']['duplicate']['id'] = 'duplicate'
        codes = {r[0] for r in MODULE.check_production_management(pack, index)}
        self.assertTrue({'PRODUCTION_PLAN_OWNERSHIP', 'PRODUCTION_ASSET_SOURCE'} <= codes)

    def test_season_requires_current_coverage_and_finite_factors(self):
        pack, index = fixture(); m = pack['scenarios'][0]['dynamic_world']['local_economy']['production_management']
        m['seasons']['year'] = {'id': 'year', 'origin_tick': 2, 'phase_ticks': 1, 'factors_bp': [20001]}; m['plans']['farm']['season_profile_id'] = 'year'
        codes = {r[0] for r in MODULE.check_production_management(pack, index)}
        self.assertTrue({'PRODUCTION_SEASON_START', 'PRODUCTION_SEASON_BOUNDS'} <= codes)

    def test_boolean_version_is_not_integer_one(self):
        pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['local_economy']['production_management']['version'] = True
        self.assertEqual(MODULE.check_production_management(pack, index)[0][0], 'PRODUCTION_MANAGEMENT_VERSION')

    def test_malformed_asset_use_is_a_diagnostic(self):
        pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['local_economy']['production_management']['plans']['farm']['asset_uses'] = ['broken']
        self.assertEqual(MODULE.check_production_management(pack, index)[0][0], 'PRODUCTION_PLAN_ASSET')


if __name__ == '__main__':
    unittest.main()
