"""师承独立作者窄门；未完成共享能力登记前不冒正规 compose 或 World 通过。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_apprenticeship import check_family_apprenticeship


class ApprenticeshipAuthorTest(unittest.TestCase):
    """原人物、本人私人say与原课程保持独立，不允许作者直接预接受。"""
    def setUp(self):
        """加载本系统新作者，不复制整 stage 或生成运行档。"""
        root = Path(__file__).resolve().parents[2]
        self.data = json.loads((root / 'content/packs/authored/adult_apprenticeship_1_0/content.json').read_text())

    def test_explicit_original_people(self):
        """正常字段仅描述意愿；真正关系只能经原生双边决定产生。"""
        self.assertEqual(check_family_apprenticeship(self.data, {}), [])
        self.assertEqual(self.data['actors'], [])
        self.assertEqual(self.data['roles'][0]['skill_courses'][0]['mode'], 'teacher')
        self.assertEqual(self.data['roles'][0]['skill_courses'][0]['settlement_policy'], 'per_lesson_v1')

    def test_no_seeded_relation_or_control(self):
        """种子关系、自动经验及继任许可不能藏在成人来源配置里。"""
        for key in ('accepted_bonds', 'initial_xp', 'succession_consent'):
            altered = copy.deepcopy(self.data)
            altered['scenarios'][0]['family_apprenticeship'][key] = True
            self.assertTrue(check_family_apprenticeship(altered, {}))

    def test_private_speaker_only(self):
        """公职或另一个人持有的岗位不得冒先生本人意愿。"""
        self.data['roles'][0]['organization_id'] = 'society::org.city'
        self.assertTrue(check_family_apprenticeship(self.data, {}))
        self.data['roles'][0]['organization_id'] = None
        self.data['roles'][0]['initial_actor'] = 'society::actor.city_steward'
        self.assertTrue(check_family_apprenticeship(self.data, {}))

    def test_actual_say_and_unambiguous_terms(self):
        """听者选项不是另一方本人发言，同一节点也不能同时表达两份师承。"""
        statements = self.data['scenarios'][0]['family_apprenticeship']['statements']
        first = next(iter(statements.values()))
        first['node_id'] = 'ask'
        self.assertTrue(check_family_apprenticeship(self.data, {}))
        first['node_id'] = 'offer'
        second = copy.deepcopy(first)
        second.update(id='statement.second', listener_actor_id='society::actor.apprentice')
        statements[second['id']] = second
        self.assertTrue(check_family_apprenticeship(self.data, {}))

    def test_provider_is_actual_speaker(self):
        """玩家自己的say、另一具名说话人或缺省不能代表候选本人愿意。"""
        offer = next(node for node in self.data['dialogues'][0]['nodes'] if node['id'] == 'offer')
        for speaker in ('$player', 'society::actor.apprentice', ''):
            offer['speaker'] = speaker
            self.assertTrue(check_family_apprenticeship(self.data, {}))
        offer['speaker'] = '$provider'
        self.assertEqual(check_family_apprenticeship(self.data, {}), [])

    def test_no_gift_scope(self):
        """个人关系声明不得新增免费物资或控制附权。"""
        statement = next(iter(self.data['scenarios'][0]['family_apprenticeship']['statements'].values()))
        statement['inherit_assets'] = True
        self.assertTrue(check_family_apprenticeship(self.data, {}))

    def test_original_extended_skill_selection(self):
        """额外六技艺只沿原情景的十六技艺选择，不增一套能力名或自行枚举。"""
        scenario = self.data['scenarios'][0]
        statement = next(iter(scenario['family_apprenticeship']['statements'].values()))
        statement['skill_id'] = 'naval'
        scenario.pop('skill_profile', None)
        self.assertTrue(check_family_apprenticeship(self.data, {}))
        scenario['skill_profile'] = 'full_sixteen_v1'
        self.assertEqual(check_family_apprenticeship(self.data, {}), [])


if __name__ == '__main__':
    unittest.main()
