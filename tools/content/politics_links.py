"""政治作者叶核实际辖区、Office及公库，不替作者生成人物或扩大任官授权。"""


def check_political_fiscal(pack, index):
    """财政能力必须接同一辖区与完整真实Grant，目录不创建默认政策或税额。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        fiscal = world.get('political_fiscal')
        if fiscal is None or not fiscal.get('enabled'):
            continue
        root = ('scenarios', number, 'dynamic_world', 'political_fiscal')
        politics = world.get('politics', {})
        organization = world.get('organization', {})
        if not politics.get('enabled') or not {'native_dynamic_world'} <= set(pack.get('required_capabilities', [])):
            issues.append(('FISCAL_CAPABILITY', root, '财政须显式登记政治与财政能力，不能默认开征'))
        offices = organization.get('offices', {})
        grants = organization.get('grants', {})
        for ident, jurisdiction in politics.get('jurisdictions', {}).items():
            covered = False
            for office in offices.values():
                if office.get('organization_id') != jurisdiction.get('political_organization_id'):
                    continue
                for grant_id in office.get('grant_ids', []):
                    grant = grants.get(grant_id, {})
                    if ('political_set_tax' in grant.get('allowed_command_kinds', [])
                            and {'kind': 'site', 'id': jurisdiction.get('seat_place_id')} in grant.get('scopes', [])
                            and jurisdiction.get('public_wallet_id') in grant.get('wallet_ids', [])
                            and set(jurisdiction.get('public_inventory_ids', [])) & set(grant.get('inventory_ids', []))):
                        covered = True
            if not covered:
                issues.append(('FISCAL_COMPLETE_GRANT_REQUIRED', root + (ident,), '辖区须有同一真实Grant覆盖治所、公钱和至少一个公粮容器'))
    return issues


def check_politics(pack, index):
    """沿现有内容诊断三元组返回精确字段；中央只需登记此具名检查器。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        politics = world.get('politics')
        if politics is None:
            continue
        root = ('scenarios', number, 'dynamic_world', 'politics')

        def issue(code, path, message):
            """原错误路径用于编辑器定位，不自行修补或静默扩大初态。"""
            issues.append((code, root + tuple(path), message))

        if not politics.get('enabled') or not {'native_dynamic_world', 'regional_life_v1'} <= set(pack.get('required_capabilities', [])):
            issue('POLITICS_CAPABILITY', (), '政治须显式启用原生与地方生活，并由politics/1模块身份隔离')
        organization = world.get('organization', {})
        offices, organizations = organization.get('offices', {}), organization.get('organizations', {})
        initial = scenario.get('initial', {})
        wallets = {row['id']: row for row in initial.get('wallets', [])}
        inventories = {row['id']: row for row in initial.get('inventories', [])}
        places = set(scenario.get('enabled', {}).get('places', []))
        seen_offices, seen_seats, seen_assets = set(), set(), set()
        texts = pack.get('texts', {})
        command_names = politics.get('command_name_keys', {})
        if not organization.get('enabled'):
            issue('POLITICS_ORGANIZATION', (), '政治公职必须引用当前情景的真实组织')
        for ident, row in politics.get('jurisdictions', {}).items():
            path = ('jurisdictions', ident)
            principal = row.get('political_organization_id')
            for key in ('name_key', 'office_name_key'):
                if row.get(key) not in texts:
                    issue('POLITICS_TEXT_REQUIRED', path + (key,), '辖区与公职须有可读名称，不能只给内部编号')
            office = offices.get(row.get('office_id'), {})
            if ident != row.get('id') or principal not in organizations or row.get('controlling_organization_id') != principal:
                issue('POLITICS_JURISDICTION', path, '首版稳定辖区须明确同一政治归属与实际控制；占领不能伪装整合')
            if row.get('revision') != 0 or row.get('seat_place_id') not in places:
                issue('POLITICS_SEAT', path, '辖区须引用当前实际启用的治所，初态版本须为零')
            if office.get('organization_id') != principal or row.get('office_id') in seen_offices:
                issue('POLITICS_OFFICE', path + ('office_id',), '每个辖区须有同组织的唯一Office，不复制另一套负责人')
            if row.get('seat_place_id') in seen_seats:
                issue('POLITICS_DUPLICATE_SEAT', path + ('seat_place_id',), '本版一个治所只能归一个辖区，区域统辖需后续明确集合')
            for grant_id in office.get('grant_ids', []):
                grant = organization.get('grants', {}).get(grant_id, {})
                for kind in grant.get('allowed_command_kinds', []):
                    if command_names.get(kind) not in texts:
                        issue('POLITICS_COMMAND_NAME', path, '每项真实授权须有可读职责名称')
                allowed_accounts = {row.get('public_wallet_id'), *row.get('public_inventory_ids', [])}
                for scope in grant.get('scopes', []):
                    if not (scope.get('kind') == 'site' and scope.get('id') == row.get('seat_place_id') or scope.get('kind') == 'account' and scope.get('id') in allowed_accounts):
                        issue('POLITICS_OFFICE_SCOPE', path, '地方职责只能指向本辖区治所与公库；区域统辖另按正式规则扩展')
            seen_offices.add(row.get('office_id'))
            seen_seats.add(row.get('seat_place_id'))
            containers = [('public_wallet_id', row.get('public_wallet_id'), wallets)]
            containers.extend(('public_inventory_ids', item, inventories) for item in row.get('public_inventory_ids', []))
            if not row.get('public_inventory_ids'):
                issue('POLITICS_GRANARY_REQUIRED', path, '辖区必须列明实际公共仓储')
            for field, asset, table in containers:
                if asset in seen_assets or table.get(asset, {}).get('owner') != {'kind': 'organization', 'id': principal}:
                    issue('POLITICS_PUBLIC_ASSET', path + (field,), '公库必须为组织当前真实容器，不能复制公库或混入私产')
                seen_assets.add(asset)
        policies = politics.get('appointment_policies', {})
        if not policies:
            issue('POLITICS_APPOINTMENT_POLICY', ('appointment_policies',), '至少提供一份明确任用资格和期限政策')
        for ident, policy in policies.items():
            if policy.get('name_key') not in texts:
                issue('POLITICS_POLICY_NAME', ('appointment_policies', ident), '任用政策须有明确可读名称')
            if (policy.get('id') != ident or any(type(policy.get(key)) is not int or policy[key] <= 0
                    for key in ('response_ticks', 'arrival_ticks', 'minimum_administration'))
                    or type(policy.get('minimum_merit')) is not int or policy['minimum_merit'] < 0):
                issue('POLITICS_APPOINTMENT_POLICY', ('appointment_policies', ident), '资格与时窗须显式提供有效整数，不能默认免费授职')
    return issues


