"""限定作者准入，不代正规Installed或真实原军级形成/战果。"""
import unittest
from content.military_command_shock import check_military_command_shock
class MilitaryCommandShockAuthorTest(unittest.TestCase):
    def test_old(self): self.assertEqual(check_military_command_shock({'scenarios': [{}]}, {}), [])
    def test_explicit_and_guards(self):
        scenario = {'military_rout_retreat': {'profile': 'two_blocked_rounds_v1'}, 'military_command_shock': {'profile': 'army_commander_once_v1', 'morale_loss': 10}}
        pack = {'scenarios': [scenario], 'required_capabilities': ['military_rout_retreat_v1', 'military_command_shock_v1']}
        self.assertEqual(check_military_command_shock(pack, {}), [])
        for value in (True, 9, 20):
            scenario['military_command_shock']['morale_loss'] = value
            self.assertTrue(check_military_command_shock(pack, {}))
        scenario['military_command_shock']['morale_loss'] = 10
        scenario.pop('military_rout_retreat')
        self.assertTrue(check_military_command_shock(pack, {}))
if __name__ == '__main__': unittest.main()
