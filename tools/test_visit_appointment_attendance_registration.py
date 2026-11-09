"""实际当前共享接线结构门；真实source/Action语义另有原域门，不能代World运行。"""
import ast,copy,json,subprocess,unittest
from pathlib import Path
from tools.visit_appointment_attendance_registration_candidate import BASE,SHARED,transform
from tools.content.visit_appointment_attendance_links import permits_private_reception_attendance
class WrittenAttendanceRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[1];cls.sources={p:(cls.repo/p).read_text() for p in SHARED}
    def source(self,suffix):
        rows=[s for p,s in self.sources.items() if p.endswith(suffix)];self.assertEqual(1,len(rows));return rows[0]
    def test_no_new_persistent_state_or_variant(self):
        self.assertFalse(any(p.endswith(('/interaction_model.hpp','/command_types.hpp','/phase_plan.cpp','/resident_world.cpp')) for p in SHARED))
        s=self.source('/world_codec.cpp');self.assertNotIn('section("visit_appointment_attendance"',s);self.assertNotIn('domains.push_back({"visit_appointment_attendance"',s)
    def test_marker_only_actual_cold_entry(self):
        s=self.source('/world_codec.cpp')
        self.assertIn('validate_visit_appointment_attendance_layout(identity.catalog.module_versions,dynamic)',s)
        self.assertIn('validate_visit_appointment_attendance_layout(versions,dynamic,version("visit_appointment_attendance")!=0)',s)
        self.assertNotIn('has("visit_appointment_attendance","1")!=(version("visit_appointment_attendance")==1)',s)
    def test_three_identity_producers(self):
        for suffix in ['/world_codec.cpp','/native_initial_state.cpp','/native_resident_backend.gd']:self.assertIn('visit_appointment_attendance',self.source(suffix))
    def test_current_same_stack_read(self):
        s=self.source('/dynamic_integration.cpp');self.assertIn('written_attendance.value?&*written_attendance.value:nullptr',s);self.assertLess(s.index('visit_appointment_attendance_read_world(w)'),s.index('written_attendance.value?&*written_attendance.value:nullptr'));self.assertIn('wake_visit_appointment_attendance_world(w)',s)
    def test_original_template_order_and_null_default(self):
        s=self.source('/personal_appointment_attendance.hpp');self.assertIn('written_visits=nullptr',s);self.assertIn('written_catalog=nullptr',s)
        s=self.source('/personal_appointment_attendance.cpp');self.assertIn('promise->terms.earliest_tick<selected->earliest_tick',s);self.assertIn('if(read.written_visits||read.written_catalog)',s)
    def test_full_sources_before_derived_plan(self):
        s=self.source('/world_bundle.cpp');self.assertLess(s.index('validate_visit_appointment_world(*this)'),s.index('validate_visit_appointment_attendance_world(*this)'))
        s=self.source('/personal_appointment_attendance_world.cpp');self.assertIn('w.catalog->visit_appointment_attendance&&visit_appointment_attendance_goal(row.second.goal_ref)',s)
    def test_complete_native_registration(self):
        s=self.source('/resident.cmake')
        for name in ['visit_appointment_attendance.cpp','visit_appointment_attendance_catalog.cpp','visit_appointment_attendance_world.cpp']:self.assertEqual(2,s.count('    src/resident/domains/interaction/visit_appointments/'+name))
        self.assertEqual(1,s.count('tests/resident/visit_appointment_attendance_test.cpp'))
    def test_real_runtime_test_registration(self):
        self.assertIn('test_visit_appointment_attendance_catalog',self.source('/verify.py'))
        ast.parse(self.source('/verify.py'))
        self.assertTrue((self.repo/'game/tests/test_visit_appointment_attendance_catalog.gd').is_file())
    def test_explicit_private_reception_attendance_pair(self):
        s=self.source('/dynamic_world_links.py');ast.parse(s)
        self.assertIn('private_only = permits_private_reception_attendance(scenario,policy) or ',s)
        scene=json.loads((self.repo/'content/packs/authored/visit_appointments_1_1/content.json').read_text())['scenarios'][0]
        policy=scene['dynamic_world']['director']['subjects']['society::actor.study_swordswoman']
        self.assertNotIn('organization_id',policy)
        self.assertTrue(permits_private_reception_attendance(scene,policy))
        for key in ['visit_appointment_attendance','visit_appointment_profile','personal_appointment_profile','visit_reception_profile','visit_reception_intents']:
            broken=copy.deepcopy(scene);del broken[key]
            self.assertFalse(permits_private_reception_attendance(broken,policy),key)
        broken=copy.deepcopy(scene);del broken['dynamic_world']['director']['personal_appointment_attendance']
        self.assertFalse(permits_private_reception_attendance(broken,policy))
        for directory,field in [('visit_reception_intents','intents'),('visit_appointment_attendance','invitation_policy_ids')]:
            broken=copy.deepcopy(scene);broken[directory][field]=[]
            self.assertFalse(permits_private_reception_attendance(broken,policy))
        broken=copy.deepcopy(scene);broken['dynamic_world']['director']['personal_appointment_attendance']['policies']=[]
        self.assertFalse(permits_private_reception_attendance(broken,policy))
        broken=copy.deepcopy(scene);broken['dynamic_world']['organization']['named_membership_enabled']=False
        self.assertFalse(permits_private_reception_attendance(broken,policy))
        for missing in ['visit_reception','personal_appointment_attendance']:
            single=copy.deepcopy(policy);single['templates'].remove(missing)
            self.assertFalse(permits_private_reception_attendance(scene,single))
        foreign=copy.deepcopy(policy);foreign['templates'].append('fulfill_contract')
        self.assertFalse(permits_private_reception_attendance(scene,foreign))
    def test_strict_profile_and_cap(self):
        d=json.loads(self.source('/content.schema.json'))['$defs']['scenario']['properties']['visit_appointment_attendance'];self.assertEqual([1],d['properties']['version']['enum']);self.assertFalse(d['additionalProperties']);self.assertTrue(d['properties']['invitation_policy_ids']['uniqueItems'])
        caps=json.loads(self.source('/rule_capabilities.json'))['capabilities'];self.assertEqual(1,sum(c['capability_id']=='visit_appointment_attendance_v1' for c in caps))
if __name__=='__main__':unittest.main()
