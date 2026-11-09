"""直接消费产品作者检查器，反例不复制另一份准入规则。"""
import copy
import unittest
from content.military_siege_engineering import check_military_siege_engineering

class EngineeringAuthorTest(unittest.TestCase):
    def setUp(self):
        self.policy = {'version': 1, 'profile': 'commander_fortification_05_v1', 'topology_ids': ['topology']}
        self.scenario = {'military_siege_engineering': self.policy, 'military_tactic_learning': {'version': 1}, 'dynamic_world': {'growth_tactic_lessons': {'version': 1}}, 'enabled': {'siege_topologies': ['topology'], 'strongholds': ['fort']}}
        self.pack = {'scenarios': [self.scenario], 'required_capabilities': ['military_siege_engineering_v1', 'military_tactic_learning_v1', 'growth_tactic_lessons_v1']}
        self.index = {'siege_topologies': {'topology': {}}, 'strongholds': {'fort': {'topology_id': 'topology'}}}
    def check(self):
        return check_military_siege_engineering(self.pack, self.index)
    def test_selected(self):
        before = copy.deepcopy(self.pack)
        self.assertEqual(self.check(), [])
        self.assertEqual(before, self.pack)
    def test_old_unselected(self):
        self.scenario.pop('military_siege_engineering')
        self.assertEqual(self.check(), [])
    def test_unknown_version(self):
        for version in (0, 2, True):
            self.policy['version'] = version
            self.assertTrue(self.check())
    def test_missing_learning(self):
        self.scenario.pop('military_tactic_learning')
        self.assertTrue(self.check())
    def test_missing_lesson(self):
        self.scenario['dynamic_world'].clear()
        self.assertTrue(self.check())
    def test_duplicate_or_disabled(self):
        self.policy['topology_ids'].append('topology')
        self.assertTrue(self.check())
        self.policy['topology_ids'] = ['unknown']
        self.assertTrue(self.check())
    def test_no_stronghold(self):
        self.scenario['enabled']['strongholds'] = []
        self.assertTrue(self.check())

if __name__ == '__main__':
    unittest.main()
