"""固定六叶只读接缝与实际编译登记；源码门不替代原生World和GUI。"""
from pathlib import Path
import subprocess
import unittest
from known_contact_location_registration_candidate import BASE,SHARED,transform
ROOT=Path(__file__).resolve().parents[1]

class KnownContactRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        """直接读取同一Git基准，不消费中央在写的工作树。"""
        cls.source={p:transform(p,subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+p],text=True)) for p in SHARED}
    def leaf(self,name):
        """每个具名共享叶必须唯一。"""
        found=[v for p,v in self.source.items() if p.endswith('/'+name)];self.assertEqual(len(found),1);return found[0]
    def test_read_only_registration(self):
        """没有phase、Command、codec或新段身份更新。"""
        for path in SHARED:self.assertNotIn(Path(path).name,['phase_plan.cpp','dispatcher.cpp','world_codec.cpp','command_types.hpp','native_initial_state.cpp'])
        self.assertIn('query_known_contact_location_world(world',self.leaf('live_view_queries.cpp'))
        backend=self.leaf('native_resident_backend.gd');self.assertIn('observed_letter_delivery_and_report_v2',backend);self.assertIn('witnessed_personal_meetings_v1',backend)
    def test_actual_build_leaves(self):
        """三个CPP在两原编译清单，两个真实测试和GD注册不遗漏。"""
        cmake=self.leaf('resident.cmake')
        for name in ['known_contact_location','known_contact_location_view','known_contact_location_world']:
            self.assertEqual(cmake.count('    src/resident/domains/interaction/known_contacts/'+name+'.cpp\n'),2)
            self.assertEqual(cmake.count('    src/resident/domains/interaction/known_contacts/'+name+'.hpp\n'),1)
        for name in ['known_contact_location','known_contact_message']:self.assertIn('add_test(NAME resident_'+name,cmake)
        self.assertIn("'test_known_contact_location_panel'",self.leaf('verify.py'))
    def test_normal_preload_and_two_places(self):
        """沿原会面/路侧页队列，后继无源时不触发未知查询。"""
        self.assertIn('KnownContactLocationPanel.prepare(host)',self.leaf('resident_page_requests.gd'))
        self.assertEqual(self.leaf('screen_composer.gd').count('KnownContactLocationPanel.build('),2)
        self.assertIn('known_contact_locations_enabled()',self.leaf('resident_page_requests.gd'))

if __name__=='__main__':unittest.main()
