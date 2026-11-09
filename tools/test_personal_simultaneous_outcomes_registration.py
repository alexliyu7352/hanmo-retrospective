"""同一显式目录的三入口与冷门源码接线，不代World银行或完整二进制回归。"""
from pathlib import Path
import os
import unittest

ROOT = Path(os.environ.get('HANMO_SOURCE_ROOT', Path(__file__).resolve().parents[1]))
OWNED = Path(os.environ.get('HANMO_OWNED_SOURCE_ROOT', ROOT))


def source(path):
    """候选只覆盖具名共享叶，其余从同一owner固定后像读取。"""
    candidate = ROOT / path
    return (candidate if candidate.is_file() else OWNED / path).read_text(encoding='utf8')


class PersonalSimultaneousRegistrationTest(unittest.TestCase):
    """未知版本/旧格式的行为反例见同源原生小门，这里只防遗漏接线。"""
    def test_three_producers_use_same_last_marker(self):
        """三个真实目录生产者都在既有尾后追加相同模块。"""
        paths = ['native/world/src/resident/kernel/world_codec.cpp', 'native/world/src/resident/kernel/native_initial_state.cpp', 'game/ui/native_resident_backend.gd']
        for path in paths:
            text = source(path)
            previous = text.index('emplace_back("family_collateral_legacy","1")') if path.endswith('.cpp') else text.index('definition.has("family_collateral_legacy")')
            following = text.index('emplace_back(personal_simultaneous_outcomes_module,"1")', previous) if path.endswith('.cpp') else text.index('definition.has("personal_simultaneous_outcomes")', previous)
            self.assertGreater(following, previous)

    def test_parallel_agency_marker_keeps_same_relative_order(self):
        """中央合流后三个生产者均先追加代理再追加本策略，不能只各自位于旧尾之后。"""
        for path in ['native/world/src/resident/kernel/world_codec.cpp', 'native/world/src/resident/kernel/native_initial_state.cpp', 'game/ui/native_resident_backend.gd']:
            text = source(path)
            if path.endswith('.cpp'):
                agency = 'emplace_back(political_governance_agency_domain,"1")'
                personal = 'emplace_back(personal_simultaneous_outcomes_module,"1")'
            else:
                agency = 'expected["module_versions"].append({"name":"political_governance_agencies","version":"1"})'
                personal = 'expected["module_versions"].append({"name":"personal_simultaneous_outcomes","version":'
            self.assertEqual(text.count(agency), 1)
            self.assertEqual(text.count(personal), 1)
            self.assertLess(text.index(agency), text.index(personal))

    def test_actual_catalog_and_world_bound(self):
        """原catalog decode与完整冷验必须都消费实际目标集合。"""
        codec = source('native/world/src/resident/kernel/world_codec.cpp')
        self.assertIn('configure_personal_simultaneous_outcomes(data,scenario,personal.value)', codec)
        self.assertIn('validate_personal_simultaneous_outcome_modules(identity.catalog.module_versions,has(personal_simultaneous_outcomes_module))', codec)
        world = source('native/world/src/resident/kernel/world_bundle.cpp')
        self.assertLess(world.index('validate_personal_simultaneous_outcome_modules('), world.index('validate_personal(common,personal,'))

    def test_no_new_state_section_or_personal_tail(self):
        """新策略只有目录marker，原Personal访客不多空字段或新section。"""
        codec = source('native/world/src/resident/kernel/world_codec.cpp')
        self.assertNotIn('domains.push_back({personal_simultaneous_outcomes_module', codec)
        self.assertNotIn('section(personal_simultaneous_outcomes_module', codec)
        old = source('native/world/src/resident/domains/personal/personal_codec.cpp')
        self.assertNotIn('simultaneous', old)

    def test_same_hot_cold_classifier(self):
        """原热轮和原冷结果继续调用同一personal_outcome，保护失败排在新分支前。"""
        round_source = source('native/world/src/resident/domains/personal/personal_round.cpp')
        new = round_source.index('if(personal_mutual_defeat_draw(b,c))')
        self.assertLess(round_source.index('return "protected_lost"'), new)
        self.assertLess(new, round_source.index('if(controller.hp<=0)return "defeat"'))
        self.assertIn('auto expected=personal_outcome(b,c);', source('native/world/src/resident/domains/personal/personal_session.cpp'))

    def test_identity_and_target_registration(self):
        """唯一构建清单含target与身份中的新CPP及头，不把可编译误作可追溯。"""
        cmake = source('native/world/resident.cmake')
        self.assertEqual(2, cmake.count('    src/resident/domains/personal/personal_simultaneous_outcomes.cpp\n'))
        self.assertEqual(1, cmake.count('    src/resident/domains/personal/personal_simultaneous_outcomes.hpp\n'))
        self.assertIn('tests/resident/personal_simultaneous_outcomes_test.cpp', cmake)


if __name__ == '__main__':
    unittest.main()
