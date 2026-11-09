"""顺序轮作者选择有限门，不替代合包/Installed运行。"""
import copy
import unittest
from content.military_sequential_rounds import check_military_sequential_rounds
class MilitarySequentialAuthorTest(unittest.TestCase):
    def test_disabled_and_exact_pair(self):
        self.assertEqual(check_military_sequential_rounds({"scenarios":[{}]},{}),[])
        p={"required_capabilities":["military_commanded_damage_v1","military_sequential_rounds_v1"],"scenarios":[{"military_commanded_damage":{"profile":"entry_commanded_damage_05_v1"},"military_sequential_rounds":{"profile":"entry_initiative_05_v1"}}]}
        self.assertEqual(check_military_sequential_rounds(p,{}),[])
        for field in ["military_commanded_damage","military_sequential_rounds"]:
            broken=copy.deepcopy(p);broken["scenarios"][0][field]["profile"]="future"
            self.assertTrue(check_military_sequential_rounds(broken,{}))
        broken=copy.deepcopy(p);broken["required_capabilities"].pop();self.assertTrue(check_military_sequential_rounds(broken,{}))
        broken=copy.deepcopy(p);broken["scenarios"][0]["military_sequential_rounds"]["skip_history"]=True;self.assertTrue(check_military_sequential_rounds(broken,{}))
if __name__=="__main__": unittest.main()
