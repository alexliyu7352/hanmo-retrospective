"""资源作者有限门：完整前置与真实军种范围，不代Installed或World。"""
import copy
import unittest
from content.military_command_resources import check_military_command_resources
class CommandResourcesAuthorTest(unittest.TestCase):
    def test_exact_and_rejections(self):
        self.assertEqual(check_military_command_resources({"scenarios":[{}]},{}),[])
        p={"required_capabilities":["military_command_resources_v1","military_sequential_rounds_v1"],"scenarios":[{"military_sequential_rounds":{"profile":"entry_initiative_05_v1"},"military_command_resources":{"profile":"entry_tactics_recovery_05_v1"},"enabled":{"garrisons":["camp"]}}]}
        index={"garrisons":{"camp":{"units":[{"troop_type_id":"foot"}]}},"troop_types":{"foot":{"tactic_capacity":100}}}
        self.assertEqual(check_military_command_resources(p,index),[])
        for field in ["military_sequential_rounds","military_command_resources"]:
            bad=copy.deepcopy(p);bad["scenarios"][0][field]["profile"]="future";self.assertTrue(check_military_command_resources(bad,index))
        bad=copy.deepcopy(p);bad["required_capabilities"].pop();self.assertTrue(check_military_command_resources(bad,index))
        bad=copy.deepcopy(index);bad["troop_types"]["foot"]["tactic_capacity"]=8;self.assertTrue(check_military_command_resources(p,bad))
        bad=copy.deepcopy(p);bad["scenarios"][0]["military_command_resources"]["skip_history"]=True;self.assertTrue(check_military_command_resources(bad,index))
if __name__=="__main__":unittest.main()
