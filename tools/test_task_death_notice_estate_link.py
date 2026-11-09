"""已收死亡文书跳转的窄源码守卫；不冒实际Godot控件或清算许可验收。"""
from pathlib import Path
import unittest
class TaskDeathNoticeEstateLinkTest(unittest.TestCase):
    """只本人有限原页选中binding，旧来源与旧控件不能提交跳转。"""
    def test_exact_received_source(self):
        """回调重新取同页、同人、同源的收信列表，不接受任意调用者给的死者ID。"""
        s=(Path(__file__).resolve().parents[1]/'game/ui/task_death_notice_panel.gd').read_text();body=s.split('static func _estate(host_ref:',1)[1]
        for guard in ['is_queued_for_deletion','is_inside_tree','is_visible_in_tree','host._advancing','host._storage_intent','host._scene_counter!=epoch','host.kernel.source_identity()!=source','host.kernel.command_context().get("actor_id")!=actor','private_shop_liquidations_enabled','host.kernel.demand_view(QUERY','row.get("binding_fact_id")==binding','row.get("recipient_actor_id")==actor','if not found: return']:
            self.assertIn(guard,body)
        self.assertIn('panel.show_notice(host,binding,source,actor)',body);self.assertNotIn('dispatch',body)
    def test_only_explicit_enabled_entry(self):
        """旧profile没有新按钮，实际原row绑定也必须非空。"""
        s=(Path(__file__).resolve().parents[1]/'game/ui/task_death_notice_panel.gd').read_text()
        self.assertIn('host.kernel.has_method("private_shop_liquidations_enabled") and host.kernel.private_shop_liquidations_enabled()',s)
        self.assertIn('_estate_button(host,box,str(row.binding_fact_id),draft)',s)
if __name__=='__main__':unittest.main()
