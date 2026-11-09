"""正常组合后的同一作者数据，关键禁用/授权/辖区/模板边界不静默放宽。"""
import copy,json,sys,unittest
from pathlib import Path
from content.treaty_passage_links import check_treaty_passage
class TreatyAuthoringTest(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  data=json.loads(CATALOG_PATH.read_text())['catalog'];cls.index=data;cls.pack={k:list(v.values()) if k!='texts' and isinstance(v,dict) else v for k,v in data.items()};cls.number=next(i for i,s in enumerate(cls.pack['scenarios']) if 'treaty_passage' in s.get('dynamic_world',{}))
 def setUp(self):self.pack=copy.deepcopy(self.pack);self.world=self.pack['scenarios'][self.number]['dynamic_world']
 def codes(self):return [r[0] for r in check_treaty_passage(self.pack,self.index)]
 def test_actual_composed(self):self.assertEqual(self.codes(),[])
 def test_profile_missing(self):del self.world['treaty_passage'];self.assertIn('TREATY_PASSAGE_PROFILE',self.codes())
 def test_ratifier_grant_missing(self):self.world['organization']['grants']['grant.treaty.sect']['allowed_command_kinds']=[];self.assertIn('TREATY_PASSAGE_GRANT',self.codes())
 def test_place_binding_missing(self):self.world['treaty_passage']['provisions']['passage.qingchuan_sword_yard']['places'][0]['jurisdiction_id']='missing';self.assertIn('TREATY_PASSAGE_JURISDICTION',self.codes())
 def test_npc_wrong_office(self):self.world['treaty_passage']['provisions']['passage.qingchuan_sword_yard']['npc_ratifier_office_ids']=['missing'];self.assertIn('TREATY_PASSAGE_NPC',self.codes())
 def test_legacy_director(self):self.world['director'].pop('registry_version');self.assertIn('TREATY_PASSAGE_DIRECTOR',self.codes())
if __name__=='__main__':
 CATALOG_PATH=Path(sys.argv[1]);unittest.main(argv=[sys.argv[0]])
