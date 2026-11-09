"""回信有限页的原请求/本人来源和旧按钮门；不冒Godot实际运行。"""
from pathlib import Path
import unittest
class VisitReplyPanelTest(unittest.TestCase):
    """只允许原两具名动作，远方未领消息不转成预约。"""
    def test_commands_and_binding(self):
        """每次确认必须重报价、同本人/代次/request，禁止重复旧按钮。"""
        s=(Path(__file__).resolve().parents[1]/'game/ui/visit_message_replies_panel.gd').read_text()
        for token in ['const QUERY="visit_message_replies"','["reply_visit_message","collect_visit_message_reply"]','host.kernel.preview(action.command,action.params,actor)','context.next_request_seq','quote.get("revision")!=source.revision','button.disabled=true','source_identity()==source','command_context().get("actor_id")==actor','host.get_meta(META,{}).get("selected")!=action']:self.assertIn(token,s)
        for token in ['kernel.dispatch','personal_appointment_accept','actor.location','set_field']:self.assertNotIn(token,s)
    def test_stale_controls_and_cursor(self):
        """旧树/保存中/已翻页/换人不能提交原回复，读页也不生成消息。"""
        s=(Path(__file__).resolve().parents[1]/'game/ui/visit_message_replies_panel.gd').read_text()
        for token in ['is_queued_for_deletion','is_inside_tree','is_visible_in_tree','host._advancing','host._storage_intent','host._scene_counter!=epoch','page.get("source_identity")!=source','page.get("observer_actor_id")!=actor','page.get("next_cursor",-1)']:self.assertIn(token,s)
if __name__=='__main__':unittest.main()
