"""默认审真实合流源；开发候选可显式指定根，不靠重新生成旧候选证明新产物。"""
import json,os,unittest
from pathlib import Path
ROOT=Path(os.environ.get('HANMO_SOURCE_ROOT',Path(__file__).resolve().parents[1]))
def read(path):return (ROOT/path).read_text()
class Registration(unittest.TestCase):
 def test_original_recipe(self):
  s=read('native/world/src/resident/domains/tasks/task_rules.cpp');self.assertIn('return_start->reservation.operations',s);self.assertIn('recipe.operations=return_delivery->transfer.operations',s);self.assertLess(s.index('record_task_message_report_after_settlement'),s.index('record_task_message_return_delivery_after_report'))
 def test_phase_step_and_release(self):
  s=read('native/world/src/resident/kernel/phase_plan.cpp');a=s.index('}else if(action.kind==task_message_return_collect_command)');b=s.index('}else if(action.kind=="accept_offer"',a);part=s[a:b];self.assertIn('mutation.remaining_ticks=0',part);self.assertLess(part.index('apply_action'),part.index('finish_task_message_return_world'));self.assertIn('completed=true',part);self.assertIn('r=release_action(w,id)',s[b:])
 def test_actual_commit_tail(self):
  s=read('native/world/src/resident/kernel/resident_world.cpp');self.assertLess(s.index('task_message_return_new_result_world'),s.index('auto completed=commit_tail('));tail=s[s.index('Status commit_tail('):];self.assertLess(tail.index('state.results.set'),tail.index('bind_task_message_return_result_world'));self.assertLess(tail.index('state.history.push_back'),tail.index('bind_task_message_return_result_world'))
 def test_npc_write_rejected(self):
  for p,entry in [('phase_plan.cpp','Status try_npc_command('),('dynamic_integration.cpp','Status adopt_director_proposal(')]:
   s=read('native/world/src/resident/kernel/'+p);s=s[s.index(entry):];self.assertIn('task_message_return_npc_command_allowed',s);self.assertLess(s.index('task_message_return_npc_command_allowed'),s.index('prepare_command'))
 def test_complete_history_before_consumers(self):
  s=read('native/world/src/resident/kernel/world_bundle.cpp');self.assertLess(s.index('validate_family_collateral_world(*this)'),s.index('validate_task_message_return_world(*this)'));late=s[s.index('validate_task_message_return_world(*this)'):];self.assertLess(late.index('validate_task_message_return_world'),late.index('validate_private_shop_world'));self.assertIn('!catalog->task_shop_order_contacts_enabled&&!catalog->task_message_returns',s);self.assertIn('if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns)',late)
 def test_all_three_markers_same_tail(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertLess(s.index('if(result->family_collateral_legacy)id.module_versions'),s.index('if(result->task_message_returns)id.module_versions'));self.assertIn('task_message_return_selected_version(identity.catalog.module_versions)',s);self.assertIn('task_message_return_selected_version(versions)',s)
  s=read('native/world/src/resident/kernel/native_initial_state.cpp');self.assertLess(s.index('admitted.module_versions.emplace_back("family_collateral_legacy"'),s.index('admitted.module_versions.emplace_back(task_message_return_module'))
  s=read('game/ui/native_resident_backend.gd');self.assertLess(s.index('"name":"family_collateral_legacy"'),s.index('"name":"task_message_returns"'))
 def test_binary_json_and_actual_action_payload(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertIn('decode_task_message_returns',s);self.assertIn('task_message_return_action_payloads',s);self.assertIn('validate_task_message_return_json_payloads',s);self.assertEqual(s.count('visit_task_message_returns_checkpoint'),2)
 def test_real_cmake_registration(self):
  s=read('native/world/resident.cmake');self.assertEqual(s.count('src/resident/domains/tasks/message_returns/task_message_return_world.cpp'),2);self.assertIn('src/resident/domains/tasks/message_returns/task_message_return_services.hpp',s)
  for name in ['catalog','codec','receipts','profile']:self.assertIn('add_executable(test_resident_task_message_return_'+name,s)
 def test_query_and_schema(self):
  self.assertIn('query=="task_message_returns"',read('native/world/src/resident/kernel/live_view_queries.cpp'));d=json.loads(read('schemas/content.schema.json'));self.assertIn('task_message_returns',d['$defs']['scenario']['properties'])
 def test_original_plan_continuation_only(self):
  """仅原接取Action续办；真实SAY与回执绑定不能被新的开始命令或泛旅行替代。"""
  prefix='native/world/src/resident/domains/player_plan/'
  for p in ['plan_sources.cpp','plan_binding.cpp']:
   self.assertIn('action.kind==task_message_return_collect_command',read(prefix+p))
   self.assertIn('player_plan_message_return_continuation',read(prefix+p))
  s=read(prefix+'plan_page.cpp');self.assertIn('action->kind==task_message_return_collect_command',s);self.assertIn('task_message_return_continuation_meeting_matches',s)
  s=read(prefix+'plan_execution.cpp');self.assertIn('task_message_return_continuation_meeting_matches',s);self.assertIn('!return_action&&',s);self.assertIn('player_plan_message_return_completion(w,actor,*step->action_id,receipt,w.common.tick(),result_id)',s)
  s=read(prefix+'plan_binding.cpp');self.assertIn('source.value->started_tick>step.source.observed_tick',s);self.assertIn('source.value->status=="collected"?StableId{}:source.value->reason',s)
  s=read('native/world/src/resident/kernel/world_bundle.cpp');a=s.index('validate_task_message_return_world(*this)');b=s.index('if(catalog->task_shop_order_contacts_enabled||catalog->task_message_returns){auto inheritance=');self.assertLess(a,b);self.assertIn('validate_player_plan_world(*this)',s[b:])
 def test_all_new_marker_relative_order(self):
  """合流的代理和平局marker仍在原回书marker之前；三个真实生产者次序一致。"""
  a=read('native/world/src/resident/kernel/world_codec.cpp');self.assertLess(a.index('id.module_versions.emplace_back(personal_simultaneous_outcomes_module'),a.index('if(result->task_message_returns)id.module_versions'))
  a=read('native/world/src/resident/kernel/native_initial_state.cpp');self.assertLess(a.index('admitted.module_versions.emplace_back(personal_simultaneous_outcomes_module'),a.index('admitted.module_versions.emplace_back(task_message_return_module'))
  a=read('game/ui/native_resident_backend.gd');self.assertLess(a.index('"name":"personal_simultaneous_outcomes"'),a.index('"name":"task_message_returns"'))
if __name__=='__main__':unittest.main()
