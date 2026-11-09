"""死亡策略作者准入的必要门；不以目录通过代替真实战损和接续运行。"""
import unittest
from tools.content.family_life import PROFILE, check_family_life


class FamilyLifePolicyTests(unittest.TestCase):
    """独立核已实现策略、明确能力和原生世界约束。"""
    def test_explicit_policy(self):
        """正确显式情景可入目录，旧情景无需增加能力。"""
        self.assertEqual([], check_family_life({'scenarios': []}, {}))
        self.assertEqual([], check_family_life({'required_capabilities': ['family_life_v1'],
            'scenarios': [{'family_life': dict(PROFILE), 'dynamic_world': {}}]}, {}))

    def test_unsupported_automatic_succession(self):
        """作者不能用启用死亡记录来宣告自动授权继承。"""
        policy = dict(PROFILE, player_death='automatic_heir')
        issues = check_family_life({'required_capabilities': ['family_life_v1'],
            'scenarios': [{'family_life': policy, 'dynamic_world': {}}]}, {})
        self.assertEqual(['FAMILY_LIFE_PROFILE'], [item[0] for item in issues])

    def test_required_runtime_and_capability(self):
        """缺原生阶段或能力声明均拒，旧核心不得忽略生命段。"""
        issues = check_family_life({'scenarios': [{'family_life': dict(PROFILE)}]}, {})
        self.assertEqual({'FAMILY_LIFE_NATIVE', 'FAMILY_LIFE_CAPABILITY'}, {item[0] for item in issues})


if __name__ == '__main__':
    unittest.main()
