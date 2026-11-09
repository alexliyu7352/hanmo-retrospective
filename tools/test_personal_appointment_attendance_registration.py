"""模板15的原生产/调度来源契约；仅源门，不代真实NPC行旅与World回滚。"""
import json,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from personal_appointment_attendance_candidate import BASE,SHARED,transform
class AttendanceRegistrationTest(unittest.TestCase):
    """固定原件全部共享叶转化，禁止滚动中央脏源混入。"""
    @classmethod
    def setUpClass(cls):
        """按同Git对象建立轻量源码视图。"""
        cls.source={p:transform(p,subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+p]).decode()) for p in SHARED}
    def leaf(self,name):
        """具名唯一文件读取。"""
        values=[v for k,v in self.source.items() if k.endswith('/'+name)];self.assertEqual(1,len(values));return values[0]
    def test_explicit_numeric_identity(self):
        """保原13并独立15，不占预留14；旧visitor未触。"""
        self.assertIn('visit_reception=13, personal_appointment_attendance=15',self.leaf('director_model.hpp'))
        self.assertFalse(any('/director_codec_checkpoint' in p for p in SHARED))
        codec=self.leaf('director_codec.cpp');self.assertIn('supply,reception,attendance_enabled)',codec);self.assertIn('AppointmentAttendanceProfileMismatch',codec)
    def test_real_preflight_before_first_decode(self):
        """第一次Director解码前用完整原目录，不把晚marker当准入替身。"""
        s=self.leaf('native_initial_state.cpp');self.assertLess(s.index('validate_personal_appointment_attendance_author_source(scenario'),s.index('auto response_director='));self.assertIn('reception_catalog,attendance.value)',s)
        s=self.leaf('world_codec.cpp');self.assertIn('validate_personal_appointment_attendance_catalog(*director.value.personal_appointment_attendance,*result->personal_appointments)',s)
    def test_marker_order_and_no_second_state(self):
        """三producer都在原康复尾之后追加纯目录身份，无第二预约段。"""
        for name,old,new in [('native_initial_state.cpp','admitted.module_versions.emplace_back("personal_injury_recovery"','admitted.module_versions.emplace_back("personal_appointment_attendance"'),('world_codec.cpp','id.module_versions.emplace_back("personal_injury_recovery"','id.module_versions.emplace_back("personal_appointment_attendance"'),('native_resident_backend.gd','"name":"personal_injury_recovery"','"name":"personal_appointment_attendance"')]:
            s=self.leaf(name);self.assertLess(s.index(old),s.index(new))
        self.assertNotIn('section("personal_appointment_attendance"',self.leaf('world_codec.cpp'))
    def test_original_obligations_before_attendance(self):
        """不抢旧Task责任，原真实命令及决定预算仍原入口。"""
        s=self.leaf('director_rules.cpp');self.assertLess(s.index('if(obligation){'),s.index('auto attendance=propose_personal_appointment_attendance'));self.assertLess(s.index('auto attendance=propose_personal_appointment_attendance'),s.index('// 撤准入后只履行'))
        s=self.leaf('dynamic_integration.cpp');self.assertIn('return wake_personal_appointment_attendance_world(w)',s);self.assertIn('appointment_attendance?&*appointment_attendance:nullptr)',s);self.assertIn('apply_director_proposal(w,proposed.value.front(),tick)',s)
    def test_catalog_schema_and_native_lists(self):
        """显式版本、能力、三真实CPP双列表与两头完整登记。"""
        schema=json.loads(self.leaf('content.schema.json'))
        for key in ['dynamic_director','dynamic_director_legacy','dynamic_director_registry']:self.assertEqual([1],schema['$defs'][key]['properties']['personal_appointment_attendance']['properties']['version']['enum'])
        cap=next(c for c in json.loads(self.leaf('rule_capabilities.json'))['capabilities'] if c['capability_id']=='personal_appointment_attendance_v1');self.assertEqual(['test_personal_appointment_attendance_catalog'],cap['test_ids'])
        s=self.leaf('resident.cmake')
        for stem in ['personal_appointment_attendance','personal_appointment_attendance_catalog','personal_appointment_attendance_world']:self.assertEqual(2,s.count('    src/resident/domains/interaction/appointments/'+stem+'.cpp'))
        self.assertIn('resident_personal_appointment_attendance',s)
if __name__=='__main__':unittest.main()
