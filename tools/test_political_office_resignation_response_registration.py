"""18号与真实NPC写集的精确源码门，仍不替代World行为。"""
import unittest
from test_political_office_resignation_registration import source,K,D
class ResponsesRegistration(unittest.TestCase):
    def test_18_explicit_context(self):
        s=source('native/world/src/resident/domains/director/director_codec.cpp');self.assertIn('DirectorOfficeResignationResponseProfile resignation',s);self.assertIn('governance,agency,resignation);if(!parsed.ok)',s);self.assertIn('resignation.enabled!=(value.find("office_resignation_responses")!=nullptr)',s)
    def test_native_two_callers(self):
        for p in [K+'native_initial_state.cpp',K+'world_codec.cpp']:self.assertIn('DirectorOfficeResignationResponseProfile{resignation_responses_profile.value.has_value()}',source(p))
    def test_both_saved_original_office(self):
        s=source(K+'phase_plan.cpp');self.assertIn('auto organization=(politics||authored_resignation)',s);self.assertIn('office_resignations=authored_resignation?w.political_office_resignations',s);self.assertIn('w.political_office_resignations=std::move(*office_resignations)',s)
        s=source(K+'dynamic_integration.cpp');self.assertIn('office_resignations(w.political_office_resignations)',s);self.assertIn('w.political_office_resignations=std::move(office_resignations)',s);self.assertIn('organization(w.organization)',s)
    def test_no_external_author_origin(self):
        s=source('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertNotIn('decode_authored_office',s);self.assertIn('decode_political_office_resignation_command(kind,params)',s)
        s=source(D+'political_office_resignation_rules.cpp');self.assertIn('else if(actor!=x.common.state().metadata.player_actor_id)',s)
    def test_active_policy_and_obligations_only(self):
        s=source(K+'political_office_resignation_response_world.cpp');self.assertIn('r->obligations_only',s);self.assertIn('director_subject_active',s);self.assertIn('command.origin.director_policy_id==id.value',s)
    def test_old_bytes_conditional_tail(self):
        s=source(D+'political_office_resignation_codec.cpp');self.assertIn('if(v.version==2)a("authored_origins",v.authored_origins)',s)
    def test_complete_source_before_plan(self):
        s=source(K+'world_bundle.cpp');self.assertLess(s.index('validate_political_office_resignation_world(*this)'),s.index('validate_political_office_resignation_response_world(*this)'))
    def test_no_remote_approval(self):
        s=source(D+'political_office_resignation_rules.cpp');self.assertIn('for(const auto& who:{actor,r->actor_id})',s);self.assertIn('contact(x,who,head.seat_place_id)',s)
if __name__=='__main__':unittest.main()
