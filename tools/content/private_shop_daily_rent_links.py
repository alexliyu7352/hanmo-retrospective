"""显式日租声明只选择租期规则，不准作者填写余额、应期、付款或暂停结果。"""
from copy import deepcopy

CAPABILITY = 'private_shop_daily_rent_v1'
DECLARATION = {
    'version': 1,
    'profile': 'opening_anchored_two_halfdays_v1',
    'arrears_policy': 'one_unpaid_period_then_suspend_v1',
}


def check_private_shop_daily_rent(pack, index):
    """复用原私铺完整准入；此口只核本策略与原私铺配对，未选情景维持原义。"""
    del index
    issues = []
    caps = set(pack.get('required_capabilities', []))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or 'private_shop_daily_rent' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'private_shop_daily_rent')
        def bad(code, message):
            issues.append((code, path, message))
        declaration = dynamic['private_shop_daily_rent']
        if (not isinstance(declaration, dict) or declaration != DECLARATION
                or type(declaration.get('version')) is not int):
            bad('PRIVATE_SHOP_DAILY_RENT_DECLARATION', '只准1版开设锚定日租和一期欠付暂停；不接受运行状态或另一个开关')
        shops = dynamic.get('private_shops')
        if (dynamic.get('profile') != 'local_society_v1' or not isinstance(shops, dict)
                or shops.get('enabled') is not True or type(shops.get('version')) is not int
                or shops['version'] != 1):
            bad('PRIVATE_SHOP_DAILY_RENT_PAIR', '日租须配对同一原生情景明确启用的1版私铺')
        if not {CAPABILITY, 'private_shops_v1', 'native_dynamic_world'} <= caps:
            bad('PRIVATE_SHOP_DAILY_RENT_CAPABILITY', '须声明日租、原私铺和原生世界能力')
        initial = scenario.get('initial', {})
        if isinstance(initial, dict) and 'private_shop_daily_rent' in initial:
            bad('PRIVATE_SHOP_DAILY_RENT_RUNTIME_STATE', '原日租账只能由真实开设及实际时钟创建')
    return issues


def with_private_shop_daily_rent_schema(schema):
    """返回独立严格Schema候选；重复同义允许，异义或缺正式能力表拒绝。"""
    result = deepcopy(schema)
    definitions = result['$defs']
    name = 'private_shop_daily_rent_catalog'
    definition = {
        'type': 'object',
        'properties': {
            'version': {'type': 'integer', 'enum': [1]},
            'profile': {'type': 'string', 'enum': [DECLARATION['profile']]},
            'arrears_policy': {'type': 'string', 'enum': [DECLARATION['arrears_policy']]},
        },
        'required': list(DECLARATION),
        'additionalProperties': False,
    }
    if name in definitions and definitions[name] != definition:
        raise ValueError('Conflicting private shop daily rent catalog')
    definitions[name] = definition
    properties = definitions['dynamic_world']['properties']
    reference = {'$ref': '#/$defs/' + name}
    if ('private_shop_daily_rent' in properties
            and properties['private_shop_daily_rent'] != reference):
        raise ValueError('Conflicting private shop daily rent profile')
    properties['private_shop_daily_rent'] = reference
    try:
        capabilities = result['properties']['required_capabilities']['items']['enum']
    except (KeyError, TypeError) as error:
        raise ValueError('PrivateShopDailyRentCapabilitySchemaRequired') from error
    if not isinstance(capabilities, list) or any(not isinstance(c, str) for c in capabilities):
        raise ValueError('PrivateShopDailyRentCapabilitySchemaRequired')
    if CAPABILITY not in capabilities:
        capabilities.append(CAPABILITY)
    return result
