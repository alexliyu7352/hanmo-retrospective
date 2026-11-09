"""Military9独立作者政策边界，不将来源检查冒充正规compose。"""
import copy
import unittest
from content.military_supply_attrition import check_military_supply_attrition


class MilitarySupplyAttritionAuthor(unittest.TestCase):
    def setUp(self):
        self.pack = {'required_capabilities': ['military_supply_attrition_v1', 'military_wounded_recovery_v1'],
                     'scenarios': [{'military_wounded_recovery': {'profile': 'supplied_wound_batches_v1'},
                                    'personal_injury_recovery': {'version': 1},
                                    'military_supply_attrition': {'profile': 'anonymous_shortage_v1',
                                        'shortage_threshold_half_days': 2, 'desertion_basis_points': 200,
                                        'morale_loss': 10}}]}

    def test_explicit_policy(self):
        self.assertEqual(check_military_supply_attrition(self.pack, {}), [])

    def test_old_disabled(self):
        self.assertEqual(check_military_supply_attrition({'scenarios': [{}]}, {}), [])

    def test_missing_real_recovery(self):
        del self.pack['scenarios'][0]['personal_injury_recovery']
        self.assertTrue(check_military_supply_attrition(self.pack, {}))

    def test_no_generic_named_losses_or_future_policy(self):
        for key, value in [('profile', 'all_named_v1'), ('desertion_basis_points', 10000),
                           ('shortage_threshold_half_days', True), ('morale_loss', 8)]:
            with self.subTest(key=key):
                pack = copy.deepcopy(self.pack)
                pack['scenarios'][0]['military_supply_attrition'][key] = value
                self.assertTrue(check_military_supply_attrition(pack, {}))


if __name__ == '__main__':
    unittest.main()
