"""核验显式人口军籍来源、服役许可与真实工资；只读作者输入，不创造名册或人口。"""


CAPABILITY = 'population_military_v1'
COHORT_FIELDS = {'reserve_cohort_id': ('training', 'trained_reserve'),
                 'serving_cohort_id': ('serving',), 'wounded_cohort_id': ('wounded',),
                 'captive_cohort_id': ('captive',), 'displaced_cohort_id': ('displaced',)}


def check_population_military(pack, index):
    """逐情景验证选入的空军籍slot；未启用adapter的旧情景不接受隐式来源或额外权限。"""
    issues = []
    declared = CAPABILITY in pack.get('required_capabilities', [])
    known_sources = set()
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        adapter = dynamic.get('population_military')
        initial = scenario.get('initial', {})
        source_units = [(camp, unit) for camp in initial.get('garrisons', [])
                        for unit in camp['units'] if 'population_source_id' in unit]
        root = ('scenarios', number, 'dynamic_world', 'population_military')

        def issue(code, path, message):
            """诊断沿精确情景路径返回，既有管线补充源文件与记录身份。"""
            issues.append((code, root + tuple(path), message))

        if adapter is None:
            if source_units:
                issue('POPULATION_MILITARY_REQUIRED', (), '人口来源单位必须启用本情景的军籍adapter')
            if any('military_source_id' in binding for binding in dynamic.get('local_economy', {}).get('provision_bindings', {}).values()):
                issue('POPULATION_MILITARY_PROVISION_PROFILE', (), '新军籍责任字段只允许出现在显式adapter情景')
            continue
        if not declared or dynamic.get('profile') != 'local_society_v1':
            issue('POPULATION_MILITARY_CAPABILITY', (), '人口军籍必须显式声明population_military_v1及原生动态profile')
        economy, organization = dynamic['local_economy'], dynamic['organization']
        sources, consents = adapter['sources'], adapter['consents']
        for key, binding in economy['provision_bindings'].items():
            if 'military_source_id' not in binding or (binding['policy_owner'] != 'military' and binding.get('military_source_id')):
                issue('POPULATION_MILITARY_PROVISION_FIELD', ('sources',), '本profile全部供养绑定须显式给出military_source_id；宏观绑定为空')
            if binding.get('military_source_id') and binding['military_source_id'] not in sources:
                issue('POPULATION_MILITARY_PROVISION_SOURCE', ('sources',), '军籍供养不能引用未登记来源')
        cohorts, obligations = economy['cohorts'], economy['obligations']
        enabled = scenario['enabled']
        camps = {row['id']: row for row in initial.get('garrisons', [])}
        used_units, used_cohorts, used_consents = set(), set(), set()
        for key, consent in consents.items():
            if key != consent['id'] or consent['through_tick'] < consent['from_tick']:
                issue('POPULATION_SERVICE_CONSENT', ('consents', key), '服役同意须有独立稳定身份及合法有效期')
        for key, source in sources.items():
            at = ('sources', key)
            known_sources.add(key)
            if source['id'] != key:
                issue('POPULATION_MILITARY_SOURCE_ID', at, '来源字典身份必须等于记录ID')
            camp = index['garrisons'].get(source['garrison_id'], {})
            office = index['offices'].get(source['military_office_id'], {})
            org_office = organization['offices'].get(source['organization_office_id'], {})
            unit = next((row for row in camps.get(source['garrison_id'], {}).get('units', [])
                         if row['id'] == source['unit_id']), {})
            prototype = next((row for row in camp.get('units', []) if row['id'] == source['unit_id']), {})
            if (source['unit_id'] in used_units or unit.get('population_source_id') != key
                    or prototype.get('population_source_id') != key or 'initial_soldiers' in unit):
                issue('POPULATION_MILITARY_UNIT_SOURCE', at + ('unit_id',), '来源必须唯一绑定本情景实际启用的空军籍slot，不能复用静态士卒')
            used_units.add(source['unit_id'])
            if any(unit.get(field) != source[field] or prototype.get(field) != source[field] for field in ('initial_training', 'initial_fatigue')):
                issue('POPULATION_MILITARY_INITIAL_QUALITY', at, '既有军事训练初态与来源品质须明确一致，不能把群体训练时长换算成军事数值')
            if (camp.get('organization_id') != source['organization_id']
                    or camp.get('place_id') != source['home_site_id']
                    or camps.get(source['garrison_id'], {}).get('organization_id') != source['organization_id']
                    or source['garrison_id'] not in enabled.get('garrisons', [])
                    or source['military_office_id'] not in enabled.get('offices', [])
                    or office.get('organization_id') != source['organization_id']
                    or office.get('garrison_id') != source['garrison_id']
                    or org_office.get('organization_id') != source['organization_id']):
                issue('POPULATION_MILITARY_INTAKE_AUTHORITY', at, '接收驻军、军职、组织职务与原所在地必须真实同属同地')
            grants = [organization['grants'][id] for id in org_office.get('grant_ids', [])
                      if id in organization['grants']]
            for command in ('mobilize_reserve', 'release_reserve'):
                if not any(command in grant['allowed_command_kinds']
                           and {'kind': 'site', 'id': source['home_site_id']} in grant['scopes']
                           and grant['amount_limit'] > 0 for grant in grants):
                    issue('POPULATION_MILITARY_GRANT', at + ('organization_office_id',), '来源须另有真实同地组织授权：' + command)
            consent = consents.get(source['service_consent_source_id'], {})
            if (consent.get('cohort_id') != source['reserve_cohort_id']
                    or consent.get('organization_id') != source['organization_id']
                    or consent.get('garrison_id') != source['garrison_id']
                    or consent.get('max_active_count', 0) < source['capacity']):
                issue('POPULATION_MILITARY_SERVICE_CONSENT', at + ('service_consent_source_id',), '训练同意不能代替本群体对本组织驻军的独立服役同意')
            used_consents.add(source['service_consent_source_id'])
            source_cohorts = []
            for field, statuses in COHORT_FIELDS.items():
                identifier = source[field]; cohort = cohorts.get(identifier, {})
                source_cohorts.append(identifier)
                if identifier in used_cohorts:
                    issue('POPULATION_MILITARY_COHORT_UNIQUE', at + (field,), '本片每个军籍slot独占同户状态群体，不能跨来源混编')
                used_cohorts.add(identifier)
                if (cohort.get('status') not in statuses or cohort.get('site_id') != source['home_site_id']
                        or cohort.get('household_group_id') != source['household_group_id']
                        or cohort.get('affiliation_id') != source['organization_id']
                        or (field != 'reserve_cohort_id' and (cohort.get('headcount') != 0 or cohort.get('labor_capacity') != 0))
                        or (field == 'reserve_cohort_id' and cohort.get('status') == 'training' and cohort.get('headcount') != 0)):
                    issue('POPULATION_MILITARY_COHORT', at + (field,), '来源状态群体须同地同户同组织；训练容器及初始军籍衍生群体必须为空')
                if field in ('serving_cohort_id', 'wounded_cohort_id', 'captive_cohort_id'):
                    if any(row['beneficiary'] == {'kind': 'cohort', 'id': identifier} and row['kind'] == 'ration'
                           for row in obligations.values()):
                        issue('POPULATION_MILITARY_DUPLICATE_RATION', at + (field,), '军籍承担现役、伤兵及俘虏实际口粮，不得另留宏观重复责任')
                    bindings = [row for row in economy['provision_bindings'].values() if row['subject'] == {'kind': 'cohort', 'id': identifier}]
                    if len(bindings) != 1 or any(row['policy_owner'] != 'military' or row.get('military_source_id') != key or row['payer_inventory'] or row['obligation_id'] or row['from_tick'] != 0 or row['through_tick'] != 2147483647 for row in bindings):
                        issue('POPULATION_MILITARY_PROVISION_SOURCE', at + (field,), '军籍状态须有唯一永久来源绑定，实际粮库由当前军籍与羁押记录解析')
            paid = set()
            household = economy['households'].get(source['household_group_id'], {})
            for id in source['wage_obligation_ids']:
                wage = obligations.get(id, {})
                rate = economy['rates'].get(wage.get('rate_profile'), {})
                if (wage.get('kind') != 'wage' or not wage.get('active')
                        or wage.get('principal') != {'kind': 'organization', 'id': source['organization_id']}
                        or wage.get('destination') != household.get('wallet_id')
                        or wage.get('beneficiary', {}).get('kind') != 'cohort'
                        or wage.get('beneficiary', {}).get('id') not in source_cohorts
                        or not rate.get('per_capita') or rate.get('denominator') != 1):
                    issue('POPULATION_MILITARY_WAGE', at + ('wage_obligation_ids',), '各状态工资须引用实际公账、原家户收款账户与按人整数率')
                paid.add(wage.get('beneficiary', {}).get('id'))
            if paid != set(source_cohorts):
                issue('POPULATION_MILITARY_WAGE_COVERAGE', at + ('wage_obligation_ids',), '预备、现役、伤、俘、离队工资政策均须明确声明')
        continued_consents, continuation_issues = _integration_policies(adapter, economy, organization, enabled, index)
        used_consents.update(continued_consents)
        for code, path, message in continuation_issues:
            issue(code, path, message)
        if used_consents != set(consents):
            issue('POPULATION_MILITARY_UNUSED_CONSENT', ('consents',), '服役许可必须被本情景真实军籍来源引用')
        for camp, unit in source_units:
            if unit['population_source_id'] not in sources or unit['id'] not in used_units:
                issue('POPULATION_MILITARY_UNIT_SOURCE', ('sources',), '初始人口来源单位不能引用不存在的来源')
    for number, camp in enumerate(pack.get('garrisons', [])):
        for offset, unit in enumerate(camp['units']):
            if 'population_source_id' in unit and (not declared or unit['population_source_id'] not in known_sources):
                issues.append(('POPULATION_MILITARY_CAPABILITY', ('garrisons', number, 'units', offset),
                               '人口来源单位须有明确能力及真实情景来源'))
    return issues


