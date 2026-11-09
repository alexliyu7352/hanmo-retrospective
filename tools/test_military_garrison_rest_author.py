"""只核作者字段，正规compose与Installed保持独立验收。"""
import copy
import unittest
from content.military_garrison_rest import check_military_garrison_rest
class MilitaryGarrisonRestAuthorTest(unittest.TestCase):
    def test_old(self):
        self.assertEqual(check_military_garrison_rest({'scenarios':[{}]},{}),[])
    def test_explicit(self):
        scenario={'military_garrison_rest':{'profile':'supplied_garrison_rest_v1','garrison_ids':['camp'],'fatigue_reduction':10},'military_wounded_recovery':{'garrison_ids':['camp']},'military_command_shock':{'profile':'army_commander_once_v1'}}
        pack={'scenarios':[scenario],'required_capabilities':['military_command_shock_v1','military_garrison_rest_v1']}
        self.assertEqual(check_military_garrison_rest(pack,{}),[])
        for change in ({'fatigue_reduction':True},{'fatigue_reduction':20},{'garrison_ids':['camp','camp']},{'garrison_ids':['elsewhere']},{'garrison_ids':[]}):
            bad=copy.deepcopy(pack);bad['scenarios'][0]['military_garrison_rest'].update(change)
            self.assertTrue(check_military_garrison_rest(bad,{}))
        for key in ('military_command_shock','military_wounded_recovery'):
            bad=copy.deepcopy(pack);bad['scenarios'][0].pop(key);self.assertTrue(check_military_garrison_rest(bad,{}))
if __name__=='__main__':unittest.main()
