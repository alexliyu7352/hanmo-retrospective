"""税源失产连续性独立选中；旧财政1和旧银行不凭包能力隐式扩展。"""
CAPABILITY = 'fiscal_taxpayer_continuity_v1'


def check_fiscal_taxpayer_continuity(pack, index):
    """暂停必须依赖实际接管与原财政日程；不开新税政也可使用原税源连续性。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if 'fiscal_taxpayer_continuity' not in world:
            continue
        config = world['fiscal_taxpayer_continuity']
        takeover = world.get('political_takeover', {})
        fiscal = world.get('fiscal_settlement', {})
        politics = world.get('political_fiscal', {})
        if (not isinstance(config, dict) or config != {'enabled': True, 'version': 1}
                or type(config.get('version')) is not int or type(config.get('enabled')) is not bool
                or world.get('profile') != 'local_society_v1'
                or takeover.get('enabled') is not True or type(takeover.get('version')) is not int
                or takeover.get('version') != 1 or not takeover.get('rules')
                or type(fiscal.get('version')) is not int or fiscal.get('version') != 1
                or not fiscal.get('districts') or politics != {'enabled': True} or politics.get('enabled') is not True
                or CAPABILITY not in pack.get('required_capabilities', [])):
            issues.append(('FISCAL_TAXPAYER_CONTINUITY_PROFILE',
                           ('scenarios', number, 'dynamic_world', 'fiscal_taxpayer_continuity'),
                           '税源失产连续性须显式启用1版、原财政日程与真实接管规则，并声明对应能力'))
    return issues
