"""探听的有限页源码门；真正Godot控件与World运行另外验证。"""
from pathlib import Path
import unittest

class ContactInquiryPanelTest(unittest.TestCase):
    """UI只发真实单一探听命令，不把已有记录或预览变成新知识。"""
    def test_actual_command_and_source(self):
        """提交必须重新核quote、本人、revision、request并禁重复按钮。"""
        source=(Path(__file__).resolve().parents[1]/'game/ui/contact_inquiries_panel.gd').read_text()
        for text in ['const QUERY="contact_inquiries"','["contact_inquiry"]','host.kernel.preview(action.command,action.params,actor)','context.next_request_seq','button.disabled=true','quote.get("revision")!=source.revision','source_identity()==source','command_context().get("actor_id")==actor']:
            self.assertIn(text,source)
        for forbidden in ['kernel.dispatch','actor.location','resolved_location','set_field']:
            self.assertNotIn(forbidden,source)
    def test_stale_controls_and_paging(self):
        """实际控件和本页游标稳定才可翻页/提交，保存和换人清草稿。"""
        source=(Path(__file__).resolve().parents[1]/'game/ui/contact_inquiries_panel.gd').read_text()
        for guard in ['is_queued_for_deletion','is_inside_tree','is_visible_in_tree','host._advancing','host._storage_intent','host._scene_counter!=epoch','page.get("source_identity")!=source','page.get("observer_actor_id")!=actor','page.get("next_cursor",-1)','host.get_meta(META,{}).get("selected")!=action']:
            self.assertIn(guard,source)
        self.assertIn('未透露',source)

if __name__=='__main__':unittest.main()
