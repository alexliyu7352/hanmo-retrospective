"""家庭作者入口覆盖私产混用、重复责任和有限供养期限。"""
from copy import deepcopy
from pathlib import Path
import importlib.util
import unittest

SPEC = importlib.util.spec_from_file_location('household_provision_links', Path(__file__).resolve().parents[2] / 'tools/content/household_provision_links.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def fixture():
    """具名家庭的原库存、原ration与原binding完整同源，未声明任何人物代理权。"""
    subject = {'kind': 'cohort', 'id': 'workers'}
    authorization = {'id': 'self', 'household_group_id': 'family', 'cohort_id': 'workers', 'obligation_id': 'food', 'provision_binding_id': 'binding', 'inventory_id': 'private', 'item_id': 'grain', 'rate_profile_id': 'rate'}
    economy = {'enabled': True, 'household_provisioning': {'version': 1, 'authorizations': {'self': authorization}}, 'households': {'family': {'inventory_id': 'private'}}, 'cohorts': {'workers': {'household_group_id': 'family', 'private_inventory_id': 'private'}}, 'rates': {'rate': {'per_capita': True, 'numerator': 1, 'denominator': 1}}, 'obligations': {'food': {'kind': 'ration', 'principal': subject, 'beneficiary': subject, 'source_wallet_or_inventory': 'private', 'authority_ref': 'self', 'item_id': 'grain', 'rate_profile': 'rate', 'destination': '', 'active': True, 'period_ticks': 1, 'next_due_tick': 1}}, 'provision_bindings': {'binding': {'subject': subject, 'obligation_id': 'food', 'payer_inventory': 'private', 'policy_owner': 'macro', 'from_tick': 0, 'through_tick': 2}}}
    scenario = {'dynamic_world': {'local_economy': economy}, 'initial': {'inventories': [{'id': 'private', 'owner': {'kind': 'household', 'id': 'family'}}]}}
    return {'scenarios': [scenario]}, {'items': {'grain': {}}}


class HouseholdProvisionAuthoring(unittest.TestCase):
    """诊断失败不能成为机构检查的绕过名单，读取也不能修改作者原件。"""
    def test_valid_is_immutable_and_legacy_stays_empty(self):
        pack, index = fixture(); before = deepcopy(pack)
        self.assertEqual(MODULE.check_household_provision(pack, index), [])
        self.assertEqual(MODULE.household_ration_authorizations(pack['scenarios'][0], index), {'food'})
        self.assertEqual(pack, before)
        self.assertEqual(MODULE.check_household_provision({'scenarios': [{'dynamic_world': None}]}, {}), [])

    def test_public_or_other_family_grain_is_denied(self):
        pack, index = fixture(); pack['scenarios'][0]['initial']['inventories'][0]['owner'] = {'kind': 'organization', 'id': 'city'}
        self.assertIn('HOUSEHOLD_PROVISION_OWNER', {row[0] for row in MODULE.check_household_provision(pack, index)})
        self.assertEqual(MODULE.household_ration_authorizations(pack['scenarios'][0], index), set())

    def test_office_grant_cannot_replace_household_source(self):
        pack, index = fixture(); pack['scenarios'][0]['dynamic_world']['local_economy']['obligations']['food']['authority_ref'] = 'mayor.grant'
        self.assertIn('HOUSEHOLD_PROVISION_OBLIGATION', {row[0] for row in MODULE.check_household_provision(pack, index)})

    def test_duplicate_and_unbounded_period_are_denied(self):
        pack, index = fixture(); economy = pack['scenarios'][0]['dynamic_world']['local_economy']; section = economy['household_provisioning']
        section['authorizations']['copy'] = deepcopy(section['authorizations']['self']); section['authorizations']['copy']['id'] = 'copy'
        economy['provision_bindings']['binding']['through_tick'] = 9223372036854775807
        codes = {row[0] for row in MODULE.check_household_provision(pack, index)}
        self.assertTrue({'HOUSEHOLD_PROVISION_DUPLICATE', 'HOUSEHOLD_PROVISION_INTERVAL'} <= codes)

    def test_malformed_references_and_boolean_version_are_diagnostics(self):
        pack, index = fixture(); section = pack['scenarios'][0]['dynamic_world']['local_economy']['household_provisioning']; section['authorizations']['self']['inventory_id'] = []
        self.assertEqual(MODULE.check_household_provision(pack, index)[0][0], 'HOUSEHOLD_PROVISION_RECORD')
        section['version'] = True
        self.assertEqual(MODULE.check_household_provision(pack, index)[0][0], 'HOUSEHOLD_PROVISION_VERSION')


if __name__ == '__main__':
    unittest.main()
