"""独立核显式native-growth入口与身份；标准库即可运行，不复制完整资源或冒称GUI。"""
from __future__ import annotations
import importlib
import hashlib
import json
import os
import subprocess
from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

ROOT = Path(os.environ.get('HANMO_PROFILE_SOURCE_ROOT', Path(__file__).resolve().parents[1])).resolve()


def profile_tool():
    """缺产品助手成为明确红断言，不因模块导入错误中断收集。"""
    path = ROOT / 'tools/native_growth_profile.py'
    assert path.is_file(), '缺少显式native-growth内部staging助手'
    module = importlib.import_module('tools.native_growth_profile')
    assert Path(module.__file__).resolve() == path, '必须从指定生产源root运行工具测试'
    return module


class ProfileEntrypointTests(unittest.TestCase):
    """CLI旧行为与新显式路由的窄门；替身只观测工具边界。"""

    def test_frozen_profile_helpers_exist(self):
        """具体业务由正式输出门覆盖，这里只核既定prepare/validate两口。"""
        helper = profile_tool()
        self.assertTrue(callable(getattr(helper, 'prepare_profile', None)))
        self.assertTrue(callable(getattr(helper, 'validate_profile', None)))

    def test_default_stage_keeps_original_dispatch(self):
        """不带profile的原stage只调用原内容准备，保显式content-set参数。"""
        build = importlib.import_module('tools.build')
        with patch.object(build, 'stage_content') as stage:
            self.assertEqual(build.main(['stage']), 0)
            stage.assert_called_once_with(content_sets=[])
            selected = Path('/tmp/explicit-existing-compose')
            self.assertEqual(build.main(['stage', '--content-set', str(selected)]), 0)
            stage.assert_called_with(content_sets=[selected])

    def test_default_test_keeps_original_dispatch(self):
        """旧test继续用原引擎查找和规则runner，不插入native套件。"""
        build = importlib.import_module('tools.build')
        with patch.object(build, 'find_engine', return_value='original-engine'), patch.object(build, 'run_tests') as run:
            self.assertEqual(build.main(['test']), 0)
            run.assert_called_once_with('original-engine')

    def test_default_export_keeps_original_dispatch(self):
        """旧export保all平台委托和原退出码，不被开发profile改变。"""
        build = importlib.import_module('tools.build')
        calls = []
        release = types.ModuleType('release')
        release.main = lambda args: calls.append(args) or 17
        with patch.dict(sys.modules, {'release': release}):
            self.assertEqual(build.main(['export']), 17)
        self.assertEqual(calls, [['export', '--platform', 'all']])

    def test_old_verify_plan_keeps_native_suites_opt_in(self):
        """原四profile仍走既有计划，三个旧native门不混进默认执行集合。"""
        verify = importlib.import_module('tools.verify')
        with tempfile.TemporaryDirectory(prefix='native-profile-plan-') as directory:
            for profile in ('architecture', 'linux', 'web', 'release'):
                plan = verify.stage_plan(ROOT, Path(directory), 'original-engine', profile)
                self.assertTrue({'engine_version', 'source_static', 'godot_import'} <= plan.keys())
                for name in ('test_native_inventory_bridge', 'test_native_actor_commitments_bridge', 'test_live_native_growth_action'):
                    self.assertNotIn(name, verify.TESTS)
                    self.assertIn(name, verify.EXCLUDED_TESTS)
                    self.assertNotIn('godot_' + name, plan)

    def test_native_cli_requires_explicit_new_destination_and_library(self):
        """显式新profile须委托既定助手，不能误走原stage修改默认工程。"""
        build = importlib.import_module('tools.build')
        helper = profile_tool()
        calls = []
        def prepare(root, output, native_library, content_sets=()):
            """只记录CLI边界参数；此替身不作为正式stage成功证据。"""
            calls.append((root, output, native_library, content_sets))
            return {'game': str(output / 'game'), 'manifest_path': str(output / 'native-growth-stage.json'), 'manifest': {}}
        with tempfile.TemporaryDirectory(prefix='native-profile-cli-') as directory:
            base = Path(directory); output = base / 'isolated'; library = base / 'exact.so'; content = base / 'compose'
            with patch.object(helper, 'prepare_profile', prepare), patch.object(build, 'stage_content', side_effect=AssertionError('native profile不能写默认root')):
                self.assertEqual(build.main(['stage', '--profile', 'native-growth', '--output', str(output), '--native-library', str(library), '--content-set', str(content)]), 0)
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0][0], build.ROOT)
            self.assertEqual(Path(calls[0][1]), output)
            self.assertEqual(Path(calls[0][2]), library)
            self.assertEqual(list(calls[0][3]), [content])


