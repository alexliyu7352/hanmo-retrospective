"""正常原署委任作者的可达声明；结构通过不冒实际 NPC 或 World 已运行。"""
import json
from pathlib import Path
import unittest

from content.political_governance_intents_links import check_political_governance_intents

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'content/packs/authored/governance_intents_1_0'


class GovernanceIntentsPack(unittest.TestCase):
    """保原人物、原预算和两次本人办理，不预置自动委任的运行结果。"""
    def setUp(self):
        """每例重新读取实际提交作者，不使用另一份近似情景。"""
        self.content = json.loads((PACK / 'content.json').read_text())
        self.scenario = self.content['scenarios'][0]
        self.world = self.scenario['dynamic_world']

    def test_actual_declared_sources(self):
        actors = {actor: {} for actor in self.scenario['enabled']['actors']}
        self.assertEqual(check_political_governance_intents(self.content, {'actors': actors}), [])
        intent = next(iter(self.world['director']['political_governance_intents']['intents'].values()))
        organization = self.world['organization']
        office = organization['offices'][intent['office_id']]
        self.assertEqual(office['holder_actor_id'], intent['issuer_actor_id'])
        self.assertTrue(any(m['actor_id'] == intent['executor_actor_id'] and m['response'] == 'serving'
                            for m in organization['members'].values()))

    def test_only_original_terms_and_no_runtime_seed(self):
        original = json.loads((ROOT / 'content/packs/authored/political_economy_1_2/content.json').read_text())
        original = next(s for s in original['scenarios'] if s['id'] == 'scenario.qingchuan_governance')
        for key in ('politics_governance', 'public_relief', 'local_economy', 'politics'):
            self.assertEqual(self.world[key], original['dynamic_world'][key])
        for key in ('wallets', 'inventories'):
            self.assertEqual(self.scenario['initial'][key], original['initial'][key])
        self.assertEqual(self.scenario['initial']['offer_ids'], [])
        self.assertNotIn('political_governance_continuations', self.world)
        self.assertNotIn('tasks', self.scenario['initial'])
        self.assertNotIn('facts', self.scenario['initial'])

    def test_actual_two_subjects_and_original_budget(self):
        director = self.world['director']
        self.assertEqual(director['decision_budget'], 4)
        for role, actor in (('issuer', 'society::actor.city_steward'),
                            ('executor', 'society::actor.home_keeper')):
            admission = director['admissions']['a.governance_' + role]
            self.assertEqual(admission['minimum_age_years'], 18)
            eligible = set(self.scenario['enabled']['actors']) - set(admission['excluded_actor_ids'])
            self.assertEqual(eligible, {actor})
            self.assertEqual(director['policies'][admission['policy_id']]['templates'],
                             ['maintain_commitment', 'governance_commission'])

    def test_player_is_not_either_npc(self):
        self.assertEqual(self.scenario['playable_actor_ids'], ['society::actor.apprentice'])
        self.assertEqual(self.scenario['enabled']['scenarios'], [self.scenario['id']])
        for field in ('playable_actor_appearance_policies', 'player_start_descriptions'):
            self.assertEqual([v['actor_id'] for v in self.scenario[field]], self.scenario['playable_actor_ids'])


if __name__ == '__main__':
    unittest.main()
