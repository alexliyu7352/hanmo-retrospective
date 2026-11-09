"""婚议作者必要门；正常compose与World登记由完整共享候选另行验证。"""
import json
import unittest
from pathlib import Path
from content.family_marriage import check_family_marriage


class FamilyMarriageAuthorTest(unittest.TestCase):
    """原人物与独立意愿不由作者直接写入已结婚、怀孕或资产转移初态。"""
    def setUp(self):
        """复用真实作者包，保持未预设任何办理事实。"""
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/family_marriage_1_0/content.json').read_text())

    def test_explicit_origin_and_personal_statement(self):
        """新策略本域静态结构齐全。"""
        self.assertEqual(check_family_marriage(self.data, {}), [])

    def test_seeded_relationship_rejected(self):
        """不能用额外seed把尚未双方同意的关系变成既成婚姻。"""
        self.data['scenarios'][0]['family_marriage']['preaccepted'] = True
        self.assertTrue(check_family_marriage(self.data, {}))

    def test_one_sided_original_marriage_rejected(self):
        """一人称已婚不能把另一人的未知处境改写。"""
        origins = self.data['scenarios'][0]['family_marriage']['origins']
        first, second = origins
        origins[first].update(status='married', spouse_actor_id=second)
        self.assertTrue(check_family_marriage(self.data, {}))

    def test_gift_authority_not_implicit(self):
        """本人婚议不暗授私产赠与。"""
        statements = self.data['scenarios'][0]['family_marriage']['statements']
        next(iter(statements.values()))['gift_money'] = 20
        self.assertTrue(check_family_marriage(self.data, {}))


if __name__ == '__main__':
    unittest.main()
