"""仅核作者选择和旧剧本隔离，不把结构门说成实际课程完成。"""
import copy
import unittest
from tools.content.family_innate_learning import check_family_innate_learning


class InnateLearningAuthorTest(unittest.TestCase):
    def setUp(self):
        """最小合法选择不包含经验、同意或出生回执种子。"""
        self.scenario = {'family_innate_learning': {'version': 1, 'profile': 'lineage_per_lesson_v1'}, 'dynamic_world': {}, 'family_birth': {}, 'family_lineage': {}, 'skill_profile': 'full_sixteen_v1', 'enabled': {'roles': ['role.course']}}
        self.pack = {'required_capabilities': ['family_innate_learning_v1', 'cultivation', 'family_birth_v1', 'native_dynamic_world'], 'scenarios': [self.scenario], 'roles': [{'id': 'role.course', 'skill_courses': [{'settlement_policy': 'innate_per_lesson_v1'}]}]}
    def codes(self):
        """回传具名失败而不替作者补默认能力。"""
        return {x[0] for x in check_family_innate_learning(self.pack, {})}
    def test_valid(self):
        self.assertEqual(self.codes(), set())
    def test_unknown_fields_and_version(self):
        for key, value in [('version', 2), ('version', True), ('profile', 'guess'), ('initial_xp', 100)]:
            altered = copy.deepcopy(self.pack); altered['scenarios'][0]['family_innate_learning'][key] = value
            self.assertIn('INNATE_LEARNING_FIELDS', {x[0] for x in check_family_innate_learning(altered, {})})
    def test_selected_policy_requires_profile(self):
        del self.scenario['family_innate_learning']; self.assertIn('INNATE_LEARNING_REQUIRED', self.codes())
    def test_unselected_course_does_not_enable_old_profile(self):
        del self.scenario['family_innate_learning']; self.scenario['enabled']['roles'] = []; self.assertEqual(self.codes(), set())
    def test_missing_dependencies(self):
        del self.scenario['family_lineage']; self.assertIn('INNATE_LEARNING_DEPENDENCIES', self.codes())
    def test_profile_without_new_course(self):
        self.scenario['enabled']['roles'] = []; self.assertIn('INNATE_LEARNING_COURSE', self.codes())


if __name__ == '__main__': unittest.main()
