"""默认核实际合流源；候选必须显式给根，源门不能代真实钱款/半日执行。"""
import json,os,unittest
from pathlib import Path
ROOT=Path(os.environ.get('HANMO_SOURCE_ROOT',Path(__file__).resolve().parents[1]))
def read(p):return (ROOT/p).read_text()
class Registration(unittest.TestCase):
 def test_original_start(self):
  s=read('native/world/src/resident/kernel/dispatcher.cpp');s=s[s.index('Outcome<ActionStarted> begin_action('):s.index('// 原MilitaryCoordination.journey')];self.assertLess(s.index('revalidate_task_personal_report_quote_world'),s.index('next_id("action")'));self.assertIn('action.kind="report_offer"',s);self.assertLess(s.index('apply_actor(m)'),s.index('start_task_personal_report_world'))
 def test_sole_money_writer(self):
  s=read('native/world/src/resident/domains/tasks/task_rules.cpp');self.assertIn('prepare_task_personal_report_completion',s);self.assertIn('(personal_action||personal_report_action)?StableId{}:action.kind',s);self.assertLess(s.index('validate_task_personal_report_services'),s.index('auto result=finish_task_action_original_with_returns'));self.assertIn('record_task_personal_report_completion_in_candidate',s)
  own=read('native/world/src/resident/domains/tasks/personal_reports/task_personal_report_finish.cpp');self.assertNotIn('TransferCash',own);self.assertNotIn('apply_resources',own)
 def test_same_stack_phase_and_release(self):
  s=read('native/world/src/resident/kernel/phase_plan.cpp');self.assertIn('TaskPersonalReportWorldAccess::create(w,&dynamic_input)',s);self.assertIn('reports.value?&reports.value->services():nullptr',s);self.assertIn('r=release_action(w,id)',s)
 def test_exact_outer_command(self):
  s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('decode_task_personal_report(kind,params)',s);self.assertIn('std::is_same_v<T,TaskPersonalReportQuote>',s);self.assertIn('InvalidTaskPersonalReportReceipt',s)
  for file,entry in [('phase_plan.cpp','Status try_npc_command('),('dynamic_integration.cpp','Status adopt_director_proposal(')]:
   s=read('native/world/src/resident/kernel/'+file);s=s[s.index(entry):];self.assertLess(s.index('task_personal_report_npc_command_allowed'),s.index('prepare_command'))
 def test_actual_result(self):
  s=read('native/world/src/resident/kernel/resident_world.cpp');self.assertLess(s.index('task_personal_report_new_result_world'),s.index('auto completed=commit_tail('));s=s[s.index('Status commit_tail('):];self.assertLess(s.index('state.results.set'),s.index('bind_task_personal_report_result_world'));self.assertLess(s.index('state.history.push_back'),s.index('bind_task_personal_report_result_world'));self.assertLess(s.index('bind_task_personal_report_result_world'),s.index('finish_player_plan_commit'))
 def test_late_source_order(self):
  s=read('native/world/src/resident/kernel/world_bundle.cpp');report=s.index('validate_task_personal_report_world(*this)');self.assertLess(s.index('validate_known_personal_dialogue_world(*this)'),report);self.assertLess(s.index('validate_family_voluntary_paths_world(*this)'),report);self.assertLess(s.index('validate_task_personal_message_world(*this)'),report);self.assertLess(report,s.index('validate_private_shop_world',report));self.assertIn('validate_player_plan(common,player_plan)',s)
 def test_all_marker_and_section_orders(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertLess(s.index('id.module_versions.emplace_back(task_personal_message_module'),s.index('id.module_versions.emplace_back(task_personal_report_module'));self.assertLess(s.index('id.module_versions.emplace_back(task_personal_report_module'),s.index('task_personal_message_selected_version(id.module_versions)'));self.assertIn('has("task_message_handoffs","4")!=(version("task_message_handoffs")==4)',s);self.assertIn('task_personal_report_selected_version(versions)',s);self.assertIn('task_personal_report_selected_version(identity.catalog.module_versions)',s)
  s=read('native/world/src/resident/kernel/native_initial_state.cpp');self.assertLess(s.index('admitted.module_versions.emplace_back(task_personal_message_module'),s.index('admitted.module_versions.emplace_back(task_personal_report_module'));self.assertLess(s.index('admitted.module_versions.emplace_back(task_personal_report_module'),s.index('task_personal_message_selected_version(admitted.module_versions)'))
  s=read('game/ui/native_resident_backend.gd');self.assertLess(s.index('"name":"task_personal_message_deliveries"'),s.index('"name":"task_personal_message_reports"'))
 def test_independent_payloads(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertEqual(s.count('visit_task_personal_reports_checkpoint'),2);self.assertIn('decode_task_personal_reports',s);self.assertIn('validate_task_personal_report_json_payloads',s);self.assertIn('merge_unique(facts,personal_reports.value)',s);self.assertIn('std::optional<TaskPersonalReportState> personal_reports;',read('native/world/src/resident/domains/tasks/task_model.hpp'))
 def test_real_cmake(self):
  s=read('native/world/resident.cmake');self.assertEqual(s.count('src/resident/domains/tasks/personal_reports/task_personal_report_world.cpp'),2)
  for name in ['catalog','codec','receipts','profile']:self.assertIn('add_executable(test_resident_task_personal_report_'+name,s)
 def test_adjacent_original_observation_profile(self):
  s=read('native/world/src/resident/domains/interaction/contact_inquiries/contact_inquiry_boundary.cpp');self.assertIn('string(*message,"task_message_handoff_profile")=="known_personal_delivery_and_sender_report_v4"',s);self.assertIn('m.second=="3"||m.second=="4"',s)
 def test_finite_query(self):
  self.assertIn('query=="task_personal_reports"',read('native/world/src/resident/kernel/live_view_queries.cpp'));s=json.loads(read('schemas/content.schema.json'));self.assertIn('task_personal_message_reports',s['$defs']['dynamic_world']['properties']);self.assertEqual(read('game/ui/screen_composer.gd').count('TaskPersonalReportsPanel.visible_here'),2)
if __name__=='__main__':unittest.main()
