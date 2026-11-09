"""resident标准打包的定点边界测试；不运行世界规则、旧功能矩阵或GUI。"""
import copy
import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from tools import build, release, native_resident_profile as resident


class NativeResidentProfileTests(unittest.TestCase):
    """验证身份、输出归属、标准CLI和ZIP完整性等会影响交付的失败边界。"""

    def setUp(self):
        """每个用例使用独立临时目录，不读取真实银行或复用旧产物。"""
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.config = {'format': 'HM-NATIVE-RESIDENT-RELEASE/1', 'export_mode': 'debug',
            'engine': {'version': '4.7.2.stable.official.ed1daf0bf', 'sha256': '1' * 64},
            'native': {'source_commit': '2' * 40, 'library_sha256': '3' * 64},
            'content': {'pack_set_digest': '4' * 64, 'scenario_id': 'society::scenario.qingchuan_spring_reserve'},
            'storage': {'profile': 'resident-qingchuan', 'custom_user_dir_name': 'hanmo-resident-qingchuan'}}

    def write_config(self, value=None):
        """写出非秘密配置夹具，后续只验证路径及字段契约。"""
        path = self.root / 'config.json'
        path.write_text(json.dumps(self.config if value is None else value))
        return path

    def receipt(self):
        """构造已终态回执的结构夹具，不把它作为真实构建或玩法证据。"""
        return {'format': 'HANMO-RECOVERY-NATIVE-BUILD/1', 'status': 'BUILD_COMPLETE_RUNTIME_NOT_RUN',
            'source_commit': '2' * 40, 'source_commit_after': '2' * 40, 'source_status_after': '',
            'new_recovery_build': True, 'historical_artifacts_recreated': False,
            'dependencies_unchanged_during_build': True, 'engine': {'sha256': '1' * 64},
            'configuration': {'test_faults': False}, 'completed_at': '2026-01-01T00:00:00+00:00',
            'stages': [{'name': name, 'exit_code': 0, 'completed_at': '2026-01-01T00:00:00+00:00'}
                       for name in ['configure', 'build-hanmo_resident_journey', 'build-hanmo_world_bridge']],
            'artifacts': [{'path': '/fixture/libhanmo_world_bridge.so', 'sha256': '3' * 64}]}

    def package(self):
        """建立只供ZIP边界用例的明确虚拟目录，不称为可运行游戏。"""
        folder = self.root / 'delivery'
        folder.mkdir()
        for name in ['start.sh', 'hanmo.x86_64']:
            (folder / name).write_bytes(b'fixture-only\n')
            (folder / name).chmod(0o755)
        (folder / 'native.so').write_bytes(b'native-fixture')
        manifest = {'format': 'HM-NATIVE-RESIDENT-LINUX-BUILD/1', 'export_mode': 'debug',
                    'acceptance': {'packed_content': 'PASS'}, 'files': resident._files(folder)}
        (folder / 'BUILD.json').write_text(json.dumps(manifest))
        return folder, manifest

    def test_profile_requires_debug_exact_engine_and_stable_storage(self):
        """合法本章配置可读取，release ABI、未知引擎或穿越存储键必须拒绝。"""
        self.assertEqual(resident._read_config(self.write_config()), self.config)
        invalid = []
        for field, value in [('export_mode', 'release'), ('engine', {'version': '4.8', 'sha256': '1' * 64}),
                             ('storage', {'profile': '../old', 'custom_user_dir_name': 'valid'})]:
            variant = copy.deepcopy(self.config); variant[field] = value; invalid.append(variant)
        invalid += [[], {'format': self.config['format'], 'export_mode': 'debug'}]
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(RuntimeError):
                resident._read_config(self.write_config(value))

    def test_project_changes_only_local_entry_and_resident_switch(self):
        """同一Main与其余项目键保留，配置不按输出路径随机换存档键。"""
        raw = b'[application]\nconfig/storage_profile="old"\nconfig/custom_user_dir_name="old"\nconfig/default_scenario="old"\nconfig/default_catalog="old"\nrun/main_scene="res://main.tscn"\n'
        result = resident._project_config(raw, self.config, 'res://data/generated/fixed.json')
        self.assertIn(b'run/main_scene="res://main.tscn"', result)
        self.assertIn(b'native_resident_world_enabled=true', result)
        self.assertIn(b'config/storage_profile="resident-qingchuan"', result)
        self.assertEqual(result, resident._project_config(raw, self.config, 'res://data/generated/fixed.json'))
        with self.assertRaises(RuntimeError):
            resident._project_config(raw + b'[development]\n', self.config, 'res://data/generated/fixed.json')

    def test_original_linux_preset_retains_debug_and_web_settings(self):
        """只补实际需要的本章JSON，原debug模板配置和Web节不被重造。"""
        raw = b'[preset.0]\nname="Linux"\ninclude_filter="data/generated/*.json"\n[preset.0.options]\ncustom_template/debug=""\nbinary_format/embed_pck=true\n[preset.1]\nname="Web"\n'
        result = resident._export_presets(raw)
        self.assertIn(b'data/pack_runtime.json', result)
        self.assertIn(b'data/accepted_adult_appearances.json', result)
        self.assertTrue(result.endswith(b'[preset.1]\nname="Web"\n'))
        self.assertIn(b'custom_template/debug=""', result)

    def test_package_requirements_keep_hidden_marker_and_original_png(self):
        """原始PNG摘要、隐藏marker及导入资源要求并存，不能因纹理已导入而假绿。"""
        prefix = 'data/generated/content-sets/' + '4' * 64
        manifest = {'files': {'game/' + prefix + '/asset.png': 'a' * 64,
                             'game/' + prefix + '/.pack-generation.json': 'b' * 64,
                             'game/data/pack_runtime.json': 'c' * 64},
                    'content': {'installed_set': {'directory': 'content-sets/' + '4' * 64,
                        'files': [{'path': 'asset.png', 'sha256': 'a' * 64},
                                  {'path': '.pack-generation.json', 'sha256': 'b' * 64}]},
                        'catalog_path': 'res://' + prefix + '/catalog.hmcatalog.json', 'pack_set_digest': '4' * 64}}
        result = resident._package_requirements(manifest)
        self.assertIn({'path': 'res://' + prefix + '/asset.png', 'sha256': 'a' * 64}, result['files'])
        self.assertIn({'path': 'res://' + prefix + '/.pack-generation.json', 'sha256': 'b' * 64}, result['files'])
        self.assertIn('res://' + prefix + '/asset.png', result['resources'])
        self.assertEqual(result['catalog_path'], manifest['content']['catalog_path'])

    def test_export_plugin_is_enabled_only_in_isolated_project(self):
        """只扩展隔离副本中的插件列表，不改原作者插件与Main配置。"""
        raw = b'[editor_plugins]\n\nenabled=PackedStringArray("res://addons/hanmo_author/plugin.cfg")\n\n[rendering]\nmode="unchanged"\n'
        modified = resident._enable_export_plugin(raw)
        self.assertIn(b'"res://addons/hanmo_author/plugin.cfg", "res://addons/hanmo_resident_export/plugin.cfg"', modified)
        self.assertTrue(modified.endswith(b'[rendering]\nmode="unchanged"\n'))

    def test_native_licenses_require_exact_locked_bytes(self):
        """真实依赖许可须匹配锁摘要，换文或缺件不得装进交付目录。"""
        lock = {'libraries': {}, 'godot_bridge': {'binding': {'files': {}}}}
        receipt = {'dependencies': {}}
        for name, filename in [('flecs', 'LICENSE'), ('json', 'LICENSE.MIT'), ('picosha2', 'LICENSE'), ('godot-cpp', 'LICENSE.md')]:
            directory = self.root / name; directory.mkdir()
            raw = (name + ' actual license fixture').encode(); (directory / filename).write_bytes(raw)
            receipt['dependencies'][name] = {'root': str(directory)}
            target = lock['godot_bridge']['binding'] if name == 'godot-cpp' else lock['libraries'].setdefault(name, {'files': {}})
            target['files'][filename] = resident._sha(raw)
        path = self.root / 'receipt.json'; path.write_text(json.dumps(receipt))
        with patch.object(resident, '_blob', return_value=json.dumps(lock).encode()):
            self.assertEqual(len(resident._native_license_bytes(self.root, '2' * 40, path)), 4)
            (self.root / 'flecs/LICENSE').write_text('changed')
            with self.assertRaisesRegex(RuntimeError, 'NativeLicenseMismatch'):
                resident._native_license_bytes(self.root, '2' * 40, path)

    def test_player_readme_uses_current_chapter_storage(self):
        """试玩说明绑定实际章节存储，不沿用旧Alpha默认路径或不存在的存档导入入口。"""
        text = resident._player_readme(self.config)
        self.assertIn('hanmo-resident-qingchuan/profiles/resident-qingchuan/saves', text)
        self.assertNotIn('player-alpha', text)
        self.assertIn('NATIVE-LICENSES', text)

    def test_build_receipt_rejects_failed_forged_or_injected_build(self):
        """错误源码、库摘要、非终态以及故障注入回执都不能授权交付。"""
        receipt = self.receipt()
        resident._validate_build_receipt(receipt, '2' * 40, '3' * 64, '1' * 64)
        for key, value in [('source_commit_after', '5' * 40), ('status', 'RUNNING'),
                           ('configuration', {'test_faults': True}), ('source_status_after', ' M source.cpp'),
                           ('historical_artifacts_recreated', True), ('artifacts', [])]:
            variant = copy.deepcopy(receipt); variant[key] = value
            with self.subTest(key=key), self.assertRaises(RuntimeError):
                resident._validate_build_receipt(variant, '2' * 40, '3' * 64, '1' * 64)
        receipt['stages'][0]['exit_code'] = 1
        with self.assertRaises(RuntimeError):
            resident._validate_build_receipt(receipt, '2' * 40, '3' * 64, '1' * 64)

    def test_build_receipt_accepts_exact_fresh_and_incremental_modes(self):
        """True新建与False增量都必须沿同一完整回执门，验证不得改写原模式。"""
        for mode in [True, False]:
            with self.subTest(mode=mode):
                receipt = self.receipt(); receipt['new_recovery_build'] = mode
                original = copy.deepcopy(receipt)
                resident._validate_build_receipt(receipt, '2' * 40, '3' * 64, '1' * 64)
                self.assertEqual(receipt, original)
                self.assertIs(receipt['new_recovery_build'], mode)

    def test_build_receipt_rejects_missing_or_coerced_mode(self):
        """数值0/1、字符串、空值和缺字段不能借布尔等价进入生产准入。"""
        for value in [None, 0, 1, -1, '', 'false', 'true', [], {}]:
            variant = self.receipt(); variant['new_recovery_build'] = value
            with self.subTest(value=value), self.assertRaisesRegex(RuntimeError, 'BuildReceipt'):
                resident._validate_build_receipt(variant, '2' * 40, '3' * 64, '1' * 64)
        variant = self.receipt(); del variant['new_recovery_build']
        with self.assertRaisesRegex(RuntimeError, 'BuildReceipt'):
            resident._validate_build_receipt(variant, '2' * 40, '3' * 64, '1' * 64)

    def test_incremental_receipt_keeps_exact_identity_and_terminal_guards(self):
        """增量模式不能放松原源码、引擎、依赖、库摘要和全阶段终态守卫。"""
        receipt = self.receipt(); receipt['new_recovery_build'] = False
        invalid = [
            ('source_commit', '5' * 40), ('source_commit_after', '5' * 40),
            ('source_status_after', ' M source.cpp'), ('status', 'RUNNING'),
            ('configuration', {'test_faults': True}), ('historical_artifacts_recreated', True),
            ('dependencies_unchanged_during_build', False), ('engine', {'sha256': '9' * 64}),
            ('artifacts', []), ('artifacts', [{'path': '/fixture/libhanmo_world_bridge.so', 'sha256': '9' * 64}]),
            ('completed_at', '2999-01-01T00:00:00+00:00'), ('stages', receipt['stages'][:-1])]
        for key, value in invalid:
            variant = copy.deepcopy(receipt); variant[key] = value
            with self.subTest(key=key, value=value), self.assertRaisesRegex(RuntimeError, 'BuildReceipt'):
                resident._validate_build_receipt(variant, '2' * 40, '3' * 64, '1' * 64)
        for index in range(len(receipt['stages'])):
            for field, value in [('exit_code', 1), ('exit_code', None), ('completed_at', '')]:
                variant = copy.deepcopy(receipt); variant['stages'][index][field] = value
                with self.subTest(stage=index, field=field), self.assertRaisesRegex(RuntimeError, 'BuildReceipt'):
                    resident._validate_build_receipt(variant, '2' * 40, '3' * 64, '1' * 64)

    def test_native_identity_accepts_later_head_only_for_equal_production(self):
        """UI后继可复用原库，任何生产输入变化须换实际回执和SO。"""
        library = self.root / 'libhanmo_world_bridge.so'
        library.write_bytes(b'\x7fELF\x02\x01' + b'\0' * 10 + b'\x03\x00\x3e\x00')
        config = copy.deepcopy(self.config); config['native']['library_sha256'] = resident.sha256_file(library)
        receipt = self.receipt(); receipt['artifacts'][0]['sha256'] = config['native']['library_sha256']
        receipt['bridge_identity'] = {'dependency_lock_sha256': resident._sha(b'lock'), 'resident_core_source_sha256': '6' * 64}
        path = self.root / 'receipt.json'; path.write_text(json.dumps(receipt))
        symbol_result = type('Result', (), {'returncode': 0, 'stdout': 'hanmo_world_library_init T 0 1\n', 'stderr': ''})()
        with patch.object(resident.subprocess, 'run', return_value=symbol_result), patch.object(resident, '_blob', return_value=b'lock'), patch.object(resident, '_entries', return_value={'native/source.cpp': 'a' * 40}):
            result = resident._native_identity(self.root, '7' * 40, library, path, config)
            self.assertEqual(result['source_commit'], '2' * 40)
        with patch.object(resident.subprocess, 'run', return_value=symbol_result), patch.object(resident, '_entries', side_effect=[{'source': 'old'}, {'source': 'new'}]):
            with self.assertRaisesRegex(RuntimeError, 'NativeSourceMismatch'):
                resident._native_identity(self.root, '7' * 40, library, path, config)

    def test_missing_template_fails_before_export(self):
        """缺实际固定模板必须明确失败，不能只凭编辑器已安装声称可导出。"""
        engine = self.root / 'godot'; engine.write_bytes(b'editor-fixture')
        config = copy.deepcopy(self.config); config['engine']['sha256'] = resident.sha256_file(engine)
        toolchain = {'template_version': '4.7.2.stable', 'archives': {'templates': {'entries': {
            'templates/' + name: {'bytes': 1, 'sha256': '1' * 64} for name in ['linux_debug.x86_64', 'version.txt', 'icudt_godot.dat']}}}}
        with patch.dict(os.environ, {'XDG_DATA_HOME': str(self.root / 'xdg')}), patch.object(build, 'run_checked', return_value=config['engine']['version']), patch.object(resident, '_clean_source', return_value={'commit': '7' * 40}), patch.object(resident, '_blob', return_value=json.dumps(toolchain).encode()):
            with self.assertRaisesRegex(RuntimeError, 'ExportTemplateMissingOrChanged'):
                resident._export_prerequisites(self.root, str(engine), config)

    def test_package_preserves_permissions_and_rejects_overwrite(self):
        """已闭合目录沿现有打包器生成ZIP，重试不能覆盖首个交付原件。"""
        folder, _ = self.package()
        archive = resident.package_profile(folder)
        with zipfile.ZipFile(archive) as output:
            self.assertIsNone(output.testzip())
            self.assertTrue(output.getinfo('delivery/start.sh').external_attr >> 16 & 0o111)
        first = archive.read_bytes()
        with self.assertRaises(RuntimeError):
            resident.package_profile(folder)
        self.assertEqual(archive.read_bytes(), first)

    def test_package_rejects_extra_or_changed_members(self):
        """改库或新增成员不再匹配冻结清单，ZIP不得产生。"""
        folder, _ = self.package()
        (folder / 'native.so').write_bytes(b'changed')
        with self.assertRaisesRegex(RuntimeError, 'PackageChanged'):
            resident.package_profile(folder)
        self.assertFalse(folder.with_suffix('.zip').exists())

    def test_package_rejects_private_bank_even_when_listed(self):
        """上层清单不能绕过既有打包器对私人存档的禁止。"""
        folder, manifest = self.package()
        save = folder / 'private.hmsave'; save.write_bytes(b'private')
        manifest['files'][save.name] = resident.sha256_file(save)
        (folder / 'BUILD.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(RuntimeError, 'private save'):
            resident.package_profile(folder)

    def test_package_rejects_links_and_nonexecutable_launcher(self):
        """目录链接或丢失启动权限必须在打包前暴露。"""
        folder, _ = self.package()
        (folder / 'start.sh').chmod(0o644)
        with self.assertRaisesRegex(RuntimeError, 'ExecutablePermission'):
            resident.package_profile(folder)
        (folder / 'start.sh').chmod(0o755)
        (folder / 'linked').symlink_to(folder / 'native.so')
        with self.assertRaises(RuntimeError):
            resident.package_profile(folder)

    def test_build_routes_resident_stage_and_rejects_partial_inputs(self):
        """正常build入口传递真实回执与固定compose代；缺关键输入保持非零。"""
        with patch.object(resident, 'prepare_profile', return_value={'game': 'stage/game', 'manifest_path': 'stage/manifest'}) as prepare:
            code = build.main(['stage', '--profile', 'native-resident', '--output', 'stage', '--native-library', 'native.so', '--native-build-receipt', 'receipt.json', '--content-set', 'compose'])
            self.assertEqual(code, 0)
            self.assertEqual(prepare.call_args.kwargs['native_build_receipt'], Path('receipt.json'))
            self.assertEqual(prepare.call_args.args[3], [Path('compose')])
        self.assertEqual(build.main(['stage', '--profile', 'native-resident', '--output', 'stage']), 2)

    def test_release_package_does_not_run_engine_or_old_suite(self):
        """package只验证已导出目录，不触发旧规则矩阵或重新启动引擎。"""
        with patch.object(resident, 'package_profile', return_value=Path('delivery.zip')) as package, patch.object(build, 'find_engine', side_effect=AssertionError('Engine must not run')):
            self.assertEqual(release.main(['package', '--profile', 'native-resident', '--output', 'delivery']), 0)
            package.assert_called_once_with(Path('delivery'), None)


if __name__ == '__main__':
    unittest.main()
