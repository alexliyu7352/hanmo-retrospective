"""新税债Need显式选择原Affairs投影语义；旧财政Need1不自动扩展。"""
CAPABILITY = 'occupation_fiscal_needs_v1'


def check_occupation_fiscal_needs(pack, index):
    """新marker依赖配对新税账和原Need1，不允许无来源的空开关。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if 'occupation_fiscal_needs' not in world:
            continue
        config = world['occupation_fiscal_needs']
        need = world.get('affairs', {}).get('fiscal_need_policies', {})
        if (not isinstance(config, dict) or config != {'enabled': True, 'version': 1}
                or type(config.get('version')) is not int
                or world.get('profile') != 'local_society_v1'
                or world.get('political_occupation_fiscal') != {'enabled': True, 'version': 1}
                or world.get('fiscal_settlement', {}).get('version') != 1
                or type(need.get('version')) is not int or need.get('version') != 1
                or not need.get('policies')
                or CAPABILITY not in pack.get('required_capabilities', [])):
            issues.append(('OCCUPATION_FISCAL_NEEDS_PROFILE',
                           ('scenarios', number, 'dynamic_world', 'occupation_fiscal_needs'),
                           '新税债事项须显式启用1版，并依赖新税账、原财政Need1和对应能力'))
    return issues
