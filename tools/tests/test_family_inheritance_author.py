"""遗产作者只启用真实来源策略，未到达的死亡/消息不能通过种子字段补成。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_inheritance import check_family_inheritance


class FamilyInheritanceAuthorTest(unittest.TestCase):
    """验证本次原声明、死亡文书与独立受领能力的最小闭包。"""
    def setUp(self):
        """复用真实本域作者输入，不临时生成另一套情景。"""
        root = Path(__file__).resolve().parents[2]
        self.data = json.loads((root / 'content/packs/authored/family_inheritance_1_0/content.json').read_text())

    def test_explicit_source_profile(self):
        """只验本域策略结构，完整正常compose由公共候选执行。"""
        self.assertEqual(check_family_inheritance(self.data, {}), [])

    def test_seeded_acceptance_rejected(self):
        """多余受领字段不能绕过本人决定。"""
        self.data['scenarios'][0]['family_inheritance']['preaccepted'] = True
        self.assertTrue(check_family_inheritance(self.data, {}))

    def test_missing_notice_rejected(self):
        """死亡事实存在不代替实收消息能力。"""
        del self.data['scenarios'][0]['task_message_notices']
        self.assertTrue(check_family_inheritance(self.data, {}))

    def test_missing_capability_rejected(self):
        """旧家庭和遗赠能力不隐式启用交割。"""
        self.data['required_capabilities'].remove('family_inheritance_v1')
        self.assertTrue(check_family_inheritance(self.data, {}))


if __name__ == '__main__':
    unittest.main()
