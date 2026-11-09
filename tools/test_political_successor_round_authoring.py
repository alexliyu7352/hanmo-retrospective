"""新轮作者门不生成提名或任职事实；原首职和新专项授权须分别完整声明。"""
from copy import deepcopy
import unittest
from test_political_succession_authoring import fixture as original_fixture
from content.political_successor_round_links import NAME, CAPABILITY, COMMANDS, check_political_successor_rounds, with_political_successor_round_schema


def fixture():
    """只建立明确原作者条款，无候选接受或Office换任预置。"""
    p = original_fixture()
    p['required_capabilities'].append(CAPABILITY)
    d = p['scenarios'][0]['dynamic_world']
    d[NAME] = {'version': 1, 'policies': {'live': {'id': 'live', 'succession_policy_id': 'succession', 'nomination_grant_id': 'live_grant', 'response_ticks': 8, 'accession_ticks': 16}}}
    d['organization']['offices']['chief']['grant_ids'].append('live_grant')
    d['organization']['grants']['live_grant'] = {'id': 'live_grant', 'office_id': 'chief', 'allowed_command_kinds': sorted(COMMANDS), 'scopes': [{'kind': 'site', 'id': 'hall'}], 'wallet_ids': [], 'inventory_ids': [], 'amount_limit': 0}
    return p


class SuccessorRoundAuthoring(unittest.TestCase):
    def test_explicit(self):
        self.assertFalse(check_political_successor_rounds(fixture(), {}))
    def test_old_profile(self):
        p = fixture(); del p['scenarios'][0]['dynamic_world'][NAME]
        self.assertFalse(check_political_successor_rounds(p, {}))
    def test_original_policy(self):
        p = fixture(); del p['scenarios'][0]['dynamic_world']['political_successions']
        self.assertTrue(check_political_successor_rounds(p, {}))
    def test_invalid_profile(self):
        for group in ({'version': 2, 'policies': {}}, {'version': True, 'policies': {}}, {'version': 1, 'policies': {}, 'already_accepted': True}):
            p = fixture(); p['scenarios'][0]['dynamic_world'][NAME] = group
            self.assertTrue(check_political_successor_rounds(p, {}))
    def test_dedicated_no_budget_grant(self):
        for field, value in [('amount_limit', 1), ('amount_limit', False), ('wallet_ids', ['public']), ('inventory_ids', ['stock']), ('office_id', 'other'), ('allowed_command_kinds', ['political_offer_head_handover'])]:
            p = fixture(); p['scenarios'][0]['dynamic_world']['organization']['grants']['live_grant'][field] = value
            self.assertTrue(check_political_successor_rounds(p, {}))
    def test_unique_policy_and_finite_window(self):
        for field, value in [('id', 'other'), ('accession_ticks', 0), ('accession_ticks', True), ('response_ticks', 0), ('response_ticks', True), ('response_ticks', 2147483648), ('succession_policy_id', 'unknown'), ('nomination_grant_id', 'grant')]:
            p = fixture(); p['scenarios'][0]['dynamic_world'][NAME]['policies']['live'][field] = value
            self.assertTrue(check_political_successor_rounds(p, {}))
        p = fixture(); policies = p['scenarios'][0]['dynamic_world'][NAME]['policies']; policies['other'] = dict(policies['live'], id='other')
        self.assertTrue(check_political_successor_rounds(p, {}))
    def test_normal_second_controller(self):
        """真实第二次本人接续有独立当面条款，不依靠预置亲属或政治接受。"""
        import json
        from pathlib import Path
        content=json.loads((Path(__file__).resolve().parents[1]/'content/packs/authored/political_successor_rounds_1_0/content.json').read_text())
        scenario=content['scenarios'][0];clerk='actor.next_clerk'
        self.assertIn(clerk,scenario['enabled']['actors'])
        self.assertTrue(any(a['actor_id']==clerk and a['age_years']>=18 for a in scenario['initial']['actors']))
        self.assertTrue(any(r['adopter_actor_id']=='society::actor.apprentice' and r['adoptee_actor_id']==clerk for r in content['adult_adoption_requests']))
        self.assertTrue(any(r['successor_actor_id']==clerk and r['personal']=={'money':0,'goods':[]} for r in content['succession_consent_requests']))
        self.assertTrue(any(r['decedent_actor_id']=='society::actor.apprentice' for r in scenario['task_message_notices']['bindings']))
        self.assertNotIn('family',scenario['initial']);self.assertNotIn('political_successor_rounds',scenario['initial'])

    def test_schema_is_pure(self):
        s = {'$defs': {'dynamic_world': {'properties': {}}}}; old = deepcopy(s)
        new = with_political_successor_round_schema(s)
        self.assertEqual(s, old); self.assertFalse(new['$defs']['dynamic_world']['properties'][NAME]['additionalProperties'])
        self.assertEqual(new['$defs']['dynamic_world']['properties'][NAME]['properties']['version']['enum'], [1])


if __name__ == '__main__': unittest.main()
