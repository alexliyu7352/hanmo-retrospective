"""明确NPC经营意图只复用本情景既有权源，不能默认承接一整份待裁定责任。"""


def check_political_supply_renewals(pack, index):
    """作者准入绑定策略、接管规则和新草案；实际签署仍由原Office/Grant逐次检查。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        director = dynamic.get('director', {})
        config = director.get('political_supply_renewals')
        policies = director.get('policies', director.get('subjects', {}))
        selected = [(key, row) for key, row in policies.items()
                    if 'political_supply_renewal' in row.get('templates', []) or 'supply_renewal_intent_id' in row]
        if config is None and not selected:
            continue
        root = ('scenarios', number, 'dynamic_world', 'director', 'political_supply_renewals')

        def issue(code, message):
            """拒因只定位原作者字段，不填充运行期任务、政策或批准事实。"""
            issues.append((code, root, message))

        if (not isinstance(config, dict) or set(config) != {'version', 'intents'}
                or type(config.get('version')) is not int or config['version'] != 1
                or not isinstance(config.get('intents'), dict) or not 1 <= len(config['intents']) <= 64):
            issue('SUPPLY_RENEWAL_PROFILE', '经营续签须为显式1版有界意图表')
            continue
        if (dynamic.get('profile') != 'local_society_v1' or director.get('registry_version') != 1
                or dynamic.get('economic_supply_policies', {}).get('version') != 1
                or dynamic.get('political_takeover', {}).get('enabled') is not True
                or not {'political_supply_renewals_v1', 'economic_supply_policies_v1'} <= set(pack.get('required_capabilities', []))):
            issue('SUPPLY_RENEWAL_DEPENDENCIES', '须显式原供给pair、动态主体登记和独立续签能力')
        rules = dynamic.get('political_takeover', {}).get('rules', {})
        supplies = dynamic.get('economic_supply_policies', {}).get('policies', {})
        offices = dynamic.get('organization', {}).get('offices', {})
        seen = set()
        for key, intent in config['intents'].items():
            if (not isinstance(intent, dict) or set(intent) != {'id', 'takeover_rule_id', 'author_policy_id'}
                    or any(not isinstance(v, str) or not v or len(v) > 4096 for v in intent.values())
                    or intent['id'] != key):
                issue('SUPPLY_RENEWAL_INTENT', '意图只包含同ID及原规则/草案引用')
                continue
            rule = rules.get(intent['takeover_rule_id'], {})
            supply = supplies.get(intent['author_policy_id'], {})
            office = offices.get(rule.get('successor_office_id'), {})
            if not supply or not office or office.get('organization_id') != supply.get('organization_id'):
                issue('SUPPLY_RENEWAL_SCOPE', '原接任Office与新草案机构须一致')
            scope = (intent['takeover_rule_id'], supply.get('original_source_key'))
            if scope in seen:
                issue('SUPPLY_RENEWAL_DUPLICATE', '同接管规则与原供给源不能有竞争经营意图')
            seen.add(scope)
        used = set()
        for key, policy in selected:
            intent_id = policy.get('supply_renewal_intent_id')
            intent = config['intents'].get(intent_id, {})
            supply = supplies.get(intent.get('author_policy_id'), {})
            if ('political_supply_renewal' not in policy.get('templates', []) or not intent
                    or policy.get('organization_id') != supply.get('organization_id')):
                issue('SUPPLY_RENEWAL_POLICY', '模板须具名选择同机构的一项已声明意图')
            used.add(intent_id)
        if not selected or used != set(config['intents']):
            issue('SUPPLY_RENEWAL_UNUSED', '每项显式意图须由真实登记策略采用')
    return issues
