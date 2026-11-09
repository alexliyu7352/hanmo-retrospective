"""生育作者只描述原人物意愿和有限资源，不预造儿童、同意或后天经验。"""
import json
import unittest
from pathlib import Path
from content.family_birth import check_family_birth


class FamilyBirthAuthorTest(unittest.TestCase):
    """沿真实作者数据覆盖身份、亲代、资源与独立意愿边界。"""
    def setUp(self):
        """复用未生成任何出生事实的原作者包。"""
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/family_birth_1_0/content.json').read_text())
        self.scenario = self.data['scenarios'][0]
        self.lineage = self.scenario['family_lineage']
        self.arrangement = next(iter(self.scenario['family_birth']['arrangements'].values()))

    def codes(self, index=None):
        """返回明确作者诊断，不替代原生正常办理。"""
        return {issue[0] for issue in check_family_birth(self.data, index or {})}

    def test_explicit_personal_birth_and_care(self):
        """有限条款有效且新生儿原型不成为已有世界人物。"""
        self.assertEqual(self.codes(), set())
        self.assertNotIn(self.arrangement['terms']['child_prototype_id'], self.scenario['enabled']['actors'])

    def test_seeded_pregnancy_rejected(self):
        """不能注入已孕状态绕过本人接受。"""
        self.scenario['family_birth']['pregnant'] = True
        self.assertIn('BIRTH_FIELDS', self.codes())

    def test_same_person_rejected(self):
        """双方均须真实且为不同人物。"""
        self.arrangement['speaker_actor_id'] = self.arrangement['listener_actor_id']
        self.assertIn('BIRTH_PERSONS', self.codes())

    def test_unrelated_private_bag_rejected(self):
        """不能拿另一人的私产当作本人的有限资助。"""
        self.arrangement['terms']['source_inventory_id'] = 'bag:someone_else'
        self.assertIn('BIRTH_PERSONAL_CARE', self.codes())

    def test_bad_reference_is_diagnostic(self):
        """坏引用须给诊断，不能触发Python异常。"""
        self.arrangement['terms']['caregiver_actor_id'] = {}
        self.assertIn('BIRTH_TERM_REFERENCE', self.codes())

    def test_unknown_parent_remains_unknown(self):
        """空亲代不变成零档或隐含另一位亲属。"""
        self.assertEqual(self.lineage['original_innate_levels'], {})
        self.assertTrue(all(not origin['parent_actor_ids'] for origin in self.lineage['origins'].values()))
        self.assertEqual(self.codes(), set())

    def test_lineage_cycle_rejected(self):
        """已知亲代环不能作为出生资格来源。"""
        first, second = self.lineage['origins']
        self.lineage['origins'][first]['parent_actor_ids'] = [second]
        self.lineage['origins'][second]['parent_actor_ids'] = [first]
        self.assertIn('LINEAGE_CYCLE', self.codes())

    def test_endowment_bounded(self):
        """无失踪概率和超出上限的先天学习赠益。"""
        self.lineage['endowment']['unweighted'] = [10000, 1, 0, 0]
        next(iter(self.lineage['endowment']['families'].values()))['learning_bonus_bp'][-1] = 1501
        self.assertTrue({'INNATE_DISTRIBUTION', 'INNATE_BONUS'} <= self.codes())

    def test_prototype_cannot_inherit_learned_skill(self):
        """原型的后天经验不能通过出生复制。"""
        prototype = next(actor for actor in self.data['actors'] if actor['id'] == self.arrangement['terms']['child_prototype_id'])
        prototype['skills']['medicine'] = 1
        self.assertIn('BIRTH_PROTOTYPE', self.codes({'actors': self.data['actors']}))


if __name__ == '__main__':
    unittest.main()
