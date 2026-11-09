"""Explicit authored yield disturbances; no weather simulation or authority from prose."""
CAPABILITY = 'production_disruptions_v1'
PROFILE = 'authored_production_disruptions_v1'
POLICY = 'minimum_remaining_yield_v1'
MAX_INTEGER = 9223372036854775807


def check_production_disruptions(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or 'production_disruptions' not in dynamic:
            continue
        root = ('scenarios', number, 'dynamic_world', 'production_disruptions')

        def issue(code, path, text):
            issues.append((code, root + tuple(path), text))

        if CAPABILITY not in pack.get('required_capabilities', []):
            issue('PRODUCTION_DISRUPTION_CAPABILITY', (), '须声明 production_disruptions_v1；旧核心不能忽略本段继续运行')
        section = dynamic['production_disruptions']
        if not isinstance(section, dict) or set(section) != {'version', 'profile', 'combination_policy', 'causes'} or type(section.get('version')) is not int or section['version'] != 1 or section.get('profile') != PROFILE or section.get('combination_policy') != POLICY:
            issue('PRODUCTION_DISRUPTION_PROFILE', (), '须明确v1作者计划及minimum_remaining_yield_v1；不接受隐含叠加公式')
            continue
        causes = section['causes']
        economy = dynamic.get('local_economy', {})
        if not isinstance(causes, dict) or not 1 <= len(causes) <= 65536 or not isinstance(economy, dict) or economy.get('enabled') is not True:
            issue('PRODUCTION_DISRUPTION_CATALOG', (), '须启用真实地方经济并列出有限具名原因')
            continue
        units, bindings = economy.get('production_units', {}), economy.get('production_bindings', {})
        if not isinstance(units, dict) or not isinstance(bindings, dict):
            issue('PRODUCTION_DISRUPTION_UNITS', (), '须引用已有macro生产单元，不能以本段创建库存或劳力')
            continue
        sites = set(scenario.get('enabled', {}).get('places', []))
        texts = index.get('texts', pack.get('texts', {}))
        sources = set()
        cause_count_by_unit = {}
        for key, cause in causes.items():
            path = ('causes', key)
            required = {'id', 'source_key', 'site_id', 'reason_key', 'from_tick', 'through_tick', 'remaining_yield_bp', 'unit_ids'}
            if not isinstance(cause, dict) or set(cause) != required or cause.get('id') != key:
                issue('PRODUCTION_DISRUPTION_CAUSE', path, '原因记录须有严格字段且ID等于表键')
                continue
            source = cause['source_key']
            if not isinstance(source, str) or not source or source in sources:
                issue('PRODUCTION_DISRUPTION_SOURCE', path + ('source_key',), '同一个原因只能登记一次；多个单元列在同一unit_ids中')
            else:
                sources.add(source)
            if cause['site_id'] not in sites or cause['site_id'] not in index.get('places', {}):
                issue('PRODUCTION_DISRUPTION_SITE', path + ('site_id',), '须引用本情景启用的真实地点')
            if not isinstance(cause['reason_key'], str) or not cause['reason_key'] or cause['reason_key'] not in texts:
                issue('PRODUCTION_DISRUPTION_REASON', path + ('reason_key',), '须有可显示的明确作者原因；文字本身不等于动态事件事实')
            start, end, factor = cause['from_tick'], cause['through_tick'], cause['remaining_yield_bp']
            if type(start) is not int or type(end) is not int or not 0 <= start <= end <= MAX_INTEGER or type(factor) is not int or not 0 <= factor <= 10000:
                issue('PRODUCTION_DISRUPTION_WINDOW', path, '须有有效闭区间及0至10000的剩余产量系数')
            targets = cause['unit_ids']
            if not isinstance(targets, list) or not 1 <= len(targets) <= 256 or any(not isinstance(value, str) or not value for value in targets) or len(targets) != len(set(targets)):
                issue('PRODUCTION_DISRUPTION_TARGETS', path + ('unit_ids',), '须逐个引用本地不重复的实际生产单元')
                continue
            for ordinal, unit_id in enumerate(targets):
                cause_count_by_unit[unit_id] = cause_count_by_unit.get(unit_id, 0) + 1
                if cause_count_by_unit[unit_id] > 32:
                    issue('PRODUCTION_DISRUPTION_UNIT_LIMIT', path + ('unit_ids', ordinal), '每个单元本profile最多32个明确原因')
                unit, binding = units.get(unit_id), bindings.get(unit_id)
                if not isinstance(unit, dict) or unit.get('site_id') != cause['site_id'] or not isinstance(binding, dict) or binding.get('policy_owner') != 'macro':
                    issue('PRODUCTION_DISRUPTION_UNIT', path + ('unit_ids', ordinal), '影响只作用于同地唯一macro生产者，不跨域修改庄园或军需')
    return issues
