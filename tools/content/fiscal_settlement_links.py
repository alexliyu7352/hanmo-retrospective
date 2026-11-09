"""财政来源只校验原产权、生产及显式参数；政治政策和钱粮结算仍归各自原生owner。"""


def _row(table, key):
    """不让坏作者类型触发hash错误，也不补造任何实际容器。"""
    value = table.get(key) if isinstance(table, dict) and isinstance(key, str) else None
    return value if isinstance(value, dict) else {}


def _integer(value, low, high):
    """作者百分率与周期是有界整数，bool不作为1接受。"""
    return type(value) is int and low <= value <= high


def check_fiscal_settlement(pack, index):
    """仅独立fiscal_settlement/1启用时检查来源；旧情景没有新增默认税政。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world')
        if not isinstance(dynamic, dict) or dynamic.get('fiscal_settlement') is None:
            continue
        section = dynamic['fiscal_settlement']
        root = ('scenarios', number, 'dynamic_world', 'fiscal_settlement')

        def issue(code, path, message):
            """诊断绑定原字段；无税源不能改成定期给公库加钱。"""
            issues.append((code, root + tuple(path), message))

        if not isinstance(section, dict) or type(section.get('version')) is not int or section.get('version') != 1:
            issue('FISCAL_SETTLEMENT_VERSION', (), '财政结算须显式选择version=1')
            continue
        districts = section.get('districts')
        economy = dynamic.get('local_economy', {})
        if not isinstance(districts, dict) or not 1 <= len(districts) <= 1000 or not isinstance(economy, dict) or economy.get('enabled') is not True:
            issue('FISCAL_SETTLEMENT_SHAPE', (), '财政地区须为具名表并依赖已启用的真实地方经济')
            continue
        initial = scenario.get('initial', {})
        wallets = {row.get('id'): row for row in initial.get('wallets', []) if isinstance(row, dict) and isinstance(row.get('id'), str)}
        inventories = {row.get('id'): row for row in initial.get('inventories', []) if isinstance(row, dict) and isinstance(row.get('id'), str)}
        used_sites, used_sources, used_wallets, used_stocks, used_units = set(), set(), set(), set(), set()
        for district_id, district in districts.items():
            path = ('districts', district_id)
            if not isinstance(district, dict) or district.get('id') != district_id or not isinstance(district.get('site_id'), str):
                issue('FISCAL_DISTRICT_ID', path, '财政地区须有匹配ID与真实地点')
                continue
            site = district['site_id']
            if site in used_sites or site not in scenario.get('enabled', {}).get('places', []):
                issue('FISCAL_DISTRICT_SITE', path + ('site_id',), '同一启用地点只允许一个财政周期，不复制辖区收入')
            used_sites.add(site)
            period, first = district.get('period_ticks'), district.get('first_due_tick')
            if not _integer(period, 1, 2147483647) or not _integer(first, 1, 2147483647) or first < period or not _integer(district.get('initial_tax_base_bp'), 5000, 15000) or not _integer(district.get('normal_recovery_bp'), 0, 300):
                issue('FISCAL_DISTRICT_PARAMETERS', path, '周期/首期、50至150%现金税基和0至300基点恢复速度须显式给出；恢复值是开发参数')
            sources = district.get('sources')
            if not isinstance(sources, dict) or not 1 <= len(sources) <= 10000:
                issue('FISCAL_SOURCE_TABLE', path + ('sources',), '每区须有1至10000项具名真实税源')
                continue
            for source_id, source in sources.items():
                source_path = path + ('sources', source_id)
                if not isinstance(source, dict) or source.get('id') != source_id or any(not isinstance(source.get(key), str) or not source[key] for key in ('wallet_id', 'inventory_id', 'grain_item_id')):
                    issue('FISCAL_SOURCE_ID', source_path, '税源须有匹配ID与原钱包、库存和物品引用')
                    continue
                stock_key = (source['inventory_id'], source['grain_item_id'])
                if source_id in used_sources or source['wallet_id'] in used_wallets or stock_key in used_stocks:
                    issue('FISCAL_DUPLICATE_SOURCE', source_path, '同一钱源或库存物品不能在不同税源重复计税')
                used_sources.add(source_id); used_wallets.add(source['wallet_id']); used_stocks.add(stock_key)
                taxpayer = source.get('taxpayer', {})
                if not isinstance(taxpayer, dict):
                    taxpayer = {}
                owner = None
                if taxpayer.get('kind') == 'cohort':
                    cohort = _row(economy.get('cohorts'), taxpayer.get('id')); home = _row(economy.get('households'), cohort.get('household_group_id'))
                    if cohort and home and cohort.get('site_id') == site and cohort.get('private_wallet_id') == home.get('wallet_id') == source['wallet_id'] and cohort.get('private_inventory_id') == home.get('inventory_id') == source['inventory_id']:
                        owner = {'kind': 'household', 'id': cohort['household_group_id']}
                elif taxpayer.get('kind') == 'organization':
                    org = _row(dynamic.get('organization', {}).get('organizations'), taxpayer.get('id'))
                    if org:
                        owner = {'kind': 'organization', 'id': taxpayer['id']}
                if owner is None or wallets.get(source['wallet_id'], {}).get('owner') != owner or inventories.get(source['inventory_id'], {}).get('owner') != owner:
                    issue('FISCAL_SOURCE_OWNER', source_path, '税源只允许同地群体真实家庭或组织容器；Actor私人钱包不得补贴财政')
                if not _integer(source.get('base_cash_due'), 0, 2147483647) or not _integer(source.get('grain_share_bp'), 0, 10000) or not _integer(source.get('civilian_grain_reserve'), 0, 2147483647) or source['grain_item_id'] not in index.get('items', {}):
                    issue('FISCAL_SOURCE_PARAMETERS', source_path, '基准钱税、基准粮税率和民用留粮须有真实物品与明确整数单位')
                units = source.get('production_unit_ids')
                if not isinstance(units, list) or len(units) > 256 or any(not isinstance(unit, str) for unit in units) or (source.get('grain_share_bp', 0) != 0 and not units):
                    issue('FISCAL_PRODUCTION_LIST', source_path + ('production_unit_ids',), '有粮税时须列实际生产者，最多256项，不从库存余额虚构产量')
                    continue
                for unit_id in units:
                    unit = _row(economy.get('production_units'), unit_id); binding = _row(economy.get('production_bindings'), unit_id)
                    if unit_id in used_units or not unit or unit.get('site_id') != site or unit.get('output_inventory_id') != source['inventory_id'] or unit.get('owner') != owner or binding.get('policy_owner') != 'macro':
                        issue('FISCAL_PRODUCTION_OWNER', source_path + ('production_unit_ids',), '粮税只能消费同地同产权原macro生产收据，生产者不可重复登记')
                    used_units.add(unit_id)
    return issues
