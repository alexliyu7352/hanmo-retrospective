"""生前让职作者门不生成任职事实；原首职和新专项授权须分别完整声明。"""
from copy import deepcopy
import unittest
from test_political_succession_authoring import fixture as original_fixture
from content.political_head_handover_links import NAME, CAPABILITY, COMMANDS, check_political_head_handovers, with_political_head_handover_schema


def fixture():
    """只建立明确原作者条款，无候选接受或Office换任预置。"""
    p = original_fixture()
    p['required_capabilities'].append(CAPABILITY)
    d = p['scenarios'][0]['dynamic_world']
    d[NAME] = {'version': 1, 'policies': {'live': {'id': 'live', 'succession_policy_id': 'succession', 'handover_grant_id': 'live_grant', 'response_ticks': 8}}}
    d['organization']['offices']['chief']['grant_ids'].append('live_grant')
    d['organization']['grants']['live_grant'] = {'id': 'live_grant', 'office_id': 'chief', 'allowed_command_kinds': sorted(COMMANDS), 'scopes': [{'kind': 'site', 'id': 'hall'}], 'wallet_ids': [], 'inventory_ids': [], 'amount_limit': 0}
    return p


class HandoverAuthoring(unittest.TestCase):
    def test_explicit(self):
        self.assertFalse(check_political_head_handovers(fixture(), {}))
    def test_old_profile(self):
        p = fixture(); del p['scenarios'][0]['dynamic_world'][NAME]
        self.assertFalse(check_political_head_handovers(p, {}))
    def test_original_policy(self):
        p = fixture(); del p['scenarios'][0]['dynamic_world']['political_successions']
        self.assertTrue(check_political_head_handovers(p, {}))
    def test_invalid_profile(self):
        for group in ({'version': 2, 'policies': {}}, {'version': True, 'policies': {}}, {'version': 1, 'policies': {}, 'already_accepted': True}):
            p = fixture(); p['scenarios'][0]['dynamic_world'][NAME] = group
            self.assertTrue(check_political_head_handovers(p, {}))
    def test_dedicated_no_budget_grant(self):
        for field, value in [('amount_limit', 1), ('amount_limit', False), ('wallet_ids', ['public']), ('inventory_ids', ['stock']), ('office_id', 'other'), ('allowed_command_kinds', ['political_offer_head_handover'])]:
            p = fixture(); p['scenarios'][0]['dynamic_world']['organization']['grants']['live_grant'][field] = value
            self.assertTrue(check_political_head_handovers(p, {}))
    def test_unique_policy_and_finite_window(self):
        for field, value in [('id', 'other'), ('response_ticks', 0), ('response_ticks', True), ('response_ticks', 2147483648), ('succession_policy_id', 'unknown'), ('handover_grant_id', 'grant')]:
            p = fixture(); p['scenarios'][0]['dynamic_world'][NAME]['policies']['live'][field] = value
            self.assertTrue(check_political_head_handovers(p, {}))
        p = fixture(); policies = p['scenarios'][0]['dynamic_world'][NAME]['policies']; policies['other'] = dict(policies['live'], id='other')
        self.assertTrue(check_political_head_handovers(p, {}))
    def test_schema_is_pure(self):
        s = {'$defs': {'dynamic_world': {'properties': {}}}}; old = deepcopy(s)
        new = with_political_head_handover_schema(s)
        self.assertEqual(s, old); self.assertFalse(new['$defs']['dynamic_world']['properties'][NAME]['additionalProperties'])
        self.assertEqual(new['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'], [1])


if __name__ == '__main__': unittest.main()