def _integration_policies(adapter, economy, organization, enabled, index):
    """续接只声明另一方的独立服役许可和未来工资，保留原籍与原债务身份。"""
    issues, used_consents, pairs, wages = [], set(), set(), set()
    for key, policy in adapter.get('integration_policies', {}).items():
        at = ('integration_policies', key)

        def issue(code, field, message):
            """只报告此政策的来源错误，不把缺失记录补成默认同意。"""
            issues.append((code, at + field, message))

        source = adapter['sources'].get(policy['source_id'])
        if policy['id'] != key or source is None:
            issue('POPULATION_INTEGRATION_SOURCE', (), '接收政策须有稳定身份并引用本情景真实军籍来源')
            continue
        pair = (source['id'], policy['receiving_organization_id'])
        if pair in pairs:
            issue('POPULATION_INTEGRATION_DUPLICATE', (), '同一军籍对同一接收组织只能有一份明确责任政策')
        pairs.add(pair)
        camp = index['garrisons'].get(policy['receiving_garrison_id'], {})
        receiver = policy['receiving_organization_id']
        if (receiver == source['organization_id'] or receiver not in organization['organizations']
                or camp.get('organization_id') != receiver
                or policy['receiving_garrison_id'] not in enabled.get('garrisons', [])):
            issue('POPULATION_INTEGRATION_RECIPIENT', (), '接收军营必须真实属于另一启用组织，不能修改原籍组织冒充续接')
        consent_id = policy['service_consent_source_id']
        consent = adapter['consents'].get(consent_id, {})
        if (consent_id == source['service_consent_source_id']
                or consent.get('cohort_id') != source['reserve_cohort_id']
                or consent.get('organization_id') != receiver
                or consent.get('garrison_id') != policy['receiving_garrison_id']
                or consent.get('max_active_count', 0) < source['capacity']):
            issue('POPULATION_INTEGRATION_CONSENT', ('service_consent_source_id',),
                  '原服役同意不授权换军籍；须独立声明同一原籍群体对接收组织军营的许可')
        used_consents.add(consent_id)
        expected = {economy['obligations'][identifier]['beneficiary']['id']: economy['obligations'][identifier]
                    for identifier in source['wage_obligation_ids'] if identifier in economy['obligations']}
        paid = set()
        for identifier in policy['wage_obligation_ids']:
            wage = economy['obligations'].get(identifier, {})
            beneficiary = wage.get('beneficiary', {})
            previous = expected.get(beneficiary.get('id'))
            if (identifier in wages or identifier in source['wage_obligation_ids']
                    or beneficiary.get('kind') != 'cohort' or previous is None
                    or wage.get('kind') != 'wage' or wage.get('active') is not False
                    or wage.get('principal') != {'kind': 'organization', 'id': receiver}
                    or any(wage.get(field) != previous[field] for field in ('destination', 'rate_profile', 'period_ticks', 'next_due_tick'))
                    or wage.get('authority_ref') == previous['authority_ref']
                    or wage.get('source_wallet_or_inventory') == previous['source_wallet_or_inventory']):
                issue('POPULATION_INTEGRATION_WAGE', ('wage_obligation_ids',),
                      '接收方须另有未启用的真实工资义务，逐状态保持家户收款人与原工资率及日程；旧债不换义务人')
            if beneficiary.get('id') in paid:
                issue('POPULATION_INTEGRATION_WAGE_COVERAGE', ('wage_obligation_ids',), '每个原籍状态只能有一份接收方未来工资')
            paid.add(beneficiary.get('id'))
            wages.add(identifier)
        if paid != set(expected):
            issue('POPULATION_INTEGRATION_WAGE_COVERAGE', ('wage_obligation_ids',), '接收责任必须覆盖原籍预备、服役、伤、俘和离队的全部工资政策')
        target = index['strongholds'].get(policy.get('target_stronghold_id'), {})
        if policy.get('ration_obligation_ids') and (target.get('garrison_id') != source['garrison_id']
                or target.get('id') not in enabled.get('strongholds', [])):
            issue('POPULATION_INTEGRATION_RATION_TARGET', ('target_stronghold_id',),
                  '未来公库产权必须明确绑定原军籍驻地的实际启用据点，不能泛指日后可能取得的库房')
        public_inventories = {identifier for fort in index['strongholds'].values()
                              if fort['garrison_id'] == source['garrison_id']
                              for identifier in fort['public_inventory_ids']}
        rations = [row for row in economy['obligations'].values()
                   if row['kind'] == 'ration' and row['active']
                   and row['source_wallet_or_inventory'] in public_inventories]
        continued = set()
        for identifier in policy.get('ration_obligation_ids', []):
            ration = economy['obligations'].get(identifier, {})
            matches = [row for row in rations if row['beneficiary'] == ration.get('beneficiary')]
            previous = matches[0] if len(matches) == 1 else None
            inventory = index['inventories'].get(ration.get('source_wallet_or_inventory'), {})
            if (ration.get('kind') != 'ration' or ration.get('active') is not False or previous is None
                    or ration.get('principal') != {'kind': 'organization', 'id': receiver}
                    or inventory.get('owner') != {'kind': 'organization', 'id': source['organization_id']}
                    or inventory.get('id') not in enabled.get('inventories', [])
                    or ration.get('authority_ref') == previous['authority_ref']
                    or any(ration.get(field) != previous[field] for field in ('source_wallet_or_inventory', 'item_id', 'rate_profile', 'period_ticks', 'next_due_tick', 'destination'))):
                issue('POPULATION_INTEGRATION_RATION', ('ration_obligation_ids',),
                      '接收口粮须为同一受益人另立停用义务，保留原地真实公库、粮种、整数率和日程；实际占领与续接同候选生效')
            if previous is not None:
                if previous['id'] in continued:
                    issue('POPULATION_INTEGRATION_RATION_COVERAGE', ('ration_obligation_ids',), '同一原口粮责任只能续接一次')
                continued.add(previous['id'])
        if continued != {row['id'] for row in rations}:
            issue('POPULATION_INTEGRATION_RATION_COVERAGE', ('ration_obligation_ids',),
                  '被占公库全部仍生效口粮必须明确续接，包含预备、离队和具名营署人员；不能只保留在役军籍')
    return used_consents, issues


