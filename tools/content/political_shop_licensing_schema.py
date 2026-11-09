"""为统一作者候选提供唯一法律Schema片段；不写共享文件或替换旧动态身份约定。"""
from copy import deepcopy


def political_shop_licensing_schema_definitions():
    """外层实体引用沿原reference注解，公职/辖区/铺位保持原动态目录局部ID。"""
    def reference(kind):
        return {'$ref': '#/$defs/reference_' + kind}

    def entity():
        return {'$ref': '#/$defs/entity_id'}

    court = {
        'type': 'object',
        'properties': {
            'id': entity(), 'jurisdiction_id': entity(), 'office_id': entity(),
            'role_id': reference('roles'), 'dialogue_id': reference('dialogues'),
            'request_node_id': entity(),
            'premises_ids': {'type': 'array', 'minItems': 1, 'maxItems': 4096,
                             'uniqueItems': True, 'items': entity()},
            'warrant_reason_codes': {'type': 'array', 'minItems': 1, 'maxItems': 256,
                                     'uniqueItems': True, 'items': entity()},
            'initial_wanted': {
                'type': 'array', 'maxItems': 4096,
                'items': {'type': 'object',
                          'properties': {'actor_id': reference('actors'), 'reason_code': entity()},
                          'required': ['actor_id', 'reason_code'], 'additionalProperties': False}},
            'complete_registry': {'type': 'boolean', 'const': True},
            'permit_ticks': {'type': 'integer', 'minimum': 1, 'maximum': 4096}},
        'required': ['id', 'jurisdiction_id', 'office_id', 'role_id', 'dialogue_id',
                     'request_node_id', 'premises_ids', 'warrant_reason_codes',
                     'initial_wanted', 'complete_registry', 'permit_ticks'],
        'additionalProperties': False}
    catalog = {
        'type': 'object',
        'properties': {'version': {'type': 'integer', 'enum': [1]},
                       'courts': {'type': 'object', 'minProperties': 1, 'maxProperties': 4096,
                                  'additionalProperties': {'$ref': '#/$defs/political_shop_court'}}},
        'required': ['version', 'courts'], 'additionalProperties': False}
    profile2 = {'type': 'object',
                'properties': {'version': {'type': 'integer', 'const': 2},
                               'catalog': {'$ref': '#/$defs/political_shop_licensing_catalog'}},
                'required': ['version', 'catalog'], 'additionalProperties': False}
    return {'political_shop_court': court, 'political_shop_licensing_catalog': catalog,
            'political_shop_licensing_profile2': profile2}


def with_political_shop_licensing_schema(schema):
    """返回独立候选；同名不同义则拒，不能静默覆盖中央后继或改写旧Schema。"""
    result = deepcopy(schema)
    definitions = result['$defs']
    for name in ('entity_id', 'reference_roles', 'reference_dialogues', 'reference_actors'):
        if name not in definitions:
            raise ValueError('Missing original schema definition: ' + name)
    for name, value in political_shop_licensing_schema_definitions().items():
        if name in definitions and definitions[name] != value:
            raise ValueError('Conflicting political shop schema definition: ' + name)
        definitions[name] = value
    properties = definitions['dynamic_world']['properties']
    original = {'$ref': '#/$defs/political_shop_licensing_catalog'}
    selection = {'oneOf': [original, {'$ref': '#/$defs/political_shop_licensing_profile2'}]}
    if ('political_shop_licensing' in properties
            and properties['political_shop_licensing'] not in (original, selection)):
        raise ValueError('Conflicting political shop author selection')
    properties['political_shop_licensing'] = selection
    return result
