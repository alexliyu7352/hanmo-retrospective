"""有限作者坏源门，不冒运行中的死亡、实际送信、签约或归还资产。"""
import copy
import unittest
from tools.content.private_shop_liquidation_links import check_private_shop_liquidations


class PrivateShopEstateAuthorTest(unittest.TestCase):
    """只核本新profile闭包与真实双方声明、可执行的原死亡文书路径。"""
    def setUp(self):
        """所有人物/角色/文书仅为作者定义，场景没有预制运行事实。"""
        self.index = {'actors': {a: {'id': a} for a in ('owner', 'executor', 'witness')},
                      'roles': {}, 'dialogues': {}, 'offers': {'notice': {'id': 'notice',
                      'template': 'message', 'issuer': {'kind': 'actor', 'id': 'witness'},
                      'goal': {'receiver_actor': 'executor'}}}}
        for actor in ('owner', 'executor'):
            self.index['roles'][actor + '.role'] = {'id': actor + '.role', 'initial_actor': actor,
                                                  'place_id': 'market', 'dialogue_id': actor + '.dialogue'}
            self.index['dialogues'][actor + '.dialogue'] = {'id': actor + '.dialogue',
                'nodes': [{'id': 'yes', 'kind': 'say', 'speaker': '$provider'}]}
        policy = {'id': 'estate', 'premises_id': 'stall', 'proprietor_actor_id': 'owner',
                  'executor_actor_id': 'executor', 'proprietor_role_id': 'owner.role',
                  'executor_role_id': 'executor.role', 'proprietor_dialogue_id': 'owner.dialogue',
                  'executor_dialogue_id': 'executor.dialogue', 'proprietor_appointment_node_id': 'yes',
                  'executor_acceptance_node_id': 'yes'}
        self.pack = {'required_capabilities': ['private_shop_liquidations_v1', 'private_shops_v1',
            'family_life_v1', 'task_death_notices_v1', 'task_message_handoffs_v1', 'native_dynamic_world',
            'tasks.message', 'dialogue', 'multi_actor', 'task_shop_orders_v1'], 'scenarios': [{
            'task_shop_order_profile': 'private_shop_orders_v1',
            'enabled': {'actors': ['owner', 'executor', 'witness'], 'roles': ['owner.role', 'executor.role'],
                'dialogues': ['owner.dialogue', 'executor.dialogue'], 'offers': ['notice']},
            'family_life': {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'},
            'task_message_handoff_profile': 'observed_letter_delivery_v1',
            'task_message_notices': {'version': 1, 'profile': 'witnessed_death_letters_v1',
                                    'bindings': [{'offer_id': 'notice', 'decedent_actor_id': 'owner'}]},
            'dynamic_world': {'profile': 'local_society_v1', 'private_shops': {'enabled': True,
                'version': 1, 'premises': {'stall': {'place_id': 'market'}}},
                'private_shop_liquidations': {'version': 1, 'policies': {'estate': policy}}}}]}
        self.scenario = self.pack['scenarios'][0]
        self.policy = policy

    def codes(self):
        """返回精确诊断身份，不忽略未匹配的原字段路径。"""
        return {x[0] for x in check_private_shop_liquidations(self.pack, self.index)}

    def test_explicit_complete_source(self):
        self.assertEqual(self.codes(), set())

    def test_legacy_unchanged(self):
        del self.scenario['dynamic_world']['private_shop_liquidations']
        self.assertEqual(self.codes(), set())

    def test_capability_each_required(self):
        original = list(self.pack['required_capabilities'])
        for cap in original:
            self.pack['required_capabilities'] = [x for x in original if x != cap]
            self.assertIn('PRIVATE_SHOP_ESTATE_CAPABILITIES', self.codes())

    def test_bad_catalog_shapes(self):
        for value in (None, [], {'version': True, 'policies': {}}, {'version': 1, 'policies': {}, 'mandates': {}}):
            self.scenario['dynamic_world']['private_shop_liquidations'] = value
            self.assertIn('PRIVATE_SHOP_ESTATE_POLICIES', self.codes())

    def test_original_two_people_required(self):
        self.policy['executor_actor_id'] = 'owner'
        self.assertIn('PRIVATE_SHOP_ESTATE_PARTIES', self.codes())

    def test_other_provider_role_rejected(self):
        self.index['roles']['executor.role']['initial_actor'] = 'witness'
        self.assertIn('PRIVATE_SHOP_ESTATE_PERSONAL_SAY', self.codes())

    def test_player_or_choice_not_other_person_consent(self):
        node = self.index['dialogues']['executor.dialogue']['nodes'][0]
        node['speaker'] = '$player'
        self.assertIn('PRIVATE_SHOP_ESTATE_PERSONAL_SAY', self.codes())
        node['speaker'] = '$provider'; node['kind'] = 'choice'
        self.assertIn('PRIVATE_SHOP_ESTATE_PERSONAL_SAY', self.codes())

    def test_disabled_notice_does_not_authorize_remote_knowledge(self):
        self.scenario['enabled']['offers'] = []
        self.assertIn('PRIVATE_SHOP_ESTATE_NOTICE_ROUTE', self.codes())

    def test_different_decedent_or_recipient_rejected(self):
        self.scenario['task_message_notices']['bindings'][0]['decedent_actor_id'] = 'witness'
        self.assertIn('PRIVATE_SHOP_ESTATE_NOTICE_ROUTE', self.codes())
        self.scenario['task_message_notices']['bindings'][0]['decedent_actor_id'] = 'owner'
        self.index['offers']['notice']['goal']['receiver_actor'] = 'witness'
        self.assertIn('PRIVATE_SHOP_ESTATE_NOTICE_ROUTE', self.codes())

    def test_missing_life_or_fake_self_notice_rejected(self):
        del self.scenario['family_life']
        self.assertIn('PRIVATE_SHOP_ESTATE_DEATH_SOURCE', self.codes())
        self.index['offers']['notice']['issuer']['id'] = 'owner'
        self.assertIn('PRIVATE_SHOP_ESTATE_NOTICE_ROUTE', self.codes())

    def test_original_order_profile_required(self):
        self.scenario['task_shop_order_profile'] = 'invented'
        self.assertIn('PRIVATE_SHOP_ESTATE_ORDER_PAIR', self.codes())

    def test_no_author_mutation(self):
        before = copy.deepcopy((self.pack, self.index))
        self.codes()
        self.assertEqual((self.pack, self.index), before)

class PrivateShopEstateSchemaTest(unittest.TestCase):
    """类型引用与运行真值隔离，版本选择沿官方enum准入合同。"""
    def test_schema_references_and_no_runtime_fields(self):
        from tools.content.private_shop_liquidation_schema import with_private_shop_liquidation_schema
        source = {'$defs': {'dynamic_world': {'properties': {}}, 'entity_id': {'type': 'string'}}, 'properties': {'required_capabilities': {'items': {'enum': ['private_shops_v1']}}}}
        before = copy.deepcopy(source)
        result = with_private_shop_liquidation_schema(source)
        self.assertEqual(source, before)
        self.assertEqual(with_private_shop_liquidation_schema(result), result)
        policy = result['$defs']['private_shop_executor_policy']
        self.assertEqual(policy['properties']['executor_actor_id'], {'$ref': '#/$defs/reference_actors'})
        self.assertEqual(policy['properties']['executor_role_id'], {'$ref': '#/$defs/reference_roles'})
        self.assertEqual(policy['properties']['executor_dialogue_id'], {'$ref': '#/$defs/reference_dialogues'})
        self.assertEqual(policy['properties']['premises_id'], {'$ref': '#/$defs/entity_id'})
        self.assertEqual(policy['properties']['executor_acceptance_node_id'], {'$ref': '#/$defs/entity_id'})
        self.assertFalse(policy['additionalProperties'])
        catalog = result['$defs']['private_shop_liquidation_catalog']
        self.assertEqual(catalog['properties']['version'], {'type': 'integer', 'enum': [1]})
        self.assertEqual(set(catalog['properties']), {'version', 'policies'})
        self.assertFalse(catalog['additionalProperties'])
        self.assertEqual(result['properties']['required_capabilities']['items']['enum'], ['private_shops_v1', 'private_shop_liquidations_v1'])

    def test_conflicting_schema_rejected(self):
        from tools.content.private_shop_liquidation_schema import with_private_shop_liquidation_schema
        for original in ({'$defs': {'dynamic_world': {'properties': {}},
                                   'private_shop_executor_policy': {'type': 'string'}}},
                         {'$defs': {'dynamic_world': {'properties': {'private_shop_liquidations': {}}}}}):
            with self.assertRaises(ValueError):
                with_private_shop_liquidation_schema(original)


if __name__ == '__main__':
    unittest.main()
