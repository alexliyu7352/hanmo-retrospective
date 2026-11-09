"""有限日租导航源码门；不等同Godot控件或Native付款运行。"""
from pathlib import Path
import unittest
ROOT = Path(__file__).resolve().parents[1]


class DailyRentNavigation(unittest.TestCase):
    def test_original_shop_entry(self):
        text = (ROOT / 'game/ui/private_shop_panel.gd').read_text()
        self.assertIn('private_shop_daily_rent_enabled', text)
        self.assertIn('PrivateShopRentResumeRequired', text)
        self.assertIn('由店主明确复业', text)
        self.assertIn('draft.get("shop_id")!=shop', text)
        self.assertIn('DailyRentPanel.show_shop(host,shop,"","",source,actor)', text)
        self.assertIn('not _live(hr,cr,source,actor,epoch)', text)

    def test_notice_row_rechecked(self):
        text = (ROOT / 'game/ui/private_shop_liquidations_panel.gd').read_text()
        callback = text[text.index('static func _open_rent'):]
        self.assertIn('host.kernel.demand_view(QUERY,_args(draft))', callback)
        self.assertIn('page.get("source_identity")!=source', callback)
        self.assertIn('page.get("observer_actor_id")!=actor', callback)
        for field in ('shop_id', 'mandate_fact_id', 'death_notice_binding_fact_id'):
            self.assertIn('row.get("' + field + '")', callback)
        self.assertIn('DailyRentPanel.show_shop(host,shop,mandate,notice,source,actor)', callback)

    def test_actual_two_step_money_confirmation(self):
        text = (ROOT / 'game/ui/private_shop_daily_rent_panel.gd').read_text()
        self.assertIn('"selected":{}', text)
        callback = text[text.index('static func _submit'):]
        self.assertIn('d.selected!=action', callback)
        self.assertIn('host.kernel.preview(action.command,action.params,actor)', callback)
        self.assertIn('fresh.get("terms")!=action.get("quote",{}).get("terms")', callback)
        self.assertIn('cr.get_ref().disabled=true', callback)
        self.assertIn('"params":action.params.duplicate(true)', callback)
        self.assertIn('"expected_revision":source.revision', callback)
        self.assertNotIn('resume_rent', callback)


    def test_owner_history_pagination_rechecks_source(self):
        text = (ROOT / 'game/ui/private_shop_daily_rent_panel.gd').read_text()
        callback = text[text.index('static func _page'):text.index('static func _live')]
        for required in ('not _live(hr,cr,source,actor,epoch)', 'host.kernel.demand_view(QUERY,_args(d))', 'not page.get("owner_history",false)', 'page.get("history_cursor")!=d.get("cursor",0)', 'cursor!=page.get("next_cursor",-1)', 'd.selected={}'): self.assertIn(required, callback)
        self.assertNotIn('_resident_submit_frozen', callback)
        self.assertIn('row.local_payment_id', text)
        self.assertIn('row.arrear_id', text)


if __name__ == '__main__':
    unittest.main()
