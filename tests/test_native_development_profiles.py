"""只核已审native开发profile选择和唯一build字典配置，不创建资源工程或运行游戏。"""
from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import native_growth_profile as profile

ROOT = Path(__file__).resolve().parents[1]
BUILD = {'profile': 'native_travel_schedule', 'source_commit': '62654abb3238de1f4ad23037dbd250734c399608',
         'source_sha256': 'b55e1083a866e4193c522b2ba8d0e165f19badf248eacaa6b117e8973542811f',
         'core_source_sha256': '186418319a643aaf4cd1c9d30df272d25fed5b4c7860ca9a1150e3386d6ced91',
         'api_sha256': 'd0e4c08c03b165156dabe6bfb6a906baf0069189f62035341230a246c86d6986',
         'dependency_lock_sha256': 'ca0e5e4df4843d2c04b0358ed9874e13a45bb7c65eac531083d170615ed72c0d'}


class NativeDevelopmentProfiles(unittest.TestCase):
    """使用真实工程配置与固定已审build值，锁住新旧profile和有限配置语义。"""

    def setUp(self):
        """每例仅准备内存配置；不读写两个冻结运行目录。"""
        self.raw = (ROOT / 'game/project.godot').read_bytes()
        self.storage = {'profile': 'native-growth-local-fixture', 'custom_user_dir_name': 'hanmo-local-fixture'}
        self.commit = 'a' * 40

    def travel_configuration(self):
        """走产品配置器的显式旅行分支，测试不复写其生成算法。"""
        return profile._project_config(self.raw, self.storage, self.commit, profile='native-travel', expected_build=BUILD)

    def test_old_default_configuration_is_unchanged(self):
        """旧调用仍只追加growth开关和原源码字段，不能悄悄打开旅行。"""
        result = profile._project_config(self.raw, self.storage, self.commit)
        expected = self.raw.decode().replace('config/storage_profile="player-alpha"', 'config/storage_profile="native-growth-local-fixture"')
        expected = expected.replace('config/custom_user_dir_name="hanmo-f05"', 'config/custom_user_dir_name="hanmo-local-fixture"')
        expected += '\n[development]\n\nnative_growth_action_enabled=true\nnative_projection_source_commit="' + self.commit + '"\n'
        self.assertEqual(result, expected.encode())
        self.assertNotIn(b'native_travel_schedule_enabled', result)

    def test_profiles_pin_distinct_reviewed_artifacts(self):
        """profile只能选固定库与固定审查来源，不能把新库冒称旧growth库。"""
        old = profile._profile_spec('native-growth'); new = profile._profile_spec('native-travel')
        self.assertEqual(old['library_sha256'], '991c5e09d20b1deb07b15741d839c32259ee888a1af7411d4fe342f9a65546f2')
        self.assertEqual(new['library_sha256'], '65ada2192d00429b0edd77b0b16038c9247a6fff06d0828a2573d967c2f60fd1')
        self.assertEqual(new['review'], 'verification/native-travel-schedule-20261004.json')
        self.assertNotEqual(old['manifest'], new['manifest'])
        with self.assertRaisesRegex(RuntimeError, 'UnknownProfile'):
            profile._profile_spec('arbitrary-native')

    def test_travel_enables_only_its_provider_and_exact_build(self):
        """新工程关闭成长读取，只把真实六字段build交原Main配置入口。"""
        parsed = profile._project_semantics(self.travel_configuration())['development']
        self.assertEqual(parsed['native_growth_action_enabled'], 'false')
        self.assertEqual(parsed['native_travel_schedule_enabled'], 'true')
        self.assertEqual(parsed['native_travel_schedule_expected_build'], BUILD)

    def test_only_build_dictionary_allows_gui_multiline_and_reordering(self):
        """Godot正常字典重排不变身份，其它多行Variant仍明确拒绝。"""
        raw = self.travel_configuration()
        canonical = profile._project_semantics(raw)
        single = json.dumps(BUILD, ensure_ascii=False, sort_keys=True, separators=(',', ':'))
        multiline = json.dumps(dict(reversed(list(BUILD.items()))), indent=0)
        self.assertIn(single.encode(), raw)
        changed = raw.replace(single.encode(), multiline.encode())
        self.assertEqual(profile._project_semantics(changed), canonical)
        with self.assertRaisesRegex(RuntimeError, 'ConfigurationSyntax'):
            profile._project_semantics(raw + b'other={\n"x":"y"\n}\n')

    def test_build_dictionary_rejects_duplicate_extra_missing_and_bad_values(self):
        """唯一键也不能接收嵌套值、假类型、重复或漂移的字段集合。"""
        raw = self.travel_configuration()
        single = json.dumps(BUILD, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()
        cases = [b'{"profile":"native_travel_schedule",' + single[1:],
                 json.dumps({**BUILD, 'extra': 'x'}).encode(),
                 json.dumps({key: value for key, value in BUILD.items() if key != 'api_sha256'}).encode()]
        for value in [None, 1, False, [], {}, '']:
            cases.append(json.dumps({**BUILD, 'source_commit': value}).encode())
        for value in cases:
            with self.subTest(value=value):
                with self.assertRaisesRegex(RuntimeError, 'ConfigurationSyntax'):
                    profile._project_semantics(raw.replace(single, value))
        changed = raw.replace(BUILD['source_sha256'].encode(), b'f' * 64)
        self.assertNotEqual(profile._project_semantics(changed), profile._project_semantics(raw))

    def test_other_configuration_values_remain_text_exact(self):
        """原开关/存储值的文本变化不能被新字典语义吞掉。"""
        raw = self.travel_configuration()
        changed = raw.replace(b'native_travel_schedule_enabled=true', b'native_travel_schedule_enabled= true')
        self.assertNotEqual(profile._project_semantics(raw), profile._project_semantics(changed))

    def test_cross_profile_libraries_fail_before_source_lookup(self):
        """仅测试首道库摘要准入，交叉已审库也不能靠别的profile获得接受。"""
        with tempfile.TemporaryDirectory(prefix='native-profile-library-') as directory:
            library = Path(directory) / 'input.so'; library.write_bytes(b'not loaded by this test')
            for selected, other in [('native-growth', 'native-travel'), ('native-travel', 'native-growth')]:
                with self.subTest(profile=selected), patch.object(profile, 'sha256_file', return_value=profile._profile_spec(other)['library_sha256']), patch.object(profile, '_blob', side_effect=AssertionError('must reject before provenance')):
                    with self.assertRaisesRegex(RuntimeError, 'UnreviewedNativeLibrary'):
                        profile._native_identity(ROOT, self.commit, library, profile=selected)

    def test_manifest_mode_and_ambiguity_are_rejected(self):
        """只识别唯一已知manifest，显式CLI mode不能误消费其它工程。"""
        with tempfile.TemporaryDirectory(prefix='native-profile-selection-') as directory:
            output = Path(directory)
            for selected in ['native-growth', 'native-travel']:
                path = output / profile._profile_spec(selected)['manifest']; path.write_text('{}')
                self.assertEqual(profile._manifest_profile(output, None), selected)
                self.assertEqual(profile._manifest_profile(output, selected), selected)
                other = 'native-growth' if selected == 'native-travel' else 'native-travel'
                with self.assertRaisesRegex(RuntimeError, 'ProfileManifestMismatch'):
                    profile._manifest_profile(output, other)
                (output / profile._profile_spec(other)['manifest']).write_text('{}')
                with self.assertRaisesRegex(RuntimeError, 'ProfileManifestMismatch'):
                    profile._manifest_profile(output, None)
                path.unlink(); (output / profile._profile_spec(other)['manifest']).unlink()

    def test_each_profile_selects_only_its_representative(self):
        """有限Git清单替身仅核选择规则，不能把旧外部native套件混入活动工程。"""
        names = ['game/main.tscn', 'game/project.godot', 'game/assets/actual.png',
                 'game/tests/test_native_growth_profile.gd', 'game/tests/test_native_travel_profile.gd',
                 'game/tests/test_native_travel_profile.gd.uid', 'game/tests/test_native_inventory_bridge.gd']
        tree = b''.join(('100644 blob ' + '0' * 40 + '\t' + name + '\0').encode() for name in names)
        with patch.object(profile, '_git', return_value=tree):
            for selected in ['native-growth', 'native-travel']:
                entries = profile._source_entries(ROOT, self.commit, profile=selected)
                expected = profile._profile_spec(selected)['representative_test']
                self.assertIn(expected, entries); self.assertIn('game/assets/actual.png', entries)
                self.assertTrue(all(name in {expected, expected + '.uid'} for name in entries if name.startswith('game/tests/')))

    def test_storage_is_separate_between_modes(self):
        """相同测试路径/commit/nonce仍不让两种profile使用同一个存储目录。"""
        first = profile._storage(Path('/tmp/source-only'), self.commit, 'b' * 32)
        second = profile._storage(Path('/tmp/source-only'), self.commit, 'b' * 32, profile='native-travel')
        self.assertTrue(first['profile'].startswith('native-growth-'))
        self.assertTrue(second['profile'].startswith('native-travel-'))
        self.assertNotEqual(first['custom_user_dir_name'], second['custom_user_dir_name'])


if __name__ == '__main__':
    unittest.main()