class ProfilePreflightTests(unittest.TestCase):
    """不准备资产的真实前置守卫，所有路径都在独立临时目录内。"""

    def test_output_overlap_existing_and_symlink_are_rejected(self):
        """既有／父子／别名输出都在物化之前拒绝，已有文件原样保留。"""
        helper = profile_tool()
        with tempfile.TemporaryDirectory(prefix='native-profile-isolation-') as directory:
            base = Path(directory); root = base / 'source'; root.mkdir()
            existing = base / 'existing'; existing.mkdir(); marker = existing / 'keep'; marker.write_bytes(b'owned-evidence')
            library = base / 'unused.so'
            for output in (existing, root, root / 'nested', base):
                with self.subTest(output=output), patch.object(helper, '_materialize', side_effect=AssertionError('must not materialize')):
                    with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/OutputNotIsolated:'):
                        helper.prepare_profile(root, output, library)
            alias = base / 'alias'; alias.symlink_to(root, target_is_directory=True)
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/UnsafePath:'):
                helper.prepare_profile(alias, base / 'fresh', library)
            self.assertEqual(marker.read_bytes(), b'owned-evidence')
            self.assertFalse((base / 'fresh').exists())

    def test_dirty_source_is_explicitly_refused(self):
        """脏生产源不得伪装成Git身份，不运行任何资源准备。"""
        helper = profile_tool()
        with patch.object(helper, 'source_identity', return_value={'dirty_paths': [{'path': 'game/core/world_kernel.gd'}]}):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/DirtySource:'):
                helper._clean_source(ROOT)

    def test_library_hash_and_elf_checks_are_independent(self):
        """任意冒名库先被真实SHA拒，独立ELF门不被已知摘要分支省略。"""
        helper = profile_tool()
        with tempfile.TemporaryDirectory(prefix='native-profile-library-') as directory:
            library = Path(directory) / 'fake.so'; library.write_bytes(b'not-an-ELF-library')
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/UnreviewedNativeLibrary:'):
                helper._native_identity(ROOT, 'a' * 40, library)
            with patch.object(helper, 'sha256_file', return_value=helper.LIBRARY_SHA256):
                with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/InvalidNativeELF:'):
                    helper._native_identity(ROOT, 'a' * 40, library)

    def test_configuration_preserves_default_scene_and_isolates_storage(self):
        """只输出配置变化，原project保持默认off和玩家存储字节。"""
        helper = profile_tool()
        commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
        raw = helper._blob(ROOT, commit, 'game/project.godot')
        original = hashlib.sha256(raw).hexdigest()
        first = helper._storage(Path('/tmp/native-growth-output-one'), commit, 'a' * 32)
        second = helper._storage(Path('/tmp/native-growth-output-two'), commit, 'a' * 32)
        rebuilt = helper._storage(Path('/tmp/native-growth-output-one'), commit, 'b' * 32)
        self.assertNotEqual(first, rebuilt)
        self.assertEqual(first, helper._storage(Path('/tmp/native-growth-output-one'), commit, 'a' * 32))
        with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/StorageMismatch:'):
            helper._storage(Path('/tmp/native-growth-output-one'), commit, 'invalid-nonce')
        self.assertNotEqual(first, second)
        self.assertNotEqual(first['profile'], 'player-alpha')
        prepared = helper._project_config(raw, first, commit).decode()
        self.assertIn('run/main_scene="res://main.tscn"', prepared)
        self.assertIn('native_growth_action_enabled=true', prepared)
        self.assertIn('native_projection_source_commit="' + commit + '"', prepared)
        self.assertIn('config/storage_profile="' + first['profile'] + '"', prepared)
        self.assertIn('config/custom_user_dir_name="' + first['custom_user_dir_name'] + '"', prepared)
        self.assertNotIn('native_growth_action_enabled=true', raw.decode())
        self.assertEqual(hashlib.sha256(helper._blob(ROOT, commit, 'game/project.godot')).hexdigest(), original)

    def test_stale_manifest_commit_rejected_before_asset_scan(self):
        """最小负例只抵达来源首错，不能为了测试错commit复制正式资源。"""
        helper = profile_tool()
        with tempfile.TemporaryDirectory(prefix='native-profile-stale-') as directory:
            output = Path(directory); game = output / 'game'; game.mkdir()
            manifest = {'format': helper.FORMAT, 'profile': 'native-growth', 'game': str(game), 'gd_source': {'commit': 'not-a-commit'},
                        'native': {}, 'engine': {}, 'content': {}, 'storage': {}, 'configuration': {}}
            (output / helper.MANIFEST).write_text(json.dumps(manifest))
            with patch.object(helper, '_clean_source', return_value={'commit': 'a' * 40, 'dirty_paths': []}), patch.object(helper, '_source_entries', side_effect=AssertionError('must not scan assets')):
                with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/SourceCommitMismatch:'):
                    helper.validate_profile(ROOT, game)


