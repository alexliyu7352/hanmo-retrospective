"""已知去向有限页的只读与旧回调源码门，不代替实际Godot画面或完整World。"""
from pathlib import Path
import unittest

class KnownContactPanelTest(unittest.TestCase):
    """来源与本人身份必须同时稳定；页面没有业务提交路径。"""
    def test_no_location_or_command_shortcut(self):
        """历史地点不能经UI变为人物真实坐标或远程办理指令。"""
        source=(Path(__file__).resolve().parents[1]/'game/ui/known_contact_location_panel.gd').read_text()
        for forbidden in ['_resident_submit_frozen','kernel.dispatch','kernel.preview','actor.location','resolved_location']:
            self.assertNotIn(forbidden,source)
        self.assertIn('row.get("current_location_confirmed",true)',source)
        self.assertIn('不能确认对方如今仍在此处',source)
    def test_exact_current_page_callback(self):
        """翻页重读原有限页并核实际next_cursor，不接受旧视图的任意跳转。"""
        source=(Path(__file__).resolve().parents[1]/'game/ui/known_contact_location_panel.gd').read_text().split('static func _page(host_ref:',1)[1]
        for guard in ['is_queued_for_deletion','is_inside_tree','is_visible_in_tree','button.disabled','host._advancing','host._storage_intent','host._scene_counter!=epoch','host.kernel.source_identity()!=source','host.kernel.command_context().get("actor_id")!=actor','page.get("source_identity")!=source','page.get("observer_actor_id")!=actor','page.get("next_cursor",-1)']:
            self.assertIn(guard,source)

if __name__=='__main__':unittest.main()
