"""同一构建与验证入口的闭合双profile接线；只测路由，不物化完整资源。"""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from tools import build, verify


class NativeDevelopmentEntrypointTests(unittest.TestCase):
    """新模式必须走原helper/runner，不能误入旧全量默认路径。"""

    def test_build_travel_uses_shared_helper(self):
        """新profile显式传给共用prepare，旧stage不会被调用。"""
        out=Path('/tmp/native-travel-explicit'); lib=Path('/tmp/reviewed.so')
        with patch('tools.native_growth_profile.prepare_profile',return_value={'game':str(out/'game'),'manifest_path':str(out/'native-travel-stage.json')}) as prepare, patch.object(build,'stage_content') as old:
            try: code=build.main(['stage','--profile','native-travel','--output',str(out),'--native-library',str(lib)])
            except SystemExit as exc: code=exc.code
            self.assertEqual(code,0)
            prepare.assert_called_once_with(build.ROOT,out,lib,content_sets=[],profile='native-travel')
            old.assert_not_called()

    def test_verify_travel_uses_same_bounded_runner(self):
        """正式验证新mode只进入同一个开发runner并明确传profile。"""
        with tempfile.TemporaryDirectory() as tmp, patch.object(verify,'run_native_growth_verification',return_value={'ok':True}) as run:
            try: result=verify.run_verification(Path(tmp),'native-travel',Path(tmp)/'evidence','godot',staged_project=Path(tmp)/'game')
            except ValueError: result={'ok':False}
            self.assertTrue(result['ok'])
            self.assertEqual(run.call_args.kwargs.get('profile'),'native-travel')

    def test_native_mode_cannot_enter_full_default_plan(self):
        """两个开发mode直接调用旧plan时都要求显式stage上下文。"""
        for mode in ['native-growth','native-travel']:
            with self.subTest(mode=mode), self.assertRaisesRegex(ValueError,'显式staged-project'):
                verify.stage_plan(Path('/tmp/source'),Path('/tmp/run'),'godot',mode)
