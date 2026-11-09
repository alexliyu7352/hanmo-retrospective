"""受领前置源与原Task唯一writer实际接点；默认核合流源，隔离候选必须显式提供。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SHARED=Path(os.environ.get('HANMO_PERSONAL_ACCEPTANCE_CANDIDATE',ROOT))
def read(path):return (SHARED/path).read_text()
class AcceptanceRegistration(unittest.TestCase):
 def test_actual_original_writer_gates(self):
  s=read('native/world/src/resident/domains/tasks/task_rules.cpp')
  for token in ['task_personal_acceptance_original_preview_gate(catalog.personal_acceptances.get(),command)','task_personal_acceptance_original_start_gate(c,s,catalog,catalog.personal_acceptances.get()','prepare_task_personal_acceptance_completion(*personal_acceptances','record_task_personal_acceptance_completion_in_candidate','validate_task_personal_acceptance_task_coverage','validate_task_personal_acceptance_action_shape']:
   self.assertIn(token,s)
  self.assertLess(s.index('prepare_task_personal_acceptance_completion'),s.index('if(action.kind=="accept_offer"){if(!static_payment)recipe.operations='))
 def test_actual_results_and_no_npc(self):
  s=read('native/world/src/resident/kernel/resident_world.cpp');self.assertIn('task_personal_acceptance_new_result_world(current,*candidate,command.actor_id,committed_kind,applied.value)',s);self.assertIn('subordinate_appointment_source_id,personal_acceptance_sources.value);if(!completed.ok)',s);self.assertIn('bind_task_personal_acceptance_result_world(w,personal_acceptance_sources,actor,sequence,*receipt)',s)
  for p in ['phase_plan.cpp','dynamic_integration.cpp']:self.assertIn('task_personal_acceptance_npc_command_allowed',read('native/world/src/resident/kernel/'+p))
 def test_single_stack_and_late_order(self):
  s=read('native/world/src/resident/kernel/phase_plan.cpp');self.assertLess(s.index('auto acceptances=TaskPersonalAcceptanceWorldAccess::create(w,&dynamic_input)'),s.index('auto end=finish_task_action('));self.assertIn('acceptances.value?&acceptances.value->services():nullptr',s)
  s=read('native/world/src/resident/kernel/world_bundle.cpp');self.assertLess(s.index('validate_task_personal_acceptance_world(*this)'),s.index('validate_task_personal_message_world(*this)'));self.assertLess(s.index('validate_task_personal_message_world(*this)'),s.index('validate_task_personal_report_world(*this)'));self.assertLess(s.index('validate_task_personal_report_world(*this)'),s.index('validate_task_personal_cancellation_world(*this)'))
 def test_actual_alias_and_full_facts(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp');self.assertIn('task_alias->personal_acceptances=result->task_personal_acceptances',s)
  for token in ['task_personal_acceptance_selected_version(identity.catalog.module_versions)','task_personal_acceptance_selected_version(versions)','task_personal_acceptance_fact_payloads','validate_task_personal_acceptance_json_payloads','visit_task_personal_acceptances_checkpoint']:self.assertIn(token,s)
  d=read('native/world/src/resident/storage/common_codec.cpp')
  for kind in ['started','completed','interrupted']:self.assertIn('kind=="task_personal_acceptance_'+kind+'"',d)
 def test_module_tail_order(self):
  c=read('native/world/src/resident/kernel/world_codec.cpp');n=read('native/world/src/resident/kernel/native_initial_state.cpp');g=read('game/ui/native_resident_backend.gd')
  self.assertLess(c.index('id.module_versions.emplace_back(political_subordinate_appointment_module'),c.index('id.module_versions.emplace_back(task_personal_acceptance_module'))
  self.assertLess(n.index('admitted.module_versions.emplace_back(political_subordinate_appointment_module'),n.index('admitted.module_versions.emplace_back(task_personal_acceptance_module'))
  self.assertLess(g.index('"name":"political_subordinate_appointment_intents"'),g.index('"name":"task_personal_message_acceptances"'))
  self.assertLess(c.index('r=section(political_subordinate_appointment_module'),c.index('r=section(task_personal_acceptance_module'))
 def test_all_owned_registered_and_ui_reachable(self):
  s=read('native/world/resident.cmake');base=ROOT/'native/world/src/resident/domains/tasks/personal_acceptances'
  for p in base.glob('*.cpp'):self.assertEqual(s.count('    src/resident/domains/tasks/personal_acceptances/'+p.name+'\n'),2)
  for p in base.glob('*.hpp'):self.assertIn('src/resident/domains/tasks/personal_acceptances/'+p.name,s)
  for name in ['catalog','codec','receipts','profile','services','completion','transaction']:self.assertIn('add_test(NAME resident_task_personal_acceptance_'+name,s)
  for p in ['resident_page_requests.gd','screen_composer.gd']:self.assertIn('TaskPersonalAcceptancesPanel',read('game/ui/'+p))
if __name__=='__main__':unittest.main()
