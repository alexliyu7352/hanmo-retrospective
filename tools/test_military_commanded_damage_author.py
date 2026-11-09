"""作者只验固定公式/完整前置/原条件，不替代World人物历史与实际编军。"""
import unittest
from content.military_commanded_damage import check_military_commanded_damage
class MilitaryCommandedDamageAuthorTest(unittest.TestCase):
    def test_disabled(self):
        self.assertEqual(check_military_commanded_damage({"scenarios":[{}]},{}),[])
    def test_complete_and_reject_changes(self):
        s={"military_commanded_damage":{"profile":"entry_commanded_damage_05_v1"},"military_combat_conditions":{"profile":"entry_fatigue_03_v1"},"enabled":{"garrisons":["camp"]}}
        p={"scenarios":[s],"required_capabilities":["military_commanded_damage_v1","military_combat_conditions_v1"]}
        index={"garrisons":{"camp":{"units":[{"initial_training":20,"initial_fatigue":0}]}}}
        self.assertEqual(check_military_commanded_damage(p,index),[])
        s["military_commanded_damage"]["multiplier"]=20000
        self.assertTrue(check_military_commanded_damage(p,index));s["military_commanded_damage"].pop("multiplier")
        index["garrisons"]["camp"]["units"][0].pop("initial_training")
        self.assertTrue(check_military_commanded_damage(p,index))
        s.pop("military_combat_conditions")
        self.assertTrue(check_military_commanded_damage(p,index))
if __name__=="__main__":unittest.main()
