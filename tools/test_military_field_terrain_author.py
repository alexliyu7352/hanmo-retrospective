"""只核实际作者检查器的固定地形和原接敌闭包，不创建World。"""
import copy
import unittest
from content.military_field_terrain import check_military_field_terrain

class MilitaryFieldTerrainAuthorTest(unittest.TestCase):
    def test_selected_mapping(self):
        p={'required_capabilities':['military_field_terrain_v1','military_sequential_rounds_v1'], 'scenarios':[{'military_field_terrain':{'profile':'terrain_and_zones_05_v1','battlefields':[{'approach_battlefield_id':'road','field_battlefield_id':'field'}]},'military_sequential_rounds':{'profile':'entry_initiative_05_v1'},'military_rout_retreat':{'battlefield_ids':['field']},'enabled':{'battlefields':['road','field']},'initial':{'garrisons':[{'units':[{'troop_type_id':'archer'}]}]}}]}
        index={'battlefields':{'road':{'objective_id':'goal'},'field':{'objective_id':'goal'}},'troop_types':{'archer':{'category':'archer','movement':2,'minimum_range':2,'maximum_range':4,'movement_costs':{'plain':1,'road':1,'forest':2,'hill':2,'mud':2,'water':0,'wall':0}}}}
        self.assertEqual(check_military_field_terrain(p,index),[])
        self.assertEqual(check_military_field_terrain({'scenarios':[{}]},{}),[])
        for key,value in [('military_sequential_rounds',{}),('enabled',{'battlefields':['field']}),('military_rout_retreat',{})]:
            bad=copy.deepcopy(p);bad['scenarios'][0][key]=value;self.assertTrue(check_military_field_terrain(bad,index))
        bad=copy.deepcopy(p);bad['scenarios'][0]['military_field_terrain']['battlefields'].append({'approach_battlefield_id':'field','field_battlefield_id':'road'});self.assertTrue(check_military_field_terrain(bad,index))
        bad=copy.deepcopy(index);bad['battlefields']['field']['objective_id']='other';self.assertTrue(check_military_field_terrain(p,bad))
        bad=copy.deepcopy(index);bad['siege_topologies']={'fort':{'nodes':[{'battlefield_id':'field'}]}};self.assertTrue(check_military_field_terrain(p,bad))
        for key,value in [('water',1),('road',2),('mud',1)]:
            bad=copy.deepcopy(index);bad['troop_types']['archer']['movement_costs'][key]=value;self.assertTrue(check_military_field_terrain(p,bad))
        bad=copy.deepcopy(p);bad['required_capabilities'].remove('military_field_terrain_v1');self.assertTrue(check_military_field_terrain(bad,index))

if __name__=='__main__':unittest.main()
