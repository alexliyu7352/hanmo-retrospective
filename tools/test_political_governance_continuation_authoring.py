"""续办作者声明窄门，原Task/军事运行仍由实际玩法产生，不预置成事实。"""
import copy
import unittest
from content.political_governance_continuation_links import (
    NAME, CAPABILITY, governance_continuation_grant_declares_assets,
    check_political_governance_continuations, with_governance_continuation_schema)


def fixture():
    grant = {'id': 'g', 'office_id': 'new.office', 'allowed_command_kinds': ['political_continue_governance'],
             'wallet_ids': ['public'], 'inventory_ids': ['stock'], 'amount_limit': 120,
             'scopes': [{'kind': 'site', 'id': 'hall'}], 'continuity_policy_id': 'stop_when_vacant'}
    world = {'profile': 'local_society_v1', NAME: {'version': 1}, 'politics_governance': {'enabled': True},
             'public_relief': {'version': 1},
             'politics': {'jurisdictions': {'j': {'public_wallet_id': 'public', 'public_inventory_ids': ['stock']}}},
             'political_takeover': {'enabled': True, 'version': 1, 'rules': {'r': {'id': 'r', 'jurisdiction_id': 'j', 'site_id': 'hall', 'stronghold_id': 'fort', 'garrison_id': 'camp', 'successor_office_id': 'new.office'}}},
             'organization': {'organizations': {'old': {}, 'new': {}}, 'offices': {'new.office': {'id': 'new.office', 'grant_ids': ['g']}}, 'grants': {'g': grant}}}
    scenario = {'dynamic_world': world, 'initial': {'wallets': [{'id': 'public', 'owner': {'kind': 'organization', 'id': 'old'}}], 'inventories': [{'id': 'stock', 'owner': {'kind': 'organization', 'id': 'old'}}]}}
    index = {'strongholds': {'fort': {'garrison_id': 'camp', 'public_wallet_ids': ['public'], 'public_inventory_ids': ['stock']}}}
    return {'required_capabilities': [CAPABILITY], 'scenarios': [scenario]}, index


class GovernanceContinuationAuthoring(unittest.TestCase):
    def test_valid_explicit_and_old(self):
        pack, index = fixture(); self.assertEqual(check_political_governance_continuations(pack, index), [])
        old = copy.deepcopy(pack); old['scenarios'][0]['dynamic_world'].pop(NAME)
        self.assertEqual(check_political_governance_continuations(old, index), [])
    def test_version_and_no_preseed(self):
        for value in ({'version': True}, {'version': 2}, {'version': 1, 'records': []}):
            pack, index = fixture(); pack['scenarios'][0]['dynamic_world'][NAME] = value
            self.assertTrue(check_political_governance_continuations(pack, index))
    def test_full_dependencies(self):
        for key in ('politics_governance', 'public_relief', 'political_takeover'):
            pack, index = fixture(); pack['scenarios'][0]['dynamic_world'].pop(key)
            self.assertTrue(check_political_governance_continuations(pack, index))
    def test_one_original_wallet_and_stock(self):
        for key, value in (('wallet_ids', ['another']), ('inventory_ids', ['another']), ('inventory_ids', ['stock', 'stock']), ('amount_limit', 0), ('allowed_command_kinds', ['political_continue_governance', 'withdraw_funds'])):
            pack, index = fixture(); scenario = pack['scenarios'][0]; grant = scenario['dynamic_world']['organization']['grants']['g']; grant[key] = value
            self.assertFalse(governance_continuation_grant_declares_assets(scenario, index, grant))
            self.assertTrue(check_political_governance_continuations(pack, index))
    def test_no_private_owner_exception(self):
        pack, index = fixture(); scenario = pack['scenarios'][0]; scenario['initial']['wallets'][0]['owner'] = {'kind': 'actor', 'id': 'old'}
        self.assertTrue(check_political_governance_continuations(pack, index))
    def test_schema_strict_and_pure(self):
        schema = {'$defs': {'dynamic_world': {'properties': {}}}, 'properties': {'required_capabilities': {'items': {'enum': []}}}}
        result = with_governance_continuation_schema(schema)
        self.assertNotIn(NAME, schema['$defs']['dynamic_world']['properties'])
        self.assertFalse(result['$defs']['dynamic_world']['properties'][NAME]['additionalProperties'])
        self.assertIn(CAPABILITY, result['properties']['required_capabilities']['items']['enum'])


if __name__ == '__main__':
    unittest.main()
