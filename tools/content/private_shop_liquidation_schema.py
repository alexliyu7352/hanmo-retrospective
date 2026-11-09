"""限店委托作者声明；只描述原当面入口，不包含运行委托、死亡、请求或资金事实。"""
from copy import deepcopy


def with_private_shop_liquidation_schema(schema):
    """按原引用注解生成独立候选，重复同义可重入，异义定义拒绝。"""
    result = deepcopy(schema)
    defs = result['$defs']
    def ref(name):
        return {'$ref': '#/$defs/' + name}
    def record(properties):
        return {'type': 'object', 'properties': properties, 'required': list(properties),
                'additionalProperties': False}
    policy = record({
        'id': ref('entity_id'), 'premises_id': ref('entity_id'),
        'proprietor_actor_id': ref('reference_actors'), 'executor_actor_id': ref('reference_actors'),
        'proprietor_role_id': ref('reference_roles'), 'executor_role_id': ref('reference_roles'),
        'proprietor_dialogue_id': ref('reference_dialogues'),
        'proprietor_appointment_node_id': ref('entity_id'),
        'executor_dialogue_id': ref('reference_dialogues'),
        'executor_acceptance_node_id': ref('entity_id')})
    catalog = record({'version': {'type': 'integer', 'enum': [1]},
        'policies': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096,
                     'additionalProperties': ref('private_shop_executor_policy')}})
    for name, value in [('private_shop_executor_policy', policy),
                        ('private_shop_liquidation_catalog', catalog)]:
        if name in defs and defs[name] != value:
            raise ValueError('Conflicting private shop liquidation schema: ' + name)
        defs[name] = value
    target = defs['dynamic_world']['properties']
    reference = ref('private_shop_liquidation_catalog')
    if 'private_shop_liquidations' in target and target['private_shop_liquidations'] != reference:
        raise ValueError('Conflicting private shop liquidation profile')
    target['private_shop_liquidations'] = reference
    try:
        capability_ids = result['properties']['required_capabilities']['items']['enum']
    except (KeyError, TypeError) as error:
        raise ValueError('PrivateShopLiquidationCapabilitySchemaRequired') from error
    if not isinstance(capability_ids, list) or any(not isinstance(x, str) for x in capability_ids):
        raise ValueError('PrivateShopLiquidationCapabilitySchemaRequired')
    if 'private_shop_liquidations_v1' not in capability_ids:
        capability_ids.append('private_shop_liquidations_v1')
    return result
