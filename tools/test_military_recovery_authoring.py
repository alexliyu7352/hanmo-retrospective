"""有限政策准入源门；不冒伤亡、个人治疗或整World运行。"""
import copy
import unittest
from tools.content.military_recovery_links import check_military_wounded_recovery


class MilitaryRecoveryAuthoringTests(unittest.TestCase):
    """每个坏输入只改一条作者前置，原有效营地和原供给不变。"""
    def setUp(self):
        self.index = {'garrisons': {'camp': {'ration_per_person': 1}}}
        self.pack = {'required_capabilities': ['military', 'military_wounded_recovery_v1'], 'scenarios': [{
            'enabled': {'garrisons': ['camp']}, 'dynamic_world': {'profile': 'local_society_v1'},
            'military_officer_assignments': {'profile': 'political_appointment_v1'},
            'military_field_commands': {'profile': 'casualty_appointment_v1'},
            'personal_injury_recovery': {'version': 1},
            'military_wounded_recovery': {'profile': 'supplied_wound_batches_v1', 'garrison_ids': ['camp']}}]}

    def test_explicit_complete_source(self):
        before = copy.deepcopy(self.pack)
        self.assertEqual(check_military_wounded_recovery(self.pack, self.index), [])
        self.assertEqual(self.pack, before)

    def test_missing_personal_source_rejects(self):
        del self.pack['scenarios'][0]['personal_injury_recovery']
        self.assertTrue(check_military_wounded_recovery(self.pack, self.index))

    def test_zero_ration_and_unselected_camp_reject(self):
        self.index['garrisons']['camp']['ration_per_person'] = 0
        self.assertTrue(check_military_wounded_recovery(self.pack, self.index))
        self.index['garrisons']['camp']['ration_per_person'] = 1
        self.pack['scenarios'][0]['enabled']['garrisons'] = []
        self.assertTrue(check_military_wounded_recovery(self.pack, self.index))

    def test_old_profile_stays_disabled(self):
        del self.pack['scenarios'][0]['military_wounded_recovery']
        self.assertEqual(check_military_wounded_recovery(self.pack, self.index), [])


if __name__ == '__main__':
    unittest.main()
