"""永久司法提交回执只由显式新profile生成，原law1作者不自动升级或补历史。"""
from copy import deepcopy
from .political_shop_author_profile import political_shop_author_pair

NAME = 'political_shop_licensing_receipts'
CAPABILITY = NAME + '_v1'


def with_political_shop_licensing_receipt_schema(schema):
    """只增加一个严格版本声明；不让作者注入实际result或过去签字来源。"""
    result = deepcopy(schema)
    properties = result['$defs']['dynamic_world']['properties']
    group = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]}},
             'required': ['version'], 'additionalProperties': False}
    if NAME in properties and properties[NAME] != group:
        raise ValueError('Conflicting political shop receipt schema')
    properties[NAME] = group
    choices = result.get('properties', {}).get('required_capabilities', {}).get('items', {}).get('enum')
    if choices is not None and CAPABILITY not in choices:
        choices.append(CAPABILITY)
    return result


def check_political_shop_licensing_receipts(pack, index):
    """原法律/首店目录仍各自完整验证；这里只钉新尾声明和同源依赖。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if NAME not in dynamic:
            continue
        group = dynamic[NAME]
        path = ('scenarios', number, 'dynamic_world', NAME)
        if not isinstance(group, dict) or set(group) != {'version'} or type(group.get('version')) is not int or group['version'] != 1:
            issues.append(('POLITICAL_SHOP_RECEIPT_PROFILE', path, '回执只允许显式版本1，不允许作者预置实际提交来源'))
        if CAPABILITY not in pack.get('required_capabilities', []):
            issues.append(('POLITICAL_SHOP_RECEIPT_CAPABILITY', path, '永久司法回执须声明独立能力'))
        law = dynamic.get('political_shop_licensing')
        shops = dynamic.get('private_shops')
        if (not political_shop_author_pair(law, shops)
                or (law['version'] == 1 and shops['version'] != 1)):
            issues.append(('POLITICAL_SHOP_RECEIPT_DEPENDENCIES', path, '永久回执保持法律1/首店1，新增明确法律2/首店3；回执仍原版1'))
    return issues
