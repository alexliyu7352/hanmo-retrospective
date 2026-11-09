"""弟子条件控制作者的窄来源门；不将纯数据通过当自然死亡或完整游戏通过。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_disciple_legacy import check_family_disciple_legacy


class DiscipleAuthorTest(unittest.TestCase):
    """两种本人同意各有原节点，关系不能偷渡控制或财物。"""
    def setUp(self):
        """复用小作者原件，不创建运行存档。"""
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/disciple_legacy_1_0/content.json').read_text())

    def test_original_two_decisions(self):
        """正常作者只声明可达本人节点，初态没有关系或许可。"""
        self.assertEqual(check_family_disciple_legacy(self.data, {}), [])
        self.assertEqual(self.data['actors'], [])
        self.assertNotIn('family_disciple_legacy', self.data['scenarios'][0]['initial'])

    def test_selected_original_request(self):
        """未选原许可和缺完整师承能力明确拒绝。"""
        for field in ('family_legacy', 'family_apprenticeship'):
            data = copy.deepcopy(self.data)
            data['scenarios'][0].pop(field)
            self.assertTrue(check_family_disciple_legacy(data, {}))

    def test_no_assets_or_seed(self):
        """额外权利和自动许可字段不能藏在策略里。"""
        for name in ('assets', 'accepted', 'initial_death'):
            data = copy.deepcopy(self.data)
            data['scenarios'][0]['family_disciple_legacy'][name] = True
            self.assertTrue(check_family_disciple_legacy(data, {}))

    def test_exact_skill_and_parties(self):
        """另一技艺或另一师生不能为这份许可提供依据。"""
        self.data['scenarios'][0]['family_disciple_legacy']['requests']['request.disciple']['skill_id'] = 'commerce'
        self.assertTrue(check_family_disciple_legacy(self.data, {}))

    def test_npc_personal_say(self):
        """玩家选项和组织公职都不能代候选本人签字。"""
        self.data['dialogues'][0]['nodes'][2]['kind'] = 'choice'
        self.assertTrue(check_family_disciple_legacy(self.data, {}))
        self.setUp()
        self.data['roles'][0]['organization_id'] = 'society::org.city'
        self.assertTrue(check_family_disciple_legacy(self.data, {}))


if __name__ == '__main__':
    unittest.main()
