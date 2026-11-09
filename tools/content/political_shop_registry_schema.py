"""接册是依赖原法律/首店/接管的独立显式作者选择，不为旧包补运行事实。"""
from copy import deepcopy


def with_political_shop_registry_schema(schema):
    result = deepcopy(schema)
    definitions = result['$defs']
    entity = {'$ref': '#/$defs/entity_id'}
    fields = {key: deepcopy(entity) for key in ['id', 'court_id', 'organization_id', 'office_id', 'request_node_id']}
    fields.update(role_id={'$ref': '#/$defs/reference_roles'}, dialogue_id={'$ref': '#/$defs/reference_dialogues'})
    policy = {'type': 'object', 'properties': fields, 'required': list(fields), 'additionalProperties': False}
    catalog = {'type': 'object', 'properties': {'enabled': {'const': True, 'type': 'boolean'},
        'version': {'type': 'integer', 'enum': [1]}, 'policies': {'type': 'object', 'minProperties': 1,
        'maxProperties': 4096, 'additionalProperties': policy}}, 'required': ['enabled', 'version', 'policies'], 'additionalProperties': False}
    name = 'political_shop_registry_successions'
    old = definitions['dynamic_world']['properties'].get(name)
    if old is not None and old != catalog:
        raise ValueError('Conflicting political registry schema')
    definitions['dynamic_world']['properties'][name] = catalog
    choices = result.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if choices is not None and 'political_shop_registry_successions_v1' not in choices:
        choices.append('political_shop_registry_successions_v1')
    return result
