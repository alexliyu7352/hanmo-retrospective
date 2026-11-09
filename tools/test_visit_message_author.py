"""拜帖显式选择与可信接待来源门；不生成拜帖初态或复制作者素材。"""
from copy import deepcopy
import unittest
from content.visit_message_links import check_visit_messages


class VisitMessageAuthorTest(unittest.TestCase):
    """旧情景不被能力并集升级，新情景须三名独立真实人物。"""
    def setUp(self):
        self.policy = dict(id='visit', sender_actor_id='sender', recipient_actor_id='recipient', custodian_actor_id='host',
                           role_id='desk', dialogue_id='talk', deposit_node_id='deposit', receive_node_id='receive', withdraw_node_id='withdraw')
        self.scene = {'dynamic_world': {'profile': 'local_society_v1'}, 'visit_message_profile': 'witnessed_reception_v1',
                      'visit_messages': {'version': 1, 'policies': [self.policy]},
                      'enabled': {'actors': ['sender', 'recipient', 'host'], 'roles': ['desk'], 'dialogues': ['talk'], 'places': ['hall']}}
        self.pack = {'required_capabilities': ['visit_messages_v1', 'native_dynamic_world', 'multi_actor', 'dialogue'], 'scenarios': [self.scene]}
        self.index = {'actors': {x: {} for x in self.scene['enabled']['actors']}, 'places': {'hall': {}},
                      'roles': {'desk': {'initial_actor': 'host', 'place_id': 'hall', 'dialogue_id': 'talk'}},
                      'dialogues': {'talk': {'context_role': 'desk', 'nodes': [{'id': x, 'kind': 'say', 'speaker': '$provider'} for x in ('deposit', 'receive', 'withdraw')]}}}

    def test_valid_is_read_only(self):
        """作者检查不写初态或自动产生消息。"""
        original = deepcopy((self.pack, self.index)); self.assertEqual([], check_visit_messages(self.pack, self.index)); self.assertEqual(original, (self.pack, self.index))

    def test_profile_pair_and_version(self):
        """版本与显式选择必须同时存在，旧情景仍合法。"""
        self.scene.pop('visit_message_profile'); self.assertTrue(check_visit_messages(self.pack, self.index))
        self.scene.pop('visit_messages'); self.assertEqual([], check_visit_messages(self.pack, self.index))

    def test_three_real_participants(self):
        """收件人不得被本人或接待人替代，未启用角色不能成为后台消息来源。"""
        self.policy['recipient_actor_id'] = 'sender'; self.assertTrue(check_visit_messages(self.pack, self.index))
        self.policy['recipient_actor_id'] = 'absent'; self.assertTrue(check_visit_messages(self.pack, self.index))

    def test_actual_provider_says_three_distinct_nodes(self):
        """玩家话语、第三人或选择节点都不是接待本人原声明。"""
        node = self.index['dialogues']['talk']['nodes'][0]
        for speaker in ('$player', 'recipient'):
            node['speaker'] = speaker; self.assertTrue(check_visit_messages(self.pack, self.index))
        node.update(speaker='$provider', kind='choice'); self.assertTrue(check_visit_messages(self.pack, self.index))
        node['kind'] = 'say'; self.policy['receive_node_id'] = 'deposit'; self.assertTrue(check_visit_messages(self.pack, self.index))

    def test_no_secret_payload_or_second_source(self):
        """任意秘密正文和重复关系都不进入v1固定来访内容。"""
        self.policy['death_fact_id'] = 'casualty:4'; self.assertTrue(check_visit_messages(self.pack, self.index)); self.policy.pop('death_fact_id')
        self.scene['visit_messages']['policies'].append(deepcopy(self.policy)); self.assertTrue(check_visit_messages(self.pack, self.index))

    def test_original_desk_and_capability(self):
        """岗位/地点要真实启用，不让能力声明冒角色来源。"""
        self.pack['required_capabilities'].remove('visit_messages_v1'); self.assertTrue(check_visit_messages(self.pack, self.index))
        self.pack['required_capabilities'].append('visit_messages_v1'); self.scene['enabled']['places'] = []; self.assertTrue(check_visit_messages(self.pack, self.index))


if __name__ == '__main__': unittest.main()
