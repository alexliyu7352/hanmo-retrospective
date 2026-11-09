"""私铺作者只声明原铺位、本人会面和固定条款；运行合同与资产不能从作者预制。"""
from copy import deepcopy


def with_private_shop_schema(schema):
    """返回独立严格候选，引用沿既有注解namespace，不自造ID重写规则。"""
    result = deepcopy(schema)
    definitions = result['$defs']
    def ref(kind):
        return {'$ref': '#/$defs/reference_' + kind}
    def entity():
        return {'$ref': '#/$defs/entity_id'}
    def obj(fields):
        return {'type': 'object', 'properties': fields, 'required': list(fields), 'additionalProperties': False}
    def integer(low, high):
        return {'type': 'integer', 'minimum': low, 'maximum': high}
    def table(definition, nonempty=False):
        return {'type': 'object', 'maxProperties': 4096, 'minProperties': 1 if nonempty else 0,
                'additionalProperties': {'$ref': '#/$defs/' + definition}}
    premises = obj({'id': entity(), 'place_id': ref('places'), 'lessor_actor_id': ref('actors'),
                    'lessor_wallet_id': ref('wallets'), 'lessor_role_id': ref('roles'),
                    'agreement_dialogue_id': ref('dialogues'), 'agreement_node_id': entity(),
                    'market_shop_id': ref('shops'), 'opening_fee': {'const': 900, 'type': 'integer'},
                    'initial_capital': {'const': 300, 'type': 'integer'}, 'inventory_capacity': {'const': 200, 'type': 'integer'},
                    'daily_rent': {'const': 2, 'type': 'integer'}, 'rent_rate_profile': entity()})
    staff = obj({'id': entity(), 'premises_id': entity(), 'worker_actor_id': ref('actors'),
                 'wage_rate_profile': entity(), 'employer_dialogue_id': ref('dialogues'),
                 'employer_offer_node_id': entity(), 'worker_dialogue_id': ref('dialogues'),
                 'worker_accept_node_id': entity(), 'service_ticks': integer(1, 720), 'service_item_id': ref('items')})
    order = obj({'id': entity(), 'premises_id': entity(), 'client_actor_id': ref('actors'),
                 'client_role_id': ref('roles'), 'dialogue_id': ref('dialogues'), 'node_id': entity(),
                 'task_template_id': ref('task_templates'), 'item_id': ref('items'),
                 'quantity': integer(1, 2147483647), 'contract_price': integer(1, 9223372036854775807),
                 'accept_ticks': integer(1, 720), 'deadline_ticks': integer(1, 720)})
    fresh = {'private_shop_premises': premises, 'private_shop_staff_policy': staff, 'private_shop_order_policy': order}
    fresh['private_shop_catalog'] = obj({'enabled': {'type': 'boolean', 'const': True}, 'version': {'type': 'integer', 'enum': [1, 2, 3]},
        'premises': table('private_shop_premises', True), 'staff': obj({'policies': table('private_shop_staff_policy')}),
        'orders': obj({'policies': table('private_shop_order_policy')})})
    fresh['growth_market_history_selection'] = obj({'version': {'type': 'integer', 'enum': [1, 2]},
        'shop_ids': {'type': 'array', 'minItems': 1, 'maxItems': 4096, 'uniqueItems': True, 'items': ref('shops')}})
    for name, value in fresh.items():
        if name in definitions and definitions[name] != value:
            original = deepcopy(value)
            if name == 'private_shop_catalog':
                original['properties']['version']['enum'] = [1, 2]
            if name != 'private_shop_catalog' or definitions[name] != original:
                raise ValueError('Conflicting private shop schema: ' + name)
        definitions[name] = value
    for name, definition in [('private_shops', 'private_shop_catalog'), ('growth_market_history', 'growth_market_history_selection')]:
        value = {'$ref': '#/$defs/' + definition}
        current = definitions['dynamic_world']['properties'].get(name)
        if current is not None and current != value:
            raise ValueError('Conflicting private shop profile: ' + name)
        definitions['dynamic_world']['properties'][name] = value
    definitions['scenario']['properties']['task_shop_order_profile'] = {'type': 'string', 'enum': ['private_shop_orders_v1']}
    # 新源/能力与Shop2完整配对由原作者链接检查，运行根仍由原native选择。
    for name, profile in (
            ('private_shop_operation_sources', 'original_private_shop_operation_sources_v1'),
            ('private_shop_operation_capabilities', 'purpose_capability_v1')):
        value = obj({'profile': {'type': 'string', 'enum': [profile]}})
        current = definitions['scenario']['properties'].get(name)
        if current is not None and current != value:
            raise ValueError('Conflicting private shop original selection: ' + name)
        definitions['scenario']['properties'][name] = value
    return result
