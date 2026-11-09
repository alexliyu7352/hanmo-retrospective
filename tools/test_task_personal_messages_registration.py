"""默认读真实合流源；隔离候选须显式指定根，不能用重建旧稿冒实际登记。"""
import json,os,unittest
from pathlib import Path
ROOT=Path(os.environ.get('HANMO_SOURCE_ROOT',Path(__file__).resolve().parents[1]))
def read(path):return (ROOT/path).read_text()
class Registration(unittest.TestCase):
 def test_original_task_start_and_action(self):
  s=read('native/world/src/resident/kernel/dispatcher.cpp');part=s[s.index('Outcome<ActionStarted> begin_action('):s.index('// 原MilitaryCoordination.journey')];self.assertLess(part.index('revalidate_task_personal_message_quote_world'),part.index('next_id("action")'));self.assertIn('action.kind="deliver_message"',part);self.assertLess(part.index('apply_actor(m)'),part.index('start_task_personal_message_world'))
 def test_original_finish_and_release(self):
  s=read('native/world/src/resident/domains/tasks/task_rules.cpp');self.assertIn('prepare_task_personal_message_handoff',s);self.assertTrue('personal_action?StableId{}:action.kind' in s or '(personal_action||personal_report_action)?StableId{}:action.kind' in s);self.assertIn('record_task_personal_message_completion_in_candidate',s)
  s=read('native/world/src/resident/kernel/phase_plan.cpp');self.assertIn('TaskPersonalMessageWorldAccess::create(w,&dynamic_input)',s);self.assertIn('personal.value?&personal.value->services():nullptr',s);self.assertIn('r=release_action(w,id)',s)
 def test_public_command_narrow(self):
  s=read('native/world/src/resident/checkpoint/command_boundary.cpp');self.assertIn('decode_task_personal_message_deliver',s);self.assertIn('std::is_same_v<T,TaskPersonalMessageQuote>',s);self.assertIn('InvalidTaskPersonalMessageReceipt',s)
 def test_npc_rejected_before_write(self):
  for file,entry in [('phase_plan.cpp','Status try_npc_command('),('dynamic_integration.cpp','Status adopt_director_proposal(')]:
   s=read('native/world/src/resident/kernel/'+file);s=s[s.index(entry):];self.assertLess(s.index('task_personal_message_npc_command_allowed'),s.index('prepare_command'))
 def test_actual_result_before_final_validate(self):
  s=read('native/world/src/resident/kernel/resident_world.cpp');self.assertLess(s.index('task_personal_message_new_result_world'),s.index('auto completed=commit_tail('));tail=s[s.index('Status commit_tail('):];self.assertLess(tail.index('state.results.set'),tail.index('bind_task_personal_message_result_world'));self.assertLess(tail.index('state.history.push_back'),tail.index('bind_task_personal_message_result_world'))
 def test_complete_known_family_then_personal(self):
  s=read('native/world/src/resident/kernel/world_bundle.cpp');personal=s.index('validate_task_personal_message_world(*this)');self.assertLess(s.index('validate_known_personal_dialogue_world(*this)'),personal);self.assertLess(s.index('validate_family_voluntary_paths_world(*this)'),personal);late=s[personal:];self.assertLess(late.index('validate_task_personal_message_world'),late.index('validate_private_shop_world'));self.assertIn('!catalog->task_message_returns&&!catalog->task_personal_messages',s);self.assertIn('catalog->task_shop_order_contacts_enabled||catalog->task_message_returns||catalog->task_personal_messages',late)
 def test_three_marker_tails(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertLess(s.index('if(result->family_voluntary_paths)id.module_versions'),s.index('if(result->task_personal_messages)id.module_versions'));self.assertIn('task_personal_message_selected_version(identity.catalog.module_versions)',s);self.assertIn('task_personal_message_selected_version(versions)',s);self.assertIn('has("task_message_handoffs","3")!=(version("task_message_handoffs")==3)',s)
  s=read('native/world/src/resident/kernel/native_initial_state.cpp');self.assertLess(s.index('admitted.module_versions.emplace_back("family_voluntary_paths"'),s.index('admitted.module_versions.emplace_back(task_personal_message_module'))
  s=read('game/ui/native_resident_backend.gd');self.assertLess(s.index('"name":"family_voluntary_paths"'),s.index('"name":"task_personal_message_deliveries"'))
 def test_separate_payloads_and_task_field(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertEqual(s.count('visit_task_personal_messages_checkpoint'),2);self.assertIn('decode_task_personal_messages',s);self.assertIn('validate_task_personal_message_json_payloads',s);self.assertIn('merge_unique(facts,personal_messages.value)',s)
  s=read('native/world/src/resident/domains/tasks/task_model.hpp');self.assertIn('std::optional<TaskPersonalMessageState> personal_messages;',s)
 def test_real_cmake(self):
  s=read('native/world/resident.cmake');self.assertEqual(s.count('src/resident/domains/tasks/personal_messages/task_personal_message_world.cpp'),2);self.assertIn('src/resident/domains/tasks/personal_messages/task_personal_message_services.hpp',s)
  for name in ['catalog','codec','receipts','profile','validation','handoff']:self.assertIn('add_executable(test_resident_task_personal_message_'+name,s)
 def test_query_schema_panel(self):
  self.assertIn('query=="task_personal_messages"',read('native/world/src/resident/kernel/live_view_queries.cpp'));s=json.loads(read('schemas/content.schema.json'));self.assertIn('task_personal_message_deliveries',s['$defs']['dynamic_world']['properties']);self.assertIn('known_personal_delivery_and_report_v3',s['$defs']['scenario']['properties']['task_message_handoff_profile']['enum']);self.assertEqual(read('game/ui/screen_composer.gd').count('TaskPersonalMessagesPanel.visible_here'),2)
if __name__=='__main__':unittest.main()
