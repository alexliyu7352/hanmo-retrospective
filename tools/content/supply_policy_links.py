"""继任供给只冻结新政策条款，真实发行、接管批准和采购预约必须留待原生执行。"""
CAPABILITY = 'economic_supply_policies_v1'
FIELDS = {'id', 'original_source_key', 'organization_id', 'template_id', 'terms_version', 'unit',
          'funding_wallet_id', 'reward_wallet_id', 'accept_role', 'report_role', 'measure',
          'target_amount', 'max_claim_quantity', 'purchase_budget_per_unit', 'reward_per_unit',
          'deadline_ticks', 'valid_ticks', 'public_report'}
MAX = 9223372036854775807


def check_economic_supply_policies(pack, index):
    """只检查作者明确的新账开关与同库条款，不改旧Need或制造政策/批准来源。"""
    issues = []
    tables = {}
    for name in ('task_templates', 'roles', 'organizations', 'items'):
        source = index.get(name, {})
        tables[name] = dict(source) if isinstance(source, dict) else {r['id']: r for r in source}
        tables[name].update({r['id']: r for r in pack.get(name, [])})
    for n, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if 'economic_supply_policies' not in dynamic:
            continue
        path = ('scenarios', n, 'dynamic_world', 'economic_supply_policies')
        def issue(code, message):
            """诊断保原作者位置；无证资产与来源不静默替换为可用默认。"""
            issues.append((code, path, message))
        config = dynamic['economic_supply_policies']
        if not isinstance(config, dict) or set(config) != {'version', 'policies'} or type(config.get('version')) is not int or config['version'] != 1:
            issue('SUPPLY_POLICY_PROFILE', '继任供给须为显式1版政策表，不能输入运行期账或批准事实')
            continue
        if dynamic.get('profile') != 'local_society_v1' or dynamic.get('political_takeover', {}).get('version') != 1 or dynamic.get('political_takeover', {}).get('enabled') is not True or CAPABILITY not in pack.get('required_capabilities', []):
            issue('SUPPLY_POLICY_DEPENDENCIES', '须声明新能力和原真实接管，政治绑定段由同一开关成对选择')
        policies = config['policies']
        if not isinstance(policies, dict) or not 1 <= len(policies) <= 4096:
            issue('SUPPLY_POLICY_TABLE', '政策表须有1至4096项')
            continue
        enabled = scenario.get('enabled', {})
        initial = scenario.get('initial', {})
        wallets = {r['id']: r for r in initial.get('wallets', [])}
        inventories = {r['id']: r for r in initial.get('inventories', [])}
        roles = {r['role_id']: r for r in initial.get('roles', [])}
        sources = dynamic.get('local_economy', {}).get('supply_sources', {})
        organization = dynamic.get('organization', {})
        rules = dynamic.get('political_takeover', {}).get('rules', {})
        for key, policy in policies.items():
            if not isinstance(policy, dict) or set(policy) not in (FIELDS, FIELDS | {'reward_total'}):
                issue('SUPPLY_POLICY_FIELDS', '条款须完整且闭集；库、物品、地点只能取原具名供给源')
                continue
            string_fields = FIELDS - {'target_amount', 'max_claim_quantity', 'purchase_budget_per_unit', 'reward_per_unit', 'deadline_ticks', 'valid_ticks', 'public_report'}
            if policy['id'] != key or any(not isinstance(policy[k], str) or not policy[k] or len(policy[k]) > 256 for k in string_fields):
                issue('SUPPLY_POLICY_ID', '每项须使用同表键与有界具名引用')
                continue
            numeric = ('target_amount', 'max_claim_quantity', 'purchase_budget_per_unit', 'reward_per_unit', 'deadline_ticks', 'valid_ticks')
            if any(type(policy[k]) is not int or policy[k] < (0 if k in ('purchase_budget_per_unit', 'reward_per_unit') else 1) or policy[k] > MAX for k in numeric):
                issue('SUPPLY_POLICY_AMOUNT', '数量、期限与整数金额必须有效且有界')
                continue
            reward = policy.get('reward_total')
            if reward is not None and (type(reward) is not int or not 0 <= reward <= MAX or policy['reward_per_unit'] != 0):
                issue('SUPPLY_POLICY_REWARD', '总酬金与逐单位酬金互斥且不得溢出')
                continue
            if policy['unit'] != 'unit' or policy['terms_version'] != '1' or policy['measure'] not in ('stock_gap', 'cumulative_delivery') or policy['valid_ticks'] > 720 or type(policy['public_report']) is not bool:
                issue('SUPPLY_POLICY_TERMS', '只支持明确采购计量、1版条款和1至720半日政策期限')
            commitment = policy['max_claim_quantity'] * policy['purchase_budget_per_unit'] + (reward if reward is not None else policy['max_claim_quantity'] * policy['reward_per_unit'])
            if commitment > MAX:
                issue('SUPPLY_POLICY_OVERFLOW', '单合同本金与酬金合计不能溢出')
            source = sources.get(policy['original_source_key'])
            if not source or source.get('source_key') != policy['original_source_key']:
                issue('SUPPLY_POLICY_SOURCE', '须引用本情景原登记的同ID供给源')
                continue
            if policy['organization_id'] not in tables['organizations'] or policy['organization_id'] not in enabled.get('organizations', []):
                issue('SUPPLY_POLICY_ORGANIZATION', '继任机构须真实启用')
            owner = inventories.get(source['inventory_id'], {}).get('owner', {})
            if owner != {'kind': 'organization', 'id': source['organization_id']}:
                issue('SUPPLY_POLICY_PUBLIC_SOURCE', '原库存须为原机构公产，不能借私产或迁往新库')
            template = tables['task_templates'].get(policy['template_id'], {})
            if template.get('template') != 'procurement' or policy['template_id'] not in enabled.get('task_templates', []):
                issue('SUPPLY_POLICY_TEMPLATE', '必须启用真实采购模板')
            for field in ('funding_wallet_id', 'reward_wallet_id'):
                value = wallets.get(policy[field], {}).get('owner', {})
                if value.get('kind') != 'organization' or value.get('id') not in (policy['organization_id'], source['organization_id']):
                    issue('SUPPLY_POLICY_FUNDS', '本金和酬金须来自继任公库或该真实接管原公库，不能借私钱包')
            for field, service in (('accept_role', 'accept_offer'), ('report_role', 'report_offer')):
                role = tables['roles'].get(policy[field], {})
                if role.get('organization_id') != policy['organization_id'] or role.get('place_id') != source['site_id'] or service not in role.get('services', []) or policy[field] not in roles or policy[field] not in enabled.get('roles', []):
                    issue('SUPPLY_POLICY_ROLE', '接报须由原地点真实启用的继任机构岗位办理')
            matched = False
            for rule in rules.values():
                office = organization.get('offices', {}).get(rule.get('successor_office_id'), {})
                if rule.get('site_id') != source['site_id'] or office.get('organization_id') != policy['organization_id']:
                    continue
                for grant_id in office.get('grant_ids', []):
                    grant = organization.get('grants', {}).get(grant_id, {})
                    if grant.get('office_id') != office.get('id') or 'political_issue_supply_policy' not in grant.get('allowed_command_kinds', []) or {'kind': 'site', 'id': source['site_id']} not in grant.get('scopes', []):
                        continue
                    if source['inventory_id'] in grant.get('inventory_ids', []) and {policy['funding_wallet_id'], policy['reward_wallet_id']} <= set(grant.get('wallet_ids', [])) and grant.get('amount_limit', -1) >= commitment:
                        matched = True
            if not matched:
                issue('SUPPLY_POLICY_COMPLETE_GRANT', '同一继任专项Grant须覆盖地点、原库、本金和酬金钱包及合计最高承诺')
    return issues
