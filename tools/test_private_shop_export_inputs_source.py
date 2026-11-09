"""新导出器的来源/覆盖源码门；不冒Godot执行、正式Installed或NativeWorld成功。"""
from pathlib import Path
import unittest

SOURCE = Path(__file__).resolve().parent / 'private_shop_export_inputs.gd'


class PrivateShopExportSource(unittest.TestCase):
    def test_installed_precedes_prepared_fallback_and_request(self):
        text = SOURCE.read_text()
        self.assertLess(text.index('Installed.read(args[0])'), text.index('Catalog.read_prepared(args[0])'))
        self.assertLess(text.index('Catalog.read_prepared(args[0])'), text.index('Backend.native_request(catalog,SCENARIO,PLAYER)'))
        self.assertIn('catalog.get("pack_set_digest")!=installed.get("pack_set_digest")', text)
        self.assertIn('request.get("expected_catalog_identity",{}).get("pack_set_digest")!=installed.get("pack_set_digest")', text)
        self.assertNotIn('request[', text)
        self.assertNotIn('catalog[', text)

    def test_exact_real_scenario_and_fresh_output(self):
        text = SOURCE.read_text()
        self.assertIn('const SCENARIO="private_shop::scenario.first_shop"', text)
        self.assertIn('const PLAYER="society::actor.apprentice"', text)
        self.assertLess(text.index('DirAccess.dir_exists_absolute(args[1])'), text.index('Installed.read(args[0])'))
        self.assertLess(text.index('PrivateShopNativeRequestSourceMismatch'), text.index('DirAccess.make_dir_recursive_absolute(args[1])'))
        self.assertIn('output.get_error()!=OK', text)
        self.assertNotIn('initialize_native', text)
        self.assertNotIn('create_resident', text)


if __name__ == '__main__':
    unittest.main()
