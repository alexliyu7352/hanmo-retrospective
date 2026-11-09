"""Q01本人页只读源门；不冒Godot运行或Native实际Task链验收。"""
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
class PersonalMessagePanelGuard(unittest.TestCase):
 def test_frozen_callback_and_only_collect(self):
  s=(ROOT/'game/ui/task_personal_messages_panel.gd').read_text()
  for required in ['const QUERY="task_personal_messages"','host.kernel.source_identity()==source','host.kernel.command_context().get("actor_id")==actor','host._scene_counter!=epoch','not control.is_visible_in_tree()','not host._storage_intent.is_empty()','action.get("command") not in ["task_personal_message_deliver"]','context.next_request_seq','"expected_revision":source.revision','host.kernel.preview(action.command,action.params,actor)','quote.get("revision")!=source.revision','button.disabled=true','fresh.get("next_request_seq")!=context.next_request_seq']:
   with self.subTest(required=required): self.assertIn(required,s)
  self.assertLess(s.index('button.disabled=true'),s.index('var quote: Dictionary=host.kernel.preview(action.command,action.params,actor)'))
  self.assertGreaterEqual(s.count('if not _live(host_ref,control_ref,source,actor,epoch): return'),3)
  self.assertNotIn('static func _choose(',s)
  self.assertNotIn('确认本人决定',s)
  self.assertNotIn('"核对："',s)
  self.assertIn('host._text(item,row.get("terms_text",""),14)',s)
  self.assertIn('host._text(item,summary,14)',s)
  self.assertNotIn('estate_',s);self.assertNotIn('policy_cursor',s)
 def test_godot_control_gate_uses_actual_panels_and_signals(self):
  s=(ROOT/'game/tests/test_personal_message_single_action_panels.gd').read_text()
  self.assertIn('preload("res://ui/task_personal_messages_panel.gd")',s)
  self.assertIn('PANELS[index].build(host,host)',s)
  self.assertIn('case.button.pressed.emit()',s)
  self.assertIn('during_preview',s)
  self.assertIn('HANMO_TASK_PANEL_MOCK_PASS',s)
  self.assertIn('version.minor)!=7',s)
 def test_disabled_profile_does_not_block_page(self):
  s=(ROOT/'game/ui/task_personal_messages_panel.gd').read_text();self.assertLess(s.index('if not page.get("enabled",true): return true'),s.index('if page.get("source_identity")!=draft.source'))
 def test_private_knowledge_and_finite_rows(self):
  s=(ROOT/'native/world/src/resident/domains/tasks/personal_messages/task_personal_message_view.cpp').read_text()
  self.assertIn('i->second.assignee==actor',s)
  self.assertIn('rows.size()<16',s)
  self.assertIn('x.common.state().metadata.player_actor_id',s)
  self.assertIn('preview_task_personal_message(x,interaction,access,actor',s)
  self.assertNotIn('resolved_location(',s)
  self.assertNotIn('state().inventories.rows()',s)
  self.assertIn('std::to_string(',s)
  self.assertIn('terms_text',s)
  self.assertIn('reward)',s)
if __name__=='__main__':unittest.main()
