"""仅作者准入边界；真实入战源与攻击人数另走原生门。"""
import unittest
from content.military_combat_conditions import check_military_combat_conditions
class MilitaryCombatConditionsAuthorTest(unittest.TestCase):
    def test_disabled(self):self.assertEqual(check_military_combat_conditions({'scenarios':[{}]},{}),[])
    def test_explicit(self):
        s={'military_combat_conditions':{'profile':'entry_fatigue_03_v1'},'military_garrison_rest':{'profile':'supplied_garrison_rest_v1'}}
        p={'scenarios':[s],'required_capabilities':['military_garrison_rest_v1','military_combat_conditions_v1']}
        self.assertEqual(check_military_combat_conditions(p,{}),[])
        s['military_combat_conditions']['multiplier']=20000;self.assertTrue(check_military_combat_conditions(p,{}));s['military_combat_conditions'].pop('multiplier')
        s.pop('military_garrison_rest');self.assertTrue(check_military_combat_conditions(p,{}))
if __name__=='__main__':unittest.main()
