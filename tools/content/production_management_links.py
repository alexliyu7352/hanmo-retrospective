"""核验宏观生产管理的物理占用与作者来源；不运行收成、改资产或创造经验。"""
from collections import defaultdict


def check_production_management(pack, index):
    """只对显式v1块检查交叉引用；错误沿原情景路径返回，旧情景保持空结果。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict):
            continue
        economy = dynamic.get('local_economy', {})
        management = economy.get('production_management')
        if management is None:
            continue
        root = ('scenarios', number, 'dynamic_world', 'local_economy', 'production_management')

        def issue(code, path, message):
            """诊断定位到真实作者字段，不把坏引用换成默认资产。"""
            issues.append((code, root + tuple(path), message))

        if not isinstance(management, dict) or type(management.get('version')) is not int or management.get('version') != 1:
            issue('PRODUCTION_MANAGEMENT_VERSION', (), '生产管理必须明确使用受支持的version=1')
            continue
        assets, seasons, plans = (management.get(key, {}) for key in ('assets', 'seasons', 'plans'))
        if any(not isinstance(table, dict) for table in (assets, seasons, plans)):
            issue('PRODUCTION_MANAGEMENT_SHAPE', (), '土地设施、季节和生产方案均须是具名记录表')
            continue
        sources = set()
        enabled_places = set(scenario.get('enabled', {}).get('places', []))
        for key, asset in assets.items():
            if not isinstance(asset, dict):
                issue('PRODUCTION_ASSET_SHAPE', ('assets', key), '资产须是完整记录')
                continue
            source = asset.get('source_key')
            if not isinstance(source, str):
                source = ''
            if asset.get('id') != key or not source or source in sources:
                issue('PRODUCTION_ASSET_SOURCE', ('assets', key), '记录身份和唯一生产来源必须明确，不重复登记同一土地或设施')
            sources.add(source)
            if asset.get('site_id') not in enabled_places:
                issue('PRODUCTION_ASSET_SITE', ('assets', key, 'site_id'), '生产资产须位于本情景启用的真实地点')
            if asset.get('kind') not in ('land', 'facility') or type(asset.get('capacity_units')) is not int or asset.get('capacity_units', -1) < 0:
                issue('PRODUCTION_ASSET_CAPACITY', ('assets', key), '资产种类与非负整数容量必须明确')
            owner = asset.get('owner', {})
            if not isinstance(owner, dict):
                owner = {}
            table = {'actor': 'actors', 'organization': 'organizations'}.get(owner.get('kind'))
            if table is None or owner.get('id') not in index.get(table, {}):
                issue('PRODUCTION_ASSET_OWNER', ('assets', key, 'owner'), '本版资产必须引用真实人物或组织所有者；家庭控制权尚不能凭同姓推断')
        for key, season in seasons.items():
            if not isinstance(season, dict) or season.get('id') != key:
                issue('PRODUCTION_SEASON_ID', ('seasons', key), '季节表键必须等于记录ID')
                continue
            factors = season.get('factors_bp')
            if type(season.get('origin_tick')) is not int or season['origin_tick'] < 0 or type(season.get('phase_ticks')) is not int or season['phase_ticks'] <= 0 or not isinstance(factors, list) or not 1 <= len(factors) <= 12 or any(type(value) is not int or not 0 <= value <= 20000 for value in factors):
                issue('PRODUCTION_SEASON_BOUNDS', ('seasons', key), '季节起点、周期和最多12项有界整数系数须合法')
        used = defaultdict(int)
        for key, plan in plans.items():
            if not isinstance(plan, dict):
                issue('PRODUCTION_PLAN_SHAPE', ('plans', key), '生产方案须是完整记录')
                continue
            unit = economy.get('production_units', {}).get(key)
            binding = economy.get('production_bindings', {}).get(key, {})
            if plan.get('unit_id') != key or unit is None or binding.get('policy_owner') != 'macro':
                issue('PRODUCTION_PLAN_SOURCE', ('plans', key), '方案必须引用同一macro单一结算者的已有生产单元')
                continue
            if len(unit.get('labor_allocation_ids', [])) > 256:
                issue('PRODUCTION_PLAN_LABOR_BOUND', ('plans', key), '一个可管理单元最多引用256个已承诺劳力来源')
            seen = set()
            totals = defaultdict(int)
            uses = plan.get('asset_uses')
            if not isinstance(uses, list) or not 1 <= len(uses) <= 32:
                issue('PRODUCTION_PLAN_ASSET_BOUND', ('plans', key, 'asset_uses'), '每个生产方案须有1至32项明确占用')
                continue
            for ordinal, use in enumerate(uses):
                if not isinstance(use, dict):
                    issue('PRODUCTION_PLAN_ASSET', ('plans', key, 'asset_uses', ordinal), '占用须是完整记录')
                    continue
                asset_id = use.get('asset_id')
                asset = assets.get(asset_id)
                path = ('plans', key, 'asset_uses', ordinal)
                if not isinstance(asset, dict) or asset_id in seen:
                    issue('PRODUCTION_PLAN_ASSET', path, '每项占用必须引用唯一已声明资产')
                    continue
                seen.add(asset_id)
                if asset.get('site_id') != unit.get('site_id') or asset.get('owner') != unit.get('owner'):
                    issue('PRODUCTION_PLAN_OWNERSHIP', path, '不能跨地点或跨产权占用他人的土地设施')
                units, rate = use.get('units'), use.get('batches_per_unit')
                if type(units) is not int or type(rate) is not int or units <= 0 or rate <= 0:
                    issue('PRODUCTION_PLAN_QUANTITY', path, '占用份数和每份产能均须为正整数')
                    continue
                used[asset_id] += units
                totals[asset.get('kind')] += units * rate
                if type(asset.get('capacity_units')) is not int or used[asset_id] > asset['capacity_units']:
                    issue('PRODUCTION_ASSET_OVERALLOCATED', path, '同一资产的全部生产占用超过其唯一容量')
                if totals[asset.get('kind')] > 9223372036854775807:
                    issue('PRODUCTION_CAPACITY_OVERFLOW', path, '产能的整数合计超出原生范围')
            season_id = plan.get('season_profile_id', '')
            if season_id:
                season = seasons.get(season_id)
                if not isinstance(season, dict):
                    issue('PRODUCTION_PLAN_SEASON', ('plans', key, 'season_profile_id'), '须引用本块真实季节表')
                elif type(season.get('origin_tick')) is not int or season['origin_tick'] > unit.get('next_due_tick', -1):
                    issue('PRODUCTION_SEASON_START', ('plans', key, 'season_profile_id'), '季节表须覆盖生产单元首个到期周期')
    return issues
