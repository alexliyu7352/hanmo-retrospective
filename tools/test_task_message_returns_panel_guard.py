"""Q03本人页只读源门；不冒Godot运行或Native实际Task链验收。"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ReturnPanelGuard(unittest.TestCase):
 def test_frozen_callback_and_only_collect(self):
  s=(ROOT/'game/ui/task_message_returns_panel.gd').read_text()
  for required in ['const QUERY="task_message_returns"','host.kernel.source_identity()==source','host.kernel.command_context().get("actor_id")==actor','host._scene_counter!=epoch','not control.is_visible_in_tree()','not host._storage_intent.is_empty()','action.get("command") not in ["task_message_return_collect"]','context.next_request_seq','"expected_revision":source.revision','host.kernel.preview(action.command,action.params,actor)','quote.get("revision")!=source.revision','button.disabled=true','draft.selected={}']:
   with self.subTest(required=required): self.assertIn(required,s)
  self.assertLess(s.index('button.disabled=true'),s.index('return host._resident_submit_frozen('))
  self.assertNotIn('estate_',s);self.assertNotIn('policy_cursor',s)
 def test_disabled_profile_does_not_block_page(self):
  s=(ROOT/'game/ui/task_message_returns_panel.gd').read_text();self.assertLess(s.index('if not page.get("enabled",true): return true'),s.index('if page.get("source_identity")!=draft.source'))
 def test_private_knowledge_and_finite_rows(self):
  s=(ROOT/'native/world/src/resident/domains/tasks/message_returns/task_message_return_view.cpp').read_text()
  self.assertIn('if(!carrier)return {true,std::move(out),{}};',s)
  self.assertLess(s.index('if(!carrier)return'),s.index('const TaskMessageReturnReportStart* reporting'))
  self.assertIn('observer==original->issuer.id&&delivery',s)
  self.assertIn('rows.size()<16',s)
  self.assertIn('x.common.state().metadata.player_actor_id',s)
  self.assertIn('preview_task_message_return_collection(x,interaction,access,observer',s)
if __name__=='__main__':unittest.main()
