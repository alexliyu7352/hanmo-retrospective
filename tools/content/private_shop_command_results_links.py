"""经营提交来源是明确能力选择，作者不能填写实际请求/回执或给旧账补证。"""
from copy import deepcopy

CAPABILITY = 'private_shop_command_results_v1'
MODULE = 'private_shop_command_results'


def check_private_shop_command_results(pack, index):
    """只核来源尾与原私铺配对；完整店铺作者准入仍由原checker负责。"""
    del index
    issues = []
    caps = set(pack.get('required_capabilities', []))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or MODULE not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', MODULE)
        declaration = dynamic[MODULE]
        if (not isinstance(declaration, dict) or declaration != {'version': 1}
                or type(declaration.get('version')) is not int):
            issues.append(('PRIVATE_SHOP_RESULTS_DECLARATION', path, '提交来源只准1版声明，不接受运行原件或第二开关'))
        shops = dynamic.get('private_shops')
        if (dynamic.get('profile') != 'local_society_v1' or not isinstance(shops, dict)
                or shops.get('enabled') is not True or type(shops.get('version')) is not int
                or shops.get('version') != 1):
            issues.append(('PRIVATE_SHOP_RESULTS_PAIR', path, '须同一情景明确启用原1版私铺'))
        if not {CAPABILITY, 'private_shops_v1', 'native_dynamic_world'} <= caps:
            issues.append(('PRIVATE_SHOP_RESULTS_CAPABILITY', path, '须声明提交来源、原私铺和原生世界能力'))
        initial = scenario.get('initial')
        if isinstance(initial, dict) and MODULE in initial:
            issues.append(('PRIVATE_SHOP_RESULTS_RUNTIME_STATE', path, '实际提交绑定不能由作者预造'))
    return issues


def with_private_shop_command_results_schema(schema):
    """返回严格独立候选；能力enum与目录同组，原输入不动，同名异义拒。"""
    result = deepcopy(schema)
    definition = {'type': 'object', 'properties': {'version': {'type': 'integer', 'enum': [1]}},
                  'required': ['version'], 'additionalProperties': False}
    name = MODULE + '_catalog'
    definitions = result['$defs']
    if name in definitions and definitions[name] != definition:
        raise ValueError('ConflictingPrivateShopCommandResultsCatalog')
    definitions[name] = definition
    properties = definitions['dynamic_world']['properties']
    ref = {'$ref': '#/$defs/' + name}
    if MODULE in properties and properties[MODULE] != ref:
        raise ValueError('ConflictingPrivateShopCommandResultsProfile')
    properties[MODULE] = ref
    try:
        caps = result['properties']['required_capabilities']['items']['enum']
    except (KeyError, TypeError) as error:
        raise ValueError('PrivateShopCommandResultsCapabilitySchemaRequired') from error
    if not isinstance(caps, list) or any(not isinstance(value, str) for value in caps):
        raise ValueError('PrivateShopCommandResultsCapabilitySchemaRequired')
    if CAPABILITY not in caps:
        caps.append(CAPABILITY)
    return result
