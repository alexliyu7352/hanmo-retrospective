"""新重建作者门：只证明明确准入，不冒行为/World验收。"""
import unittest
from copy import deepcopy
from content.political_governance_invitation_links import NAME, CAPABILITY, check_governance_invitation_replies, with_governance_invitation_schema


def fixture():
    """完整作者声明仍无预置邀请或Task。"""
    return {'required_capabilities': [CAPABILITY], 'scenarios': [{
        'family_life': {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'},
        'dynamic_world': {'profile': 'local_society_v1', NAME: {'version': 1}, 'politics': {'jurisdictions': {'court': {}}},
            'politics_governance': {'enabled': True}, 'public_relief': {'version': 1},
            'organization': {'grants': {'grant': {'may_delegate': True, 'allowed_command_kinds': ['political_commission_governance', 'political_withdraw_governance_invitation']}}}}}]}


class Authoring(unittest.TestCase):
    def test_explicit(self):
        self.assertFalse(check_governance_invitation_replies(fixture(), {}))
    def test_old(self):
        pack = fixture(); del pack['scenarios'][0]['dynamic_world'][NAME]
        self.assertFalse(check_governance_invitation_replies(pack, {}))
    def test_version_and_payload(self):
        for group in ({'version': True}, {'version': 2}, {'version': 1, 'replies': {}}):
            pack = fixture(); pack['scenarios'][0]['dynamic_world'][NAME] = group
            self.assertTrue(check_governance_invitation_replies(pack, {}))
    def test_dependencies(self):
        for key in ('politics', 'politics_governance', 'public_relief'):
            pack = fixture(); del pack['scenarios'][0]['dynamic_world'][key]
            self.assertTrue(check_governance_invitation_replies(pack, {}))
        pack = fixture(); del pack['scenarios'][0]['family_life']
        self.assertTrue(check_governance_invitation_replies(pack, {}))
    def test_capability(self):
        pack = fixture(); pack['required_capabilities'] = []
        self.assertTrue(check_governance_invitation_replies(pack, {}))
    def test_exact_original_grant(self):
        pack = fixture(); pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['allowed_command_kinds'] = ['political_withdraw_governance_invitation']
        self.assertTrue(check_governance_invitation_replies(pack, {}))
    def test_decline_only_needs_no_withdrawal_power(self):
        pack = fixture(); pack['scenarios'][0]['dynamic_world']['organization']['grants']['grant']['allowed_command_kinds'] = ['political_commission_governance']
        self.assertFalse(check_governance_invitation_replies(pack, {}))
    def test_schema_pure(self):
        source = {'$defs': {'dynamic_world': {'properties': {}}}}
        original = deepcopy(source); result = with_governance_invitation_schema(source)
        self.assertEqual(source, original)
        self.assertFalse(result['$defs']['dynamic_world']['properties'][NAME]['additionalProperties'])

if __name__ == '__main__': unittest.main()
