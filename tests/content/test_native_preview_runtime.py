"""以最小真实ELF与独立Git源验证作者原生试玩准入，不运行游戏或接触旧银行。"""
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools import native_resident_profile as resident
from tools.content.pack_errors import PackError


class NativePreviewRuntimeTests(unittest.TestCase):
    """覆盖缺件、伪身份与源变化，夹具二进制不是游戏或玩法验收证据。"""

    def setUp(self):
        """建立微型本地Git生产树与单导出符号ELF，不重编正式原生库。"""
        if not shutil.which('cc') or not shutil.which('nm'):
            self.skipTest('需要本地cc/nm生成和读取最小ELF夹具')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / 'source'
        self.root.mkdir()
        self.game = Path(self.temporary.name) / 'stage/game'
        self.game.mkdir(parents=True)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Native Preview Fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.write(self.root / 'native/dependencies.lock.json', b'{}\n')
        self.write(self.root / 'native/world/src/core.c', b'/* fixture */\n')
        self.template = b'[configuration]\nentry_symbol = "hanmo_world_library_init"\ncompatibility_minimum = "4.7.2"\nreloadable = false\n\n[libraries]\nlinux.debug.x86_64 = "@HANMO_BRIDGE_LIBRARY@"\n'
        self.write(self.root / 'native/godot/hanmo_world.gdextension.in', self.template)
        self.git('add', 'native')
        self.git('commit', '-qm', 'Fixture native source')
        native_commit = self.git('rev-parse', 'HEAD').strip()
        self.library = self.game / resident.LIBRARY
        self.library.parent.mkdir(parents=True)
        subprocess.run(['cc', '-shared', '-fPIC', '-nostdlib', '-x', 'c', '-', '-o', str(self.library)],
                       input=b'void hanmo_world_library_init(void) {}\n', check=True, capture_output=True)
        self.config = {'format': 'HM-NATIVE-RESIDENT-RELEASE/1', 'export_mode': 'debug',
            'engine': {'version': '4.7.2.stable.official.ed1daf0bf', 'sha256': '1' * 64},
            'native': {'source_commit': native_commit, 'library_sha256': resident.sha256_file(self.library)},
            'content': {'pack_set_digest': '4' * 64, 'scenario_id': 'fixture::old-stage'},
            'storage': {'profile': 'old-stage', 'custom_user_dir_name': 'old-stage'}}
        self.write(self.root / resident.CONFIG_PATH, self.encode(self.config))
        self.git('add', resident.CONFIG_PATH)
        self.git('commit', '-qm', 'Fixture profile')
        self.receipt = {'format': 'HANMO-RECOVERY-NATIVE-BUILD/1', 'status': 'BUILD_COMPLETE_RUNTIME_NOT_RUN',
            'source_commit': native_commit, 'source_commit_after': native_commit, 'source_status_after': '',
            'new_recovery_build': False, 'historical_artifacts_recreated': False,
            'dependencies_unchanged_during_build': True, 'engine': {'sha256': '1' * 64},
            'configuration': {'test_faults': False}, 'completed_at': '2026-01-01T00:00:00+00:00',
            'stages': [{'name': name, 'exit_code': 0, 'completed_at': '2026-01-01T00:00:00+00:00'}
                       for name in ['configure', 'build-hanmo_resident_journey', 'build-hanmo_world_bridge']],
            'artifacts': [{'path': '/fixture/libhanmo_world_bridge.so', 'sha256': self.config['native']['library_sha256']}],
            'bridge_identity': {'dependency_lock_sha256': resident._sha(b'{}\n'), 'resident_core_source_sha256': '6' * 64}}
        self.write(self.game.parent / 'release-profile.json', self.encode(self.config))
        self.write(self.game.parent / 'native-build-receipt.json', self.encode(self.receipt))
        self.write(self.game / resident.EXTENSION, self.template.replace(b'@HANMO_BRIDGE_LIBRARY@', ('res://' + resident.LIBRARY).encode()))
        native = resident._native_identity(self.root, self.git('rev-parse', 'HEAD').strip(), self.library,
                                            self.game.parent / 'native-build-receipt.json', self.config)
        self.manifest = {'format': resident.FORMAT, 'profile': resident.PROFILE, 'configuration': self.config,
            'profile_sha256': resident._sha(self.encode(self.config)), 'native': native,
            'status': {'stage': 'PASS'}, 'files': resident._files(self.game.parent),
            'runtime_source_blobs': {'game/old.gd': 'obsolete-gd-is-not-used'}}
        self.save_manifest()

    def git(self, *args):
        """只操作当前临时夹具仓库，失败直接保留Git诊断。"""
        return subprocess.run(['git', *args], cwd=self.root, check=True, capture_output=True, text=True).stdout

    def write(self, path, raw):
        """写入独立测试树中的原字节。"""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)

    def encode(self, value):
        """统一夹具JSON字节以便摘要保持可核。"""
        return json.dumps(value, sort_keys=True).encode()

    def save_manifest(self):
        """保存当前故障场景清单，不自动修正其他被篡改的身份。"""
        self.write(self.game.parent / resident.MANIFEST, self.encode(self.manifest))

    def prepare(self):
        """调用待实现入口，让缺功能首先成为可见失败。"""
        from tools.content.preview_native import prepare_native_runtime
        return prepare_native_runtime(self.root, self.game)

    def test_copies_only_verified_native_bytes_and_registration(self):
        """旧stage的GD与内容不得被带入，只有两件原生文件及固定注册表。"""
        self.write(self.game / 'old.gd', b'untrusted stage script')
        result = self.prepare()
        self.assertEqual(set(result['files']), {resident.LIBRARY, resident.EXTENSION, '.godot/extension_list.cfg'})
        self.assertEqual(result['files'][resident.LIBRARY], self.library.read_bytes())
        self.assertEqual(result['files']['.godot/extension_list.cfg'], ('res://' + resident.EXTENSION + '\n').encode())
        self.assertEqual(result['identity']['native'], self.manifest['native'])
        self.assertEqual(result['identity']['engine'], self.config['engine'])
        json.dumps(result['identity'])
        self.assertIn(self.game.parent / resident.MANIFEST, result['source_paths'])
        self.assertTrue(all(path.is_absolute() and path.read_bytes() == raw for path, raw in result['source_paths'].items()))

    def test_missing_library_has_specific_error(self):
        """缺库不能返回貌似可运行的预览文件。"""
        self.library.unlink()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'MissingPreviewNativeRuntime')

    def test_manifest_native_identity_must_match_original_receipt(self):
        """清单伪造原生身份不能绕过生产回执。"""
        self.manifest['native']['source_commit'] = 'f' * 40
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')

    def test_self_consistent_foreign_descriptor_is_rejected(self):
        """即使重算清单摘要，指向外来动态库的描述仍不等于受信模板。"""
        raw = (self.game / resident.EXTENSION).read_bytes().replace(b'libhanmo_world_bridge.so', b'foreign.so')
        self.write(self.game / resident.EXTENSION, raw)
        self.manifest['files']['game/' + resident.EXTENSION] = resident._sha(raw)
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')

    def test_current_production_change_requires_matching_native_build(self):
        """后继UI提交可复用库，真正生产源变化必须拒绝旧库。"""
        self.write(self.root / 'ui.txt', b'new ui')
        self.git('add', 'ui.txt')
        self.git('commit', '-qm', 'UI only')
        self.prepare()
        self.write(self.root / 'native/world/src/core.c', b'/* changed production */\n')
        self.git('add', 'native/world/src/core.c')
        self.git('commit', '-qm', 'Changed native production')
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')
        self.assertIn('NativeSourceMismatch', str(caught.exception.details))

    def test_invalid_original_receipt_cannot_be_authorized_by_stage(self):
        """同步重算stage摘要也不能把故障注入构建伪造成生产回执。"""
        self.receipt['configuration']['test_faults'] = True
        raw = self.encode(self.receipt)
        self.write(self.game.parent / 'native-build-receipt.json', raw)
        self.manifest['files']['native-build-receipt.json'] = resident._sha(raw)
        self.manifest['native']['build_receipt_sha256'] = resident._sha(raw)
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')
        self.assertIn('BuildReceipt', str(caught.exception.details))

    def test_non_elf_library_fails_even_with_matching_local_digests(self):
        """库摘要与stage自述一致仍必须通过原ELF机器与ABI检查。"""
        raw = b'This is not a native library.'
        self.library.write_bytes(raw)
        digest = resident._sha(raw)
        self.config['native']['library_sha256'] = digest
        config_raw = self.encode(self.config)
        self.write(self.game.parent / 'release-profile.json', config_raw)
        self.manifest['profile_sha256'] = resident._sha(config_raw)
        self.manifest['files']['release-profile.json'] = resident._sha(config_raw)
        self.manifest['files']['game/' + resident.LIBRARY] = digest
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertIn('NativeELF', str(caught.exception.details))

    def test_stage_cannot_choose_another_fixed_engine(self):
        """stage及receipt不能自行改写受信ROOT中的引擎身份。"""
        self.config['engine']['sha256'] = '9' * 64
        raw = self.encode(self.config)
        self.write(self.game.parent / 'release-profile.json', raw)
        self.manifest['profile_sha256'] = resident._sha(raw)
        self.manifest['files']['release-profile.json'] = resident._sha(raw)
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')
        self.assertIn('engine identity', str(caught.exception.details))

    def test_extra_exported_symbol_cannot_pass_native_abi(self):
        """有效ELF有额外导出时仍由原符号白名单阻断。"""
        subprocess.run(['cc', '-shared', '-fPIC', '-nostdlib', '-x', 'c', '-', '-o', str(self.library)],
                       input=b'void hanmo_world_library_init(void) {}\nvoid unexpected_export(void) {}\n',
                       check=True, capture_output=True)
        digest = resident.sha256_file(self.library)
        self.config['native']['library_sha256'] = digest
        raw = self.encode(self.config)
        self.write(self.game.parent / 'release-profile.json', raw)
        self.manifest['profile_sha256'] = resident._sha(raw)
        self.manifest['files']['release-profile.json'] = resident._sha(raw)
        self.manifest['files']['game/' + resident.LIBRARY] = digest
        self.save_manifest()
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertIn('NativeExports', str(caught.exception.details))

    def test_symlinked_library_is_rejected_before_reading(self):
        """即使链接指向同一原字节，也不能扩大stage输入的实际归属。"""
        external = self.game.parent / 'outside.so'
        self.library.rename(external)
        self.library.symlink_to(external)
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertIn('UnsafePath', str(caught.exception.details))

    def test_uncommitted_native_source_cannot_reuse_head_identity(self):
        """未提交原生改动不能借仍匹配的HEAD身份复用旧库。"""
        self.write(self.root / 'native/world/src/core.c', b'/* uncommitted source */\n')
        with self.assertRaises(PackError) as caught:
            self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewNativeIdentityMismatch')
        self.assertIn('uncommitted', str(caught.exception.details))

    def test_source_changed_during_identity_check_is_rejected(self):
        """身份核验后的来源变化仍由末尾原字节守卫阻断。"""
        original = resident._native_identity

        def mutate_after_validation(*args):
            """在真实校验完成后注入并发文件变化，不模拟校验结果。"""
            result = original(*args)
            self.library.write_bytes(b'changed during preparation')
            return result

        with patch.object(resident, '_native_identity', side_effect=mutate_after_validation):
            with self.assertRaises(PackError) as caught:
                self.prepare()
        self.assertEqual(caught.exception.code, 'PreviewInputsChanged')


if __name__ == '__main__':
    unittest.main()
