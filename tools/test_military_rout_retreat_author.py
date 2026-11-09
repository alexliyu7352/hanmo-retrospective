"""仅作者准入及旧禁用源门，不替代Installed场景或真实战果。"""
import copy
import unittest
from content.military_rout_retreat import check_military_rout_retreat

class MilitaryRoutAuthorTest(unittest.TestCase):
    def setUp(self):
        self.index = {'battlefields': {'field': {'exits': [{'side': 0}, {'side': 1}]}}}
        self.scenario = {'enabled': {'battlefields': ['field']}, 'military_supply_attrition': {'profile': 'anonymous_shortage_v1'},
                         'military_rout_retreat': {'profile': 'two_blocked_rounds_v1', 'blocked_rounds_before_capture': 2, 'battlefield_ids': ['field']}}
        self.pack = {'required_capabilities': ['military_rout_retreat_v1', 'military_supply_attrition_v1'], 'scenarios': [self.scenario]}
    def test_explicit(self): self.assertEqual(check_military_rout_retreat(self.pack, self.index), [])
    def test_old_unchanged(self): self.assertEqual(check_military_rout_retreat({'scenarios': [{}]}, self.index), [])
    def test_blocked_not_permission(self):
        self.scenario['military_rout_retreat']['blocked_rounds_before_capture'] = 1
        self.assertTrue(check_military_rout_retreat(self.pack, self.index))
    def test_future_or_incomplete(self):
        self.scenario.pop('military_supply_attrition')
        self.assertTrue(check_military_rout_retreat(self.pack, self.index))
    def test_unknown_disabled_and_missing_exit(self):
        for action in ('unknown','disabled','exit'):
            index = copy.deepcopy(self.index);pack = copy.deepcopy(self.pack)
            if action == 'unknown': index['battlefields'].clear()
            if action == 'disabled': pack['scenarios'][0]['enabled']['battlefields'] = []
            if action == 'exit': index['battlefields']['field']['exits'].pop()
            self.assertTrue(check_military_rout_retreat(pack,index))
if __name__ == '__main__': unittest.main()
