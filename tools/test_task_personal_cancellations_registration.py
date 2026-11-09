"""默认核实际合流源码，独立候选只能显式选择；源码门不代Cold/World运行。"""
import os,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SHARED=Path(os.environ.get('HANMO_PERSONAL_CANCELLATION_CANDIDATE',ROOT))
def read(path):return (SHARED/path).read_text()
class CancellationRegistration(unittest.TestCase):
 def test_full_cmake_owned_registration(self):
  cmake=read('native/world/resident.cmake');domain=ROOT/'native/world/src/resident/domains/tasks/personal_cancellations'
  for p in domain.glob('*.cpp'):
   with self.subTest(path=p.name):self.assertEqual(cmake.count('    src/resident/domains/tasks/personal_cancellations/'+p.name+'\n'),2)
  for p in domain.glob('*.hpp'):
   with self.subTest(path=p.name):self.assertIn('src/resident/domains/tasks/personal_cancellations/'+p.name,cmake)
  for name in ['catalog','codec','receipts','profile','transaction']:self.assertIn('add_test(NAME resident_task_personal_cancellation_'+name,cmake)
 def test_actual_result_and_player_only(self):
  s=read('native/world/src/resident/kernel/resident_world.cpp')
  self.assertLess(s.index('task_personal_cancellation_new_result_world(current,*candidate'),s.index('personal_cancellation_source.value,subordinate_appointment_source_id);if(!completed.ok)'))
  self.assertIn('bind_task_personal_cancellation_result_world(w,personal_cancellation_source,actor,sequence,*receipt)',s)
  for p in ['phase_plan.cpp','dynamic_integration.cpp']:self.assertIn('task_personal_cancellation_npc_command_allowed',read('native/world/src/resident/kernel/'+p))
  s=(ROOT/'native/world/src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_world.cpp').read_text();self.assertIn('services_.sources.player_before=',s);self.assertIn('delivery_->services().sources',s);self.assertNotIn('services_.sources.delivery.player_before=',s)
 def test_codec_three_producers_and_full_late_history(self):
  s=read('native/world/src/resident/kernel/world_codec.cpp')
  for token in ['task_personal_cancellation_selected_version(identity.catalog.module_versions)','task_personal_cancellation_selected_version(versions)','decode_task_personal_cancellation_profile','visit_task_personal_cancellations_checkpoint','validate_task_personal_cancellation_json_payloads','task_personal_cancellation_fact_payloads']:
   with self.subTest(token=token):self.assertIn(token,s)
  s=read('native/world/src/resident/kernel/world_bundle.cpp');self.assertLess(s.index('validate_task_personal_report_world(*this)'),s.index('validate_task_personal_cancellation_world(*this)'))
  self.assertIn('decode_task_personal_cancellation_profile',read('native/world/src/resident/kernel/native_initial_state.cpp'));self.assertIn('"name":"task_personal_message_cancellations","version":"1"',read('game/ui/native_resident_backend.gd'))
 def test_real_ui_navigation_and_original_task_writer(self):
  for file in ['resident_page_requests.gd','screen_composer.gd']:self.assertIn('TaskPersonalCancellationsPanel',read('game/ui/'+file))
  s=(ROOT/'native/world/src/resident/domains/tasks/personal_cancellations/task_personal_cancellation_preview.cpp').read_text();self.assertIn('cancel_task(c,tasks,d.task_catalog,d.common_catalog,task_id,nullptr,services.execution)',s);self.assertNotIn('apply_resources(',s);self.assertNotIn('WalletDelta{',s);self.assertNotIn('TransferLots{',s)
  self.assertIn('TaskPersonalCancel',read('native/world/src/resident/kernel/dispatcher.cpp'))
if __name__=='__main__':unittest.main()
