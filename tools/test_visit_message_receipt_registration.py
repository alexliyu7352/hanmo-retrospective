"""检查当前真实合流源的身份、实际结果和晚验闭包；不代World行为门。"""
import ast,json,subprocess,unittest
from pathlib import Path
from tools.visit_message_receipt_registration_candidate import BASE,SHARED,transform
class ReceiptRegistrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo=Path(__file__).resolve().parents[1]
        cls.sources={p:(cls.repo/p).read_text() for p in SHARED}
    def source(self,name):
        rows=[s for p,s in self.sources.items() if p.endswith('/'+name)];self.assertEqual(1,len(rows));return rows[0]
    def test_independent_optional_old_visitor_unchanged(self):
        self.assertIn('std::optional<VisitMessageReceiptState> visit_message_receipts;',self.source('interaction_model.hpp'))
        self.assertFalse(any(p.endswith(('/visit_message_codec.cpp','/common_codec.cpp','/command_types.hpp')) for p in SHARED))
    def test_all_three_identity_producers_tail(self):
        for path,old,new in [('world_codec.cpp','if(result->visit_appointment_attendance)id.module_versions','if(result->visit_message_receipts_enabled)id.module_versions'),('native_initial_state.cpp','if(written_attendance.value)admitted.module_versions','if(message_receipts.value)admitted.module_versions'),('native_resident_backend.gd','if definition.has("visit_appointment_attendance"):', 'if str(definition.get("visit_message_receipt_profile"')]:
            s=self.source(path);self.assertLess(s.index(old),s.index(new))
    def test_binary_sections_and_title_use_same_selection(self):
        s=self.source('world_codec.cpp')
        self.assertIn('visit_message_receipt_selected_version(identity.catalog.module_versions)',s)
        self.assertIn('message_receipts.value!=version(visit_message_receipt_module)',s)
        self.assertIn('section(visit_message_receipt_module,',s)
        self.assertIn('else if(section.domain==visit_message_receipt_module)',s)
        self.assertLess(s.index('domains.push_back({political_head_handover_domain,1});return') if 'domains.push_back({political_head_handover_domain,1});return' in s else s.index('if(catalog.political_head_handovers)domains.push_back'),s.index('if(catalog.visit_message_receipts_enabled)domains.push_back'))
    def test_json_and_new_world_exact_enabled_pair(self):
        s=self.source('world_codec.cpp');self.assertIn('UnexpectedVisitMessageReceiptDomain',s);self.assertIn('decode_visit_message_receipts(field(source,visit_message_receipt_module',s);self.assertIn('set(output,visit_message_receipt_module,encode_visit_message_receipts',s)
        self.assertIn('if(mutable_catalog->visit_message_receipts_enabled)interaction->visit_message_receipts.emplace()',self.source('native_initial_state.cpp'))
    def test_actual_outer_result_before_plan_and_full_validation(self):
        s=self.source('resident_world.cpp');self.assertIn('command.actor_id,committed_kind,applied.value);if(!visit_message_sources.ok)',s)
        self.assertIn('visit_message_sources.value',s.split('auto completed=commit_tail(',1)[1].split(';',1)[0])
        self.assertLess(s.index('state.history.push_back'),s.index('auto messages_bound=bind_visit_message_result_world'))
        self.assertLess(s.index('auto messages_bound=bind_visit_message_result_world'),s.index('finish_player_plan_commit'))
    def test_real_advancing_wrappers_and_before_clock(self):
        dispatcher=(self.repo/'native/world/src/resident/kernel/dispatcher.cpp').read_text()
        personal=(self.repo/'native/world/src/resident/kernel/personal_access.cpp').read_text()
        confirm=(self.repo/'native/world/src/resident/kernel/interaction_integration.cpp').read_text()
        self.assertIn('std::holds_alternative<WaitCommand>(command.data)',dispatcher);self.assertIn('std::holds_alternative<TravelWaitCommand>(*travel)',dispatcher)
        self.assertIn('(command.kind=="duel_start"||command.kind=="encounter_fight")&&quote.duration>0',personal);self.assertLess(personal.index('auto elapsed=advance_world'),personal.index('return apply_personal('))
        self.assertIn('auto applied=apply_command(world,actor,*operation.nested)',confirm);self.assertIn('details.data=Done{}',confirm)
        bridge=(self.repo/'native/world/src/resident/domains/interaction/visit_messages/visit_message_receipt_world.cpp').read_text();self.assertIn('after.common.tick(),before.common.tick()',bridge);self.assertIn('batch.from_tick',bridge)
    def test_full_family_source_before_receipt(self):
        s=self.source('world_bundle.cpp');self.assertLess(s.index('validate_family_biological_world(*this)'),s.index('validate_visit_message_receipt_world(*this)'))
        self.assertLess(s.index('validate_visit_message_world(*this)'),s.index('validate_visit_message_receipt_world(*this)'))
    def test_npc_cannot_borrow_current_player_entry(self):
        for f in ['phase_plan.cpp','dynamic_integration.cpp']:
            s=self.source(f);self.assertIn('VisitMessageNpcActorRequired',s);self.assertIn('w.catalog->visit_message_receipts_enabled',s);self.assertIn('w.common.state().metadata.player_actor_id',s)
        self.assertIn('std::holds_alternative<VisitMessageCommand>(command.data)',self.source('phase_plan.cpp'))
    def test_owned_preview_and_commit_same_control_guard(self):
        s=(self.repo/'native/world/src/resident/domains/interaction/visit_messages/visit_message_world.cpp').read_text();self.assertEqual(2,s.count('guard_visit_message_receipt_producer_world(w,actor,command)'))
        s=(self.repo/'native/world/src/resident/domains/interaction/visit_messages/visit_message_receipt_world.cpp').read_text();self.assertIn('VisitMessageControlChangeAwaitNextTick',s);self.assertNotIn('private_shop_command',s)
    def test_immutable_outer_authority_mode(self):
        root=self.repo/'native/world/src/resident/domains/interaction/visit_messages'
        world=(root/'visit_message_receipt_world.cpp').read_text();rules=(root/'visit_message_receipts.cpp').read_text();header=(root/'visit_message_receipts.hpp').read_text()
        self.assertIn('w.catalog->director->dynamic_subjects?VisitMessageSubmitterMode::controlled_player:VisitMessageSubmitterMode::legacy_actor',world)
        self.assertIn('if(!w.catalog->director->dynamic_subjects)return common_success();auto possible=',world)
        self.assertIn('submitter_mode=VisitMessageSubmitterMode::unavailable',header)
        self.assertIn('if(read.submitter_mode==VisitMessageSubmitterMode::controlled_player){if(!read.advance_submitter_before_fact)',rules)
        self.assertIn('s.contact->actor_id==player.value',rules)
        actual=self.source('resident_world.cpp');old=subprocess.check_output(['git','show',BASE+':native/world/src/resident/kernel/resident_world.cpp'],cwd=self.repo).decode()
        self.assertEqual([line for line in actual.splitlines() if 'PlayerCommandAuthorityRequired' in line],[line for line in old.splitlines() if 'PlayerCommandAuthorityRequired' in line])
    def test_native_all_targets_registered(self):
        s=self.source('resident.cmake')
        for n in ['visit_message_receipt_codec','visit_message_receipt_control','visit_message_receipt_profile','visit_message_receipt_world','visit_message_receipts']:
            self.assertEqual(2,s.count('    src/resident/domains/interaction/visit_messages/'+n+'.cpp'))
        for text in ['add_executable(test_resident_visit_message_receipts ','target_link_libraries(test_resident_visit_message_receipts ','target_include_directories(test_resident_visit_message_receipts ','target_compile_options(test_resident_visit_message_receipts ','add_test(NAME resident_visit_message_receipts ']:self.assertEqual(1,s.count(text))
    def test_strict_author_and_runtime_catalog_test(self):
        s=json.loads(self.source('content.schema.json'));self.assertEqual(['permanent_visit_message_results_v1'],s['$defs']['scenario']['properties']['visit_message_receipt_profile']['enum'])
        cap=next(c for c in json.loads(self.source('rule_capabilities.json'))['capabilities'] if c['capability_id']=='visit_message_receipts_v1');self.assertEqual(['test_visit_message_receipts_catalog'],cap['test_ids'])
        self.assertTrue((self.repo/'game/tests/test_visit_message_receipts_catalog.gd').is_file());ast.parse(self.source('verify.py'));ast.parse(self.source('links.py'))
if __name__=='__main__':unittest.main()
