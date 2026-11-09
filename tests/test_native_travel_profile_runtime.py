"""固定Godot执行旅行代表脚本纯合同，不复制完整工程、不装库或替代正式代表门。"""
from __future__ import annotations
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class TravelProfileRuntimeContract(unittest.TestCase):
    """只有显式固定引擎可跑此微门；普通Python执行缺引擎时准确跳过。"""

    @unittest.skipUnless(os.environ.get('HANMO_GODOT_EDITOR'), '需显式固定4.7.2 HANMO_GODOT_EDITOR')
    def test_representative_script_contract(self):
        """仅两份GD与空project验证模式、来源、储存和Engine字段，约几十KB。"""
        with tempfile.TemporaryDirectory(prefix='hanmo-travel-profile-contract-') as directory:
            root = Path(directory)
            shutil.copy2(ROOT / 'game/tests/test_native_travel_profile.gd', root / 'test_native_travel_profile.gd')
            shutil.copy2(ROOT / 'tests/fixtures/native_travel_profile/check_contract.gd', root / 'check_contract.gd')
            (root / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Travel profile contract only"\n')
            environment = os.environ.copy()
            for name in ['DATA', 'CONFIG', 'CACHE']:
                path = root / ('xdg-' + name.lower())
                path.mkdir()
                environment['XDG_' + name + '_HOME'] = str(path)
            result = subprocess.run([environment['HANMO_GODOT_EDITOR'], '--headless', '--path', str(root), '--script', 'res://check_contract.gd'], env=environment, capture_output=True, text=True, timeout=30)
            output = result.stdout + result.stderr
            self.assertEqual(result.returncode, 0, output)
            self.assertNotIn('SCRIPT ERROR', output)
            self.assertIn('NATIVE_TRAVEL_PROFILE_CONTRACT_CHECKS=32 FAILURES=0', output)


if __name__ == '__main__':
    unittest.main()
