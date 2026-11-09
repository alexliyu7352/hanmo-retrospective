"""作者可表达真实雇用，但不能注入同意、工资或第三人资产。"""
import unittest
from tools.content.estate_employment_links import check_estate_employment


class EstateEmploymentAuthorTests(unittest.TestCase):
    """局部覆盖新政策边界，原通用庄园/人物/对白schema继续负责基础形状。"""
    def source(self):
        """建立最小真正原表引用，而非测试专用跳过许可。"""
        policy = dict(id='job', estate_id='estate', job='labor', rate_profile='wage', offer_dialogue_id='dialogue', offer_node_id='node', worker_dialogue_id='dialogue', worker_node_id='node', service_ticks=4, offer_valid_ticks=2, provider_offer_dialogue_id='', provider_offer_node_id='', provider_reply_dialogue_id='', provider_reply_node_id='', provider_reply_decision='')
        return {'required_capabilities': ['estate_employment_v1', 'native_dynamic_world', 'estates', 'economy', 'dialogue', 'multi_actor'], 'estates': [{'id': 'estate', 'owner_actor_id': 'owner', 'wallet_id': 'fund', 'inventory_id': 'grain'}], 'dialogues': [{'id': 'dialogue', 'nodes': [{'id': 'node', 'kind': 'say', 'speaker': '$provider'}]}], 'scenarios': [{'enabled': {'estates': ['estate']}, 'initial': {'wallets': [{'id': 'fund', 'owner': {'kind': 'actor', 'id': 'owner'}}], 'inventories': [{'id': 'grain', 'owner': {'kind': 'actor', 'id': 'owner'}}]}, 'dynamic_world': {'profile': 'local_society_v1', 'local_economy': {'rates': {'wage': {'numerator': 3, 'denominator': 2, 'per_capita': False}}}, 'estate_employment': {'version': 1, 'policies': {'job': policy}}}}]}

    def test_original_private_policy(self):
        """原本人私产和有限分数率可正常进入作者管线。"""
        self.assertEqual(check_estate_employment(self.source(), {}), [])

    def test_no_preaccepted_contract_or_foreign_cash(self):
        """未知合同种子、公钱包、人口率、未启用庄园分别拒绝。"""
        for kind in range(4):
            pack = self.source(); scenario = pack['scenarios'][0]; world = scenario['dynamic_world']
            if kind == 0: world['estate_employment']['contracts'] = {}
            if kind == 1: scenario['initial']['wallets'][0]['owner'] = {'kind': 'organization', 'id': 'public'}
            if kind == 2: world['local_economy']['rates']['wage']['per_capita'] = True
            if kind == 3: scenario['enabled']['estates'] = []
            self.assertTrue(check_estate_employment(pack, {}))

    def test_provider_says_only_explicit_decision(self):
        """双向NPC来源要原provider说话，不能由玩家句子授接受。"""
        pack = self.source(); policy = pack['scenarios'][0]['dynamic_world']['estate_employment']['policies']['job']
        policy.update(provider_reply_dialogue_id='dialogue', provider_reply_node_id='node', provider_reply_decision='accept')
        self.assertEqual(check_estate_employment(pack, {}), [])
        pack['dialogues'][0]['nodes'][0]['speaker'] = '$player'
        self.assertTrue(check_estate_employment(pack, {}))

    def test_old_content_unchanged(self):
        """未显式选中的旧情景不要求雇用模块或新账。"""
        pack = self.source();del pack['scenarios'][0]['dynamic_world']['estate_employment'];pack['required_capabilities'] = []
        self.assertEqual(check_estate_employment(pack, {}), [])
