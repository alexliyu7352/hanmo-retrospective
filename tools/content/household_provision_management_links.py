"""Player provisioning consumes the original private ration source and an explicit Family mandate."""
from .household_provision_links import _check_scenario, _record


def check_household_provision_management(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or 'household_provision_management' not in dynamic:
            continue
        root = ('scenarios', number, 'dynamic_world', 'household_provision_management')

        def issue(code, path, message):
            issues.append((code, root + tuple(path), message))

        section = dynamic['household_provision_management']
        if not isinstance(section, dict) or set(section) != {'version', 'authorization_ids'} or type(section.get('version')) is not int or section['version'] != 1:
            issue('HOUSEHOLD_MANAGEMENT_VERSION', (), '私粮管理须明确声明 version=1 和唯一 authorization_ids，不接受附加权限字段')
            continue
        ids = section['authorization_ids']
        if not isinstance(ids, list) or not 1 <= len(ids) <= 65536 or any(not isinstance(value, str) or not value for value in ids) or len(ids) != len(set(ids)):
            issue('HOUSEHOLD_MANAGEMENT_SOURCES', ('authorization_ids',), '须引用不重复的既有家庭供养来源')
            continue
        source_issues, _ = _check_scenario(scenario, index, ('scenarios', number, 'dynamic_world', 'local_economy', 'household_provisioning'))
        issues.extend(source_issues)
        economy = dynamic.get('local_economy', {})
        provisioning = economy.get('household_provisioning', {}) if isinstance(economy, dict) else {}
        sources = provisioning.get('authorizations', {}) if isinstance(provisioning, dict) else {}
        controls = scenario.get('household_control', {})
        if not isinstance(controls, dict) or controls.get('profile') != 'initial_household_stewardship_v1' or not isinstance(controls.get('initial_mandates'), list):
            issue('HOUSEHOLD_MANAGEMENT_CONTROL', (), '玩家管理须使用 Family 的明确初态私务委托，不以公职、亲缘或同住代替')
            continue
        for ordinal, source_id in enumerate(ids):
            source = _record(sources, source_id)
            if not source or economy.get('enabled') is not True or provisioning.get('version') != 1:
                issue('HOUSEHOLD_MANAGEMENT_SOURCE', ('authorization_ids', ordinal), '须引用本情景已启用的真实家庭私粮授权')
                continue
            matches = [row for row in controls['initial_mandates'] if isinstance(row, dict) and row.get('household_group_id') == source.get('household_group_id') and row.get('operation') == 'configure_household_provisioning' and row.get('actor_accepted') is True]
            if not matches:
                issue('HOUSEHOLD_MANAGEMENT_CONTROL', ('authorization_ids', ordinal), '每项可管理供养须有明确接受的同户 Family 私务委托；具体人物和私务角色由 Family 校验')
    return issues
