"""原条件许可的本人说话来源门，保原作者和旧字段，不增私人岗位限制。"""
import copy
import json
import unittest
from pathlib import Path
from content.family_legacy import check_family_legacy


class LegacySpeakerAuthorTest(unittest.TestCase):
    """既有正常包只改一个来源字段即可复现旧代签缺口。"""
    def setUp(self):
        """读取原正常作者，以原具名双方作为依赖目录身份。"""
        self.data = json.loads((Path(__file__).resolve().parents[2] / 'content/packs/authored/family_legacy_1_0/content.json').read_text())
        self.request = self.data['legacy_control_consent_requests'][0]
        self.index = {'actors': {self.request[k]: {} for k in ('predecessor_actor_id', 'successor_actor_id')}}
        self.dialogue = next(x for x in self.data['dialogues'] if x['id'] == self.request['dialogue_id'])

    def test_original_provider_says(self):
        """原正常接受与撤回均合法，不改变原数据。"""
        self.assertEqual(check_family_legacy(self.data, self.index), [])

    def test_exact_successor_says(self):
        """与原Interaction相同，确切候选ID也是本人发言。"""
        for key in ('accept_node_id', 'revoke_node_id'):
            next(x for x in self.dialogue['nodes'] if x['id'] == self.request[key])['speaker'] = self.request['successor_actor_id']
        self.assertEqual(check_family_legacy(self.data, self.index), [])

    def test_player_third_or_missing_speaker(self):
        """只改原speaker即须拒，不能让他人台词承担候选许可。"""
        for key in ('accept_node_id', 'revoke_node_id'):
            for speaker in ('$player', self.request['predecessor_actor_id'], 'other', ''):
                with self.subTest(key=key, speaker=speaker):
                    data = copy.deepcopy(self.data)
                    talk = next(x for x in data['dialogues'] if x['id'] == self.request['dialogue_id'])
                    next(x for x in talk['nodes'] if x['id'] == self.request[key])['speaker'] = speaker
                    self.assertIn('LEGACY_CANDIDATE_SAY', [x[0] for x in check_family_legacy(data, self.index)])

    def test_choice_does_not_sign(self):
        """玩家选择节点不等于候选自己已答复。"""
        for key in ('accept_node_id', 'revoke_node_id'):
            data = copy.deepcopy(self.data)
            talk = next(x for x in data['dialogues'] if x['id'] == self.request['dialogue_id'])
            next(x for x in talk['nodes'] if x['id'] == self.request[key])['kind'] = 'choice'
            self.assertIn('LEGACY_CANDIDATE_SAY', [x[0] for x in check_family_legacy(data, self.index)])

    def test_no_new_private_role_restriction(self):
        """是否本人由实际来源核，不凭岗位具有组织归属判为他人。"""
        role = next(x for x in self.data['roles'] if x['id'] == self.dialogue['context_role'])
        role['organization_id'] = 'organization'
        self.assertEqual(check_family_legacy(self.data, self.index), [])


if __name__ == '__main__':
    unittest.main()