def future_ration_authorizations(dynamic, index):
    """只识别显式整编中的未生效同地口粮产权声明，不能授权其他义务或当前消费。"""
    if not dynamic.get('siege_integration', {}).get('enabled'):
        return set()
    adapter = dynamic.get('population_military', {})
    policies = adapter.get('integration_policies', {})
    selected = dynamic['siege_integration']['default_terms']['population_policy_ids']
    obligations = dynamic['local_economy']['obligations']
    allowed = set()
    for policy_id in selected:
        policy = policies.get(policy_id, {})
        source = adapter.get('sources', {}).get(policy.get('source_id'))
        if source is None:
            continue
        target = index['strongholds'].get(policy.get('target_stronghold_id'), {})
        if target.get('garrison_id') != source['garrison_id']:
            continue
        inventories = set(target['public_inventory_ids'])
        for identifier in policy.get('ration_obligation_ids', []):
            row = obligations.get(identifier, {})
            payer = row.get('source_wallet_or_inventory')
            old = [previous for previous in obligations.values()
                   if previous['kind'] == 'ration' and previous['active']
                   and previous['beneficiary'] == row.get('beneficiary')
                   and previous['source_wallet_or_inventory'] == payer]
            if (len(old) != 1 or row.get('kind') != 'ration' or row.get('active') is not False
                    or payer not in inventories
                    or row.get('principal') != {'kind': 'organization', 'id': policy['receiving_organization_id']}
                    or old[0]['principal'] != {'kind': 'organization', 'id': source['organization_id']}
                    or index['inventories'].get(payer, {}).get('owner') != old[0]['principal']
                    or any(row.get(field) != old[0][field] for field in ('item_id', 'rate_profile', 'period_ticks', 'next_due_tick', 'destination'))):
                continue
            allowed.add((identifier, payer, policy['receiving_organization_id']))
    return allowed
