"""见闻作者语义门；不把作者定义当成已发生经历。"""
import copy
import unittest
from tools.content.insight_cards import check_insight_cards


class InsightAuthorTests(unittest.TestCase):
    def setUp(self):
        self.card = {
            'id': 'insight.camp', 'category': 'insight', 'name_key': 'name',
            'description_key': 'description', 'hint_key': 'hint', 'icon_id': '',
            'prerequisites': {'kind': 'current_player_participation'},
            'acquire_condition': {'kind': 'completed_camp_training_v1'},
            'effect_kind': 'record_insight', 'effect_params': {'kind': 'camp_training'},
            'repeat_policy': 'once_per_actor_world',
        }
        self.pack = {'insight_cards': [self.card], 'scenarios': [{
            'dynamic_world': {'profile': 'local_society_v1'},
            'insight_cards': {'profile': 'completed_camp_training_insights_v1',
                              'card_ids': ['insight.camp']}}]}
        self.index = {'texts': {key: key for key in ('name', 'description', 'hint')}}

    def errors(self):
        return {item[0] for item in check_insight_cards(self.pack, self.index)}

    def test_explicit_profile_and_passive_absence(self):
        self.assertEqual(self.errors(), set())
        self.assertEqual(check_insight_cards({}, {}), [])

    def test_unknown_profile_and_empty_selection(self):
        self.pack['scenarios'][0]['insight_cards']['profile'] = 'unversioned'
        self.assertIn('INSIGHT_CARDS_PROFILE', self.errors())
        self.pack['scenarios'][0]['insight_cards'] = {
            'profile': 'completed_camp_training_insights_v1', 'card_ids': []}
        self.assertIn('INSIGHT_CARD_IDS', self.errors())

    def test_unknown_selected_definition(self):
        self.pack['scenarios'][0]['insight_cards']['card_ids'] = ['missing']
        self.assertIn('INSIGHT_CARD_IDS', self.errors())

    def test_old_world_cannot_activate_insights(self):
        del self.pack['scenarios'][0]['dynamic_world']
        self.assertIn('INSIGHT_CARDS_NATIVE', self.errors())

    def test_insight_cannot_grant_office_or_ability(self):
        self.card['effect_kind'] = 'unlock_office'
        self.assertIn('INSIGHT_CARD_EFFECT', self.errors())

    def test_personal_participation_is_required(self):
        self.card['prerequisites'] = {'kind': 'instructor_appointment'}
        self.assertIn('INSIGHT_CARD_PREREQUISITE', self.errors())

    def test_unsupported_and_repeated_condition(self):
        self.card['acquire_condition'] = {'kind': 'visited_camp'}
        self.assertIn('INSIGHT_CARD_CONDITION', self.errors())
        self.card['acquire_condition'] = {'kind': 'completed_camp_training_v1'}
        second = copy.deepcopy(self.card)
        second['id'] = 'duplicate.effect'
        self.pack['insight_cards'].append(second)
        self.pack['scenarios'][0]['insight_cards']['card_ids'].append(second['id'])
        self.assertIn('INSIGHT_CARD_DUPLICATE_CONDITION', self.errors())

    def test_invalid_text_and_extra_effect(self):
        self.card['name_key'] = 'missing'
        self.card['effect_params']['xp_bonus'] = 10
        self.assertTrue({'INSIGHT_CARD_TEXT', 'INSIGHT_CARD_EFFECT'} <= self.errors())

    def test_byte_bounded_ids_and_closed_fields(self):
        self.card['id'] = '字' * 86
        self.assertIn('INSIGHT_CARD_FIELDS', self.errors())
        self.card['id'] = 'insight.camp'
        self.card['granted_office_id'] = 'office'
        self.assertIn('INSIGHT_CARD_FIELDS', self.errors())


if __name__ == '__main__':
    unittest.main()
