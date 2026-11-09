"""只核原Task钱货接缝和旧分支保留；不把源门当实际结算行为。"""
import os, unittest
from pathlib import Path
ROOT=Path(os.environ.get("HANMO_SOURCE_ROOT",Path(__file__).resolve().parents[1]))
MODEL="native/world/src/resident/domains/tasks/task_model.hpp"
RULES="native/world/src/resident/domains/tasks/task_rules.cpp"
class Hooks(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.rules=(ROOT/RULES).read_text()
  cls.model=(ROOT/MODEL).read_text()
 def test_one_original_writer(self):
  self.assertEqual(self.rules.count('Status start_task_action('),1)
  self.assertEqual(self.rules.count('Outcome<TaskEffect> finish_task_action('),1)
  self.assertEqual(self.rules.count('static Outcome<TaskEffect> finish_task_action_original_with_returns('),1)
  self.assertEqual(self.model.count('const TaskMessageReturnServices* returns=nullptr'),3)
 def test_reservation_before_original_recipe(self):
  s=self.rules[self.rules.index('Status start_task_action('):self.rules.index('static Outcome<TaskEffect>')]
  self.assertLess(s.index('prepare_task_message_return_report_start'),s.index('c.apply_resources(recipe)'))
  self.assertLess(s.index('c.apply_resources(recipe)'),s.index('s.actions.insert'))
  self.assertLess(s.index('s.actions.insert'),s.index('record_task_message_return_report_start_after_resources'))
 def test_transfer_same_recipe_and_original_settlement(self):
  s=self.rules[self.rules.index('static Outcome<TaskEffect>'):self.rules.index('Outcome<TaskEffect> finish_task_action(')]
  self.assertLess(s.index('recipe.operations=return_delivery->transfer.operations'),s.index('WalletDelta'))
  self.assertEqual(s.count('c.apply_resources(recipe)'),1)
  self.assertLess(s.index('c.apply_resources(recipe)'),s.index('c.next_id("settlement")'))
  self.assertLess(s.index('record_task_message_report_after_settlement'),s.index('record_task_message_return_delivery_after_report'))
 def test_original_failed_effect_preserved(self):
  s=self.rules[self.rules.index('Outcome<TaskEffect> finish_task_action('):self.rules.index('Status cancel_task(')]
  self.assertIn('!result.ok||result.value.success||!returns',s)
  self.assertIn('finish_tick,result.value)',s)
  self.assertNotIn('result.value.task_id=',s)
 def test_legacy_cancel_writer_unchanged(self):
  body=self.rules[self.rules.index('Status cancel_task('):self.rules.index('Status observe_escort_tasks(')]
  self.assertNotIn('task_message_return_',body)
  self.assertIn('record_task_static_asset_transfer_after_resources',body)
  self.assertEqual(body.count('c.next_id("settlement")'),1)
if __name__=='__main__':unittest.main()