def check_political_career(pack, index):
    """NPC政策是作者显式意愿，仅借已有Director准入/公平复评，不从组织成员推定接受。"""
    issues = []
    for n, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        politics = world.get('politics', {})
        director = world.get('director', {})
        org = world.get('organization', {})
        tables = [('subjects', director.get('subjects', {})), ('policies', director.get('policies', {}))]
        for table, values in tables:
            for key, policy in values.items():
                p = policy.get('political_career')
                enabled = 'political_career' in policy.get('templates', [])
                path = ('scenarios', n, 'dynamic_world', 'director', table, key, 'political_career')
                if enabled != (p is not None) or enabled and not politics.get('enabled'):
                    issues.append(('POLITICAL_DIRECTOR_POLICY', path, '政治模板须同本情景显式政治能力与本人策略一起登记'))
                if p is None:
                    continue
                if p.get('response') == 'accept_qualified' and (not p.get('acceptable_organization_ids') or not p.get('acceptable_jurisdiction_ids')):
                    issues.append(('POLITICAL_ACCEPTANCE_SCOPE', path, '接受范围必须显式列出真实组织与辖区'))
                if any(x not in org.get('organizations', {}) for x in p.get('acceptable_organization_ids', [])) or any(x not in politics.get('jurisdictions', {}) for x in p.get('acceptable_jurisdiction_ids', [])):
                    issues.append(('POLITICAL_ACCEPTANCE_REFERENCE', path, '本人接受政策仅能指向该情景已有组织/辖区'))
                for i, post in enumerate(p.get('staffing_posts', [])):
                    j = politics.get('jurisdictions', {}).get(post.get('jurisdiction_id'), {})
                    office = org.get('offices', {}).get(post.get('issuing_office_id'), {})
                    if (not j or not office or office.get('organization_id') != j.get('political_organization_id') or post.get('appointment_policy_id') not in politics.get('appointment_policies', {}) or any(actor not in scenario.get('enabled', {}).get('actors', []) for actor in post.get('candidate_actor_ids', []))):
                        issues.append(('POLITICAL_STAFFING_REFERENCE', path + ('staffing_posts', i), '提名须引用真实签发职位、辖区、政策和人物'))
    return issues
