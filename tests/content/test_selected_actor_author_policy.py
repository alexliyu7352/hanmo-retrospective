"""真实青川双人物作者政策的窄回归；区分保留原外貌与成人组合域，不创建世界。"""
import copy
import json
from pathlib import Path
import unittest

from tools.content.appearance_choices import check_appearance_choices
from tools.content.pack_compose import _check_presentation_references
from tools.content.pack_errors import PackError
from tools.content.validation import validate_pack

ROOT = Path(__file__).resolve().parents[2]


def candidate():
    """每个用例读取真实1.8作者源，既有192候选与完整初态不由测试替造。"""
    return json.loads((ROOT / 'content/packs/authored/local_society_1_8/content.json').read_text())


def check(data):
    """调用生产作者语义叶并提供真实人物索引，保留全部诊断码。"""
    return check_appearance_choices(data, {'actors': {row['id']: row for row in data['actors']}})


class SelectedActorAuthorPolicyTests(unittest.TestCase):
    """新政策及相邻旧政策分别验证，失败必须保持原作者输入。"""

    def test_real_pack_uses_shared_validation(self):
        """新Schema和全部现有跨表检查接受真实两人包；不能仅凭自有叶返回空列表。"""
        data = candidate()
        before = copy.deepcopy(data)
        report = validate_pack(data)
        self.assertTrue(report.ok, [(row.code, row.path) for row in report.issues])
        self.assertEqual(data, before)

    def test_preserved_actor_is_not_forced_into_composition_domain(self):
        """保留者允许旧成人年龄域和已定义独立固定图，组合者仍受18至59及192完整表约束。"""
        data = candidate()
        scenario = data['scenarios'][0]
        person = next(row for row in scenario['initial']['actors'] if row['actor_id'] == 'actor.study_swordswoman')
        person['age_years'] = 120
        person['appearance_definition']['base_features']['face_shape'] = 'existing_older_face'
        self.assertEqual(check(data), [])
        scenario.pop('player_start_descriptions')
        self.assertEqual(check(data), [])
        scenario['playable_actor_appearance_policies'][0]['policy'] = 'preserve_existing_v1'
        scenario.pop('player_appearance_choices')
        self.assertEqual(check(data), [])
        data = candidate()
        data['scenarios'][0]['playable_actor_appearance_policies'][1]['policy'] = 'fixed_actor_appearance_composition_v1'
        self.assertEqual(check(data), [])

    def test_invalid_scope_and_old_policy_reject_without_mutation(self):
        """拒绝人数、覆盖、年龄、状态和组合内容越界；旧政策不能借新字段扩权。"""
        mutations = ['one_actor', 'duplicate_actor', 'missing_policies', 'duplicate_policy', 'foreign_policy',
                     'unknown_mode', 'duplicate_description', 'dead', 'unavailable', 'child', 'over_age',
                     'composition_age', 'generated_preserve', 'missing_choice', 'duplicate_choice',
                     'unknown_hair', 'old_policy']
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                data = candidate()
                scenario = data['scenarios'][0]
                people = {row['actor_id']: row for row in scenario['initial']['actors']}
                policies = scenario['playable_actor_appearance_policies']
                if mutation == 'one_actor': scenario['playable_actor_ids'].pop()
                elif mutation == 'duplicate_actor': scenario['playable_actor_ids'][1] = scenario['playable_actor_ids'][0]
                elif mutation == 'missing_policies': scenario.pop('playable_actor_appearance_policies')
                elif mutation == 'duplicate_policy': policies[1] = copy.deepcopy(policies[0])
                elif mutation == 'foreign_policy': policies[1]['actor_id'] = 'actor.tutor'
                elif mutation == 'unknown_mode': policies[1]['policy'] = 'generated_v1'
                elif mutation == 'duplicate_description': scenario['player_start_descriptions'][1] = copy.deepcopy(scenario['player_start_descriptions'][0])
                elif mutation == 'dead': people['actor.study_swordswoman']['alive'] = False
                elif mutation == 'unavailable': people['actor.study_swordswoman']['available'] = False
                elif mutation == 'child': people['actor.study_swordswoman']['age_years'] = 17
                elif mutation == 'over_age': people['actor.study_swordswoman']['age_years'] = 121
                elif mutation == 'composition_age': people['actor.apprentice']['age_years'] = 60
                elif mutation == 'generated_preserve': people['actor.study_swordswoman']['appearance_definition']['kind'] = 'generated'
                elif mutation == 'missing_choice': scenario['player_appearance_choices'].pop()
                elif mutation == 'duplicate_choice': scenario['player_appearance_choices'][-1] = copy.deepcopy(scenario['player_appearance_choices'][0])
                elif mutation == 'unknown_hair': scenario['player_appearance_choices'][0]['base_features']['hair'] = 'unmade_hair'
                else: scenario['dynamic_world']['founder_policy'] = 'fixed_actor_appearance_composition_v1'
                before = copy.deepcopy(data)
                self.assertTrue(check(data), mutation)
                self.assertEqual(data, before)

    def test_new_policy_requires_registered_composition_resources(self):
        """新选择政策不能删去组合登记后绕过实际资源闭包门。"""
        with self.assertRaises(PackError) as caught:
            _check_presentation_references(candidate(), {})
        self.assertEqual(caught.exception.code, 'MissingPackPresentationReference')
        self.assertEqual(caught.exception.details.get('group'), 'adult_appearance_composition')


if __name__ == '__main__':
    unittest.main()
