"""固定共享源合同检查；不冒Native运行或三个真实冷入口已跑。"""
import json,subprocess,unittest
from pathlib import Path
from tools.visit_appointment_registration_candidate import BASE,SHARED,transform
class VisitAppointmentRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[1]
        cls.sources={p:transform(p,subprocess.check_output(['git','show',BASE+':'+p],cwd=cls.repo,text=True),cls.repo) for p in SHARED}
    def source(self,suffix):
        found=[s for p,s in self.sources.items() if p.endswith(suffix)];self.assertEqual(1,len(found));return found[0]
    def test_actual_result_tail(self):
        s=self.source('/resident_world.cpp');self.assertIn('visit_appointment_new_result_world(current,*candidate',s);self.assertIn('visit_appointment_meeting_new_result_world(current,*candidate',s);self.assertIn('visit_invitation_source.value,visit_meeting_sources.value);',s);self.assertIn('bind_visit_appointment_result_world(w,visit_invitation_source,visit_meeting_sources,actor,sequence,*receipt)',s)
    def test_exact_late_source_order(self):
        s=self.source('/world_bundle.cpp');self.assertLess(s.index('valid=validate_family_biological_world(*this)'),s.index('valid=validate_visit_appointment_world(*this)'));self.assertLess(s.index('valid=validate_personal_appointment_world(*this)'),s.index('valid=validate_visit_appointment_world(*this)'))
    def test_old_and_new_hot_commitment_guard(self):
        s=self.source('/personal_appointment_world.cpp');self.assertEqual(2,s.count('check_visit_appointment_personal_accept_world'))
    def test_npc_savepoints_and_player_block(self):
        s=self.source('/phase_plan.cpp');self.assertIn('std::holds_alternative<AuthoredVisitAppointment>(command.data)',s);self.assertIn('VisitAppointmentPlayerCommandRequired',s);self.assertIn('auto interaction=',s)
        s=self.source('/dynamic_integration.cpp');self.assertIn('VisitAppointmentPlayerCommandRequired',s);self.assertIn('interaction(w.interaction)',s)
    def test_original_template13_bound_read(self):
        s=self.source('/visit_reception_world.cpp');self.assertIn('invitations_?&*invitations_:nullptr',s);self.assertIn('&*w.interaction.visit_appointment_meetings',s)
        s=self.source('/visit_reception_director.cpp');self.assertIn('visit_appointment_reception_pending',s);self.assertIn('propose_received_visit_appointment',s)
    def test_clock_and_release_not_second_action(self):
        s=self.source('/phase_plan.cpp');at=s.index('}else if(action.kind==visit_appointment_meeting_action_kind)');branch=s[at:s.index('}else if(action.kind==personal_appointment_action_kind)',at)];self.assertLess(branch.index('mutation.remaining_ticks=0'),branch.index('step_visit_appointment_meeting_world'));self.assertIn('VisitAppointmentMeetingFulfilled',branch)
    def test_independent_tail_and_old_visitors(self):
        s=self.source('/world_codec.cpp');self.assertIn('visit_visit_appointments_checkpoint',s);self.assertIn('visit_visit_appointment_meetings_checkpoint',s);self.assertIn('visit_appointment_action_payloads',s);self.assertIn('validate_visit_appointment_json_payloads',s)
        self.assertFalse(any(p.endswith('/interaction_codec.cpp') or p.endswith('/task_codec.cpp') for p in SHARED))
        self.assertLess(s.index('r=section("family_biological_legacy"'),s.index('r=section("visit_appointment_invitations"'));self.assertLess(s.index('r=section("visit_appointment_invitations"'),s.index('r=section("visit_appointment_meetings"'))
    def test_three_module_marker_producers(self):
        for suffix in ['/world_codec.cpp','/native_initial_state.cpp','/native_resident_backend.gd']:
            s=self.source(suffix)
            for name in ['visit_appointment_invitations','visit_appointment_meetings','visit_appointment_intents']:self.assertIn(name,s)
    def test_native_sources_registered_twice(self):
        s=self.source('/resident.cmake')
        for p in (self.repo/'native/world/src/resident/domains/interaction/visit_appointments').glob('*.cpp'):self.assertEqual(2,s.count('    '+p.relative_to(self.repo/'native/world').as_posix()))
    def test_declared_capability_enum(self):
        d=json.loads(self.source('/content.schema.json'));p=d['$defs']['scenario']['properties'];self.assertEqual(['witnessed_visit_appointments_v1'],p['visit_appointment_profile']['enum']);caps=json.loads(self.source('/rule_capabilities.json'))['capabilities'];self.assertEqual(1,sum(c['capability_id']=='visit_appointments_v1' for c in caps));self.assertEqual(1,sum(c['capability_id']=='visit_appointment_intents_v1' for c in caps))
    def test_external_internal_command_split(self):
        s=self.source('/command_boundary.cpp');self.assertIn('VisitAppointmentNpcOnlyCommand',s);self.assertIn('decode_visit_appointment_meeting_command(kind,params)',s);self.assertIn('decode_visit_appointment_command(kind,params)',s)
    def test_two_live_page_locations(self):
        s=self.source('/screen_composer.gd');self.assertEqual(2,s.count('VisitAppointmentsPanel.visible_here(m)'));self.assertIn('VisitAppointmentsPanel.prepare(host)',self.source('/resident_page_requests.gd'))
if __name__=='__main__':unittest.main()
