"""补任实际公共接头源门，默认核当前repo；显式候选覆盖仅供隔离三方审阅。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];C=Path(os.environ.get('HANMO_SUBORDINATE_CANDIDATE',ROOT));K='native/world/src/resident/kernel/';D='native/world/src/resident/domains/politics/subordinate_appointments/'
def source(p):return (C/p if (C/p).is_file() else ROOT/p).read_text()
class Registration(unittest.TestCase):
    def test_three_marker_order(self):
        for p,a,b in [(K+'world_codec.cpp','if(result->political_office_resignations){id.module_versions','if(result->political_subordinate_appointments)id.module_versions'),(K+'native_initial_state.cpp','admitted.module_versions.emplace_back(political_office_resignation_module','admitted.module_versions.emplace_back(political_subordinate_appointment_module'),('game/ui/native_resident_backend.gd','definition.get("dynamic_world",{}).has("political_office_resignations")','definition.get("dynamic_world",{}).has("political_subordinate_appointments")')]:
            s=source(p);self.assertLess(s.index(a),s.index(b));self.assertIn('political_subordinate_appointment_intents',s)
    def test_original_enums(self):
        s=source('native/world/src/resident/domains/director/director_model.hpp');self.assertIn('office_resignation_response=18',s);self.assertNotIn('subordinate_appointment=19',s)
    def test_codec_context(self):
        s=source(K+'world_codec.cpp');self.assertEqual(s.count('writer.political_subordinate_appointment_codec_version=subordinate_version.value;'),1);self.assertEqual(s.count('reader.political_subordinate_appointment_codec_version=subordinate_version.value;'),2);self.assertIn('else if(section.domain==political_subordinate_appointment_module)',s)
    def test_exact_receipt(self):
        s=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('c.political_subordinate_appointment_codec_version!=1',s);self.assertNotIn('c.political_subordinate_appointment_codec_version!=2',s);self.assertIn('political_subordinate_appointment_effect_uses_sources(value)&&!is_political_subordinate_appointment_command(kind)',s)
    def test_public_raw_and_authored(self):
        s=source(K+'dispatcher.cpp');self.assertIn('prepare_authored_subordinate_appointment_world',s);self.assertIn('commit_authored_subordinate_appointment_world',s);s=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('decode_political_subordinate_appointment_command',s);self.assertNotIn('AuthoredSubordinateAppointmentCommand',s)
    def test_two_npc_savepoints(self):
        for p in [K+'phase_plan.cpp',K+'dynamic_integration.cpp']:
            s=source(p);self.assertIn('PoliticalSubordinateAppointmentPlayerCommandRequired',s);self.assertIn('AuthoredSubordinateAppointmentCommand',s);self.assertIn('w.political_subordinate_appointments=std::move(',s)
        self.assertIn('(politics||authored_resignation||authored_subordinate)',source(K+'phase_plan.cpp'))
    def test_actual_result_binding(self):
        s=source(K+'resident_world.cpp');self.assertIn('political_subordinate_appointment_new_player_source(*current.political_subordinate_appointments,*candidate->political_subordinate_appointments',s);self.assertIn('subordinate_appointment_source_id,actor,sequence,*receipt',s)
    def test_late_source(self):
        s=source(K+'world_bundle.cpp');new=s.index('valid=validate_political_subordinate_appointment_world(*this)');self.assertLess(s.index('validate_family_voluntary_paths_world(*this)'),new);self.assertLess(s.index('validate_political_succession_world(*this)'),new);self.assertGreater(s.index('validate_political_subordinate_appointment_director_world(*this)'),new)
    def test_precise_original_receipt_split(self):
        s=source(K+'world_bundle.cpp');self.assertIn('politics_receipt_for_original(r,',s);self.assertIn('bool(catalog->political_subordinate_appointments)&&bool(political_subordinate_appointments)',s)
        route=source(K+'politics_receipt_routing.cpp');self.assertIn('(selected.subordinate_appointment&&is_political_subordinate_appointment_command(r.kind))',route);self.assertIn('return effect;',route)
        self.assertIn('validate_political_subordinate_appointment_player_results',source(K+'political_subordinate_appointment_world.cpp'))
    def test_original_pending_guard(self):
        s=source(K+'dispatcher.cpp');self.assertEqual(s.count('preview_political_subordinate_legacy_world(w,*politics)'),2)
    def test_payloads_and_no_fake_grants(self):
        s=source(K+'world_codec.cpp');self.assertIn('validate_political_subordinate_appointment_fact_header(legal_common',s);self.assertIn('validate_political_subordinate_appointment_fact_header(w.common',s);self.assertIn('political_subordinate_appointments->arrivals.size()',s)
    def test_no_catalog_abi_claim(self):
        s=source(K+'political_subordinate_appointment_world.cpp');self.assertIn('qualification_(w.common,*w.political_subordinate_appointments',s);self.assertIn('intent_marker!=bool(w.catalog->political_subordinate_appointment_intents)',s);self.assertIn('std::make_shared',source(K+'political_subordinate_appointment_director_world.cpp'))
if __name__=='__main__':unittest.main()
