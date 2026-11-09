"""旧自愿接续的本人接受来源修复，不调整任何钱货数额和旧格式。"""
import copy
import json
import unittest
from pathlib import Path
from content.voluntary_succession import check_voluntary_succession


class VoluntarySpeakerAuthorTest(unittest.TestCase):
    """使用已登记原包作正例，单个来源字段变更作反例。"""
    def setUp(self):
        """依赖目录只填原真实选中双方及所选品类。"""
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/voluntary_succession_1_0/content.json').read_text())
        self.request = self.data['succession_consent_requests'][0]
        self.index = {'actors': {self.request[k]: {} for k in ('predecessor_actor_id', 'successor_actor_id')}, 'items': {x['item_id']: {} for x in self.request['personal']['goods']}}
        self.dialogue = next(x for x in self.data['dialogues'] if x['id'] == self.request['dialogue_id'])
        self.node = next(x for x in self.dialogue['nodes'] if x['id'] == self.request['node_id'])

    def test_original_provider(self):
        """原已登记正常作者继续通过。"""
        self.assertEqual(check_voluntary_succession(self.data, self.index), [])

    def test_exact_candidate(self):
        """实际Interaction允许确切同一人物ID，不额外限制岗位类型。"""
        self.node['speaker'] = self.request['successor_actor_id']
        next(x for x in self.data['roles'] if x['id'] == self.dialogue['context_role'])['organization_id'] = 'organization'
        self.assertEqual(check_voluntary_succession(self.data, self.index), [])

    def test_other_speaker(self):
        """原玩家、第三人或缺说话人不能代签任何数额。"""
        for speaker in ('$player', self.request['predecessor_actor_id'], 'other', ''):
            with self.subTest(speaker=speaker):
                self.node['speaker'] = speaker
                self.assertIn('SUCCESSION_CANDIDATE_SAY', [x[0] for x in check_voluntary_succession(self.data, self.index)])

    def test_player_choice(self):
        """玩家选择仅表达玩家行为，不能冒NPC接受。"""
        self.node['kind'] = 'choice'
        self.assertIn('SUCCESSION_CANDIDATE_SAY', [x[0] for x in check_voluntary_succession(self.data, self.index)])


if __name__ == '__main__':
    unittest.main()