@unittest.skipUnless(os.environ.get('HANMO_NATIVE_GROWTH_STAGED_GAME'), 'requires the one frozen full-resource stage; no implicit stage/copy')
class FrozenStageReadOnlyTests(unittest.TestCase):
    """只读复用一次正式stage；错误输入在读取边界注入，不修改GUI正在用的文件。"""

    @classmethod
    def setUpClass(cls):
        """只做一次完整真实validate，随后负例可跳过已经证明不变的大资源扫描。"""
        cls.helper = profile_tool()
        cls.game = Path(os.environ['HANMO_NATIVE_GROWTH_STAGED_GAME']).resolve()
        cls.output = cls.game.parent
        cls.manifest_path = cls.output / cls.helper.MANIFEST
        cls.before = cls.manifest_path.read_bytes()
        cls.valid = cls.helper.validate_profile(ROOT, cls.game)
        cls.manifest = deepcopy(cls.valid['manifest'])
        cls.project_semantics = cls.helper._project_semantic_sha256((cls.game / 'project.godot').read_bytes())
        cls.hashes = {name: hashlib.sha256((cls.game / name).read_bytes()).hexdigest()
                      for name in [cls.helper.EXTENSION, cls.helper.LIBRARY]}

    @classmethod
    def tearDownClass(cls):
        """全部负例之后复核关键冻结字节；monkeypatch绝不落盘。"""
        assert cls.manifest_path.read_bytes() == cls.before
        assert cls.helper._project_semantic_sha256((cls.game / 'project.godot').read_bytes()) == cls.project_semantics
        assert cls.hashes == {name: hashlib.sha256((cls.game / name).read_bytes()).hexdigest() for name in cls.hashes}

    def test_formal_default_stage_identity(self):
        """完整真实作者与默认场景，不能以tiny、重新命名或另一个开发工程代替。"""
        self.assertEqual(Path(self.valid['game']), self.game)
        self.assertEqual(self.manifest['content']['entry']['content_id'], 'foundation.alpha_life')
        self.assertEqual(self.manifest['content']['entry']['scenario_id'], 'scenario.alpha_life')
        self.assertEqual(self.manifest['native']['library_sha256'], self.helper.LIBRARY_SHA256)
        self.assertNotEqual(self.manifest['storage']['profile'], 'player-alpha')

    def _manifest_failure(self, changes, expected):
        """仅替选定manifest读取；其他来源与受信生产核验仍真实执行。"""
        changed = deepcopy(self.manifest); changes(changed)
        original = self.helper.content_stage.read_pack
        def read(path):
            """对本次故障的manifest给隔离副本，其他文件保持原读取。"""
            return deepcopy(changed) if Path(path).resolve() == self.manifest_path else original(path)
        with patch.object(self.helper.content_stage, 'read_pack', read), patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(self.helper, '_content_identity', return_value=deepcopy(self.manifest['content'])):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/' + expected + ':'):
                self.helper.validate_profile(ROOT, self.game)

    def test_manifest_source_storage_native_and_configuration_mismatches(self):
        """来源／存储／库／lock声明／配置摘要不匹配，各自触发明确首错。"""
        cases = [(['gd_source', 'commit'], 'not-a-commit', 'SourceCommitMismatch'),
                 (['storage', 'profile'], 'player-alpha', 'StorageMismatch'),
                 (['native', 'library_sha256'], '0' * 64, 'NativeIdentityMismatch'),
                 (['native', 'dependency_lock_sha256'], '0' * 64, 'NativeIdentityMismatch'),
                 (['configuration', 'project_sha256'], '0' * 64, 'ConfigurationMismatch')]
        for path, value, expected in cases:
            with self.subTest(path=path):
                self._manifest_failure(lambda m: m[path[0]].__setitem__(path[1], value), expected)

    def test_actual_gd_and_configuration_bytes_are_checked(self):
        """将待测文件置于只读扫描首位，实际内容hash／配置核验拒绝，避免每例重读全部图片。"""
        commit = self.manifest['gd_source']['commit']
        entries = self.helper._source_entries(ROOT, commit)
        for name, expected in [('game/core/world_kernel.gd', 'StagedSourceMismatch'), ('game/project.godot', 'ConfigurationMismatch')]:
            target = self.output / name; original = Path.read_bytes
            def read(path):
                """仅改变返回字节，磁盘原件不写入。"""
                data = original(path)
                if path != target:
                    return data
                if name == 'game/project.godot':
                    changed = data.replace(b'native_growth_action_enabled=true', b'native_growth_action_enabled=false', 1)
                    assert changed != data, 'must alter the actual development switch'
                    return changed
                return data + b'\n# injected mismatch\n'
            ordered = {name: entries[name], **entries}
            with self.subTest(path=name), patch.object(self.helper, '_source_entries', return_value=ordered), patch.object(Path, 'read_bytes', read):
                with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/' + expected + ':'):
                    self.helper.validate_profile(ROOT, self.game)

    def test_actual_library_and_dependency_lock_rejected(self):
        """库真实摘要与当前Git锁分别拒绝；不复制或篡改正式so。"""
        library = self.game / self.helper.LIBRARY; original_hash = self.helper.sha256_file
        def hash_file(path):
            """只注入实际库的一次读取摘要错误，其他文件用生产hash。"""
            return '0' * 64 if Path(path) == library else original_hash(path)
        with patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(self.helper, 'sha256_file', hash_file):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/UnreviewedNativeLibrary:'):
                self.helper.validate_profile(ROOT, self.game)
        original_blob = self.helper._blob
        def blob(root, commit, name):
            """只改变当前GD源码所读依赖锁，历史编译锁仍真实。"""
            data = original_blob(root, commit, name)
            return data + b'\n' if commit == self.manifest['gd_source']['commit'] and name == 'native/dependencies.lock.json' else data
        with patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(self.helper, '_blob', blob):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/NativeLockMismatch:'):
                self.helper.validate_profile(ROOT, self.game)

    def test_native_source_changes_cannot_reuse_the_reviewed_binary(self):
        """即使同so通过SHA，当前C++实现与已审编译源不同仍须拒绝。"""
        original = self.helper._blob
        def blob(root, commit, name):
            """仅注入当前GD提交所绑定C++源差异，历史编译对象保持原字节。"""
            data = original(root, commit, name)
            if commit == self.manifest['gd_source']['commit'] and name == 'native/godot/hanmo_world_bridge.cpp':
                return data + b'\n// injected mismatch\n'
            return data
        with patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(self.helper, '_blob', blob):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/NativeSourceMismatch:'):
                self.helper.validate_profile(ROOT, self.game)

    def test_extension_bytes_rejected_without_modifying_the_file(self):
        """扩展描述即使不在Git运行条目内，也必须逐字等于正式模板。"""
        target = self.game / self.helper.EXTENSION; original = Path.read_bytes
        def read(path):
            """只返回错误入口字节，不改变实际GUI正在加载的描述。"""
            data = original(path)
            return data + b'\n# changed extension\n' if path == target else data
        with patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(Path, 'read_bytes', read):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/ExtensionMismatch:'):
                self.helper.validate_profile(ROOT, self.game)

    def test_content_manifest_invalidity_propagates(self):
        """真实内容validator拒绝坏清单形状，不将上一份有效profile当作缓存PASS。"""
        content_manifest = self.game / 'data/generated' / self.helper.content_stage.MANIFEST
        original = self.helper.content_stage.read_pack
        def read(path):
            """只替内容manifest，实际validate_stage/read_manifest继续运行。"""
            if Path(path) == content_manifest:return {'format': 'invalid-content-stage'}
            return original(path)
        with patch.object(self.helper, '_source_check', return_value=self.manifest['gd_source']['staged_files_sha256']), patch.object(self.helper.content_stage, 'read_pack', read):
            with self.assertRaisesRegex(RuntimeError, '^NativeGrowthProfile/InvalidStage:.*Invalid content stage'):
                self.helper.validate_profile(ROOT, self.game)


if __name__ == '__main__':
    unittest.main()
