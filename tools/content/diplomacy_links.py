"""外交作者只声明可提议范围；显式答复和双方认可仍由真实运行来源产生。"""


from .diplomatic_response_links import check_diplomatic_responses
from .diplomacy_v2_links import check_diplomatic_delegations

def check_diplomacy(pack, index):
    issues = check_diplomatic_responses(pack, index) + check_diplomatic_delegations(pack, index)
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        if not isinstance(world, dict) or 'diplomacy' not in world:
            continue
        root = ('scenarios', number, 'dynamic_world', 'diplomacy')

        def issue(code, path, message):
            issues.append((code, root + tuple(path), message))

        config = world['diplomacy']
        if (not isinstance(config, dict) or config.get('enabled') is not True
                or type(config.get('version')) is not int or config['version'] not in (1, 2)
                or world.get('profile') != 'local_society_v1'
                or 'native_dynamic_world' not in pack.get('required_capabilities', [])):
            issue('DIPLOMACY_PROFILE', (), '须在所选地方社会情景显式启用外交版本1或2')
            continue
        if 'envoy_tasks' in world:
            issue('DIPLOMACY_DERIVED_TASK_PROFILE', (), '使者责任由同一外交块派生，不另设作者开关')
        organizations = world.get('organization', {})
        groups = organizations.get('organizations', {})
        offices, grants = organizations.get('offices', {}), organizations.get('grants', {})
        texts, places = index.get('texts', {}), index.get('places', {})
        terms, policies = config.get('passage_terms', {}), config.get('mission_policies', {})
        if not isinstance(terms, dict) or not isinstance(policies, dict) or not terms or not policies:
            issue('DIPLOMACY_SOURCES', (), '须有具名条款和使者政策')
            continue
        referenced = set()

        def positive(value):
            return type(value) is int and 0 < value <= 2147483647

        def authorized(office, command, site):
            for grant_id in office.get('grant_ids', []):
                grant = grants.get(grant_id, {})
                if (grant.get('office_id') == office.get('id')
                        and command in grant.get('allowed_command_kinds', [])
                        and {'kind': 'site', 'id': site} in grant.get('scopes', [])):
                    return True
            return False

        for key, term in terms.items():
            path = ('passage_terms', key)
            if not isinstance(term, dict) or term.get('id') != key:
                issue('DIPLOMACY_TERMS_ID', path, '条款键须对应真实稳定ID')
                continue
            grantor, beneficiary = term.get('grantor_organization_id'), term.get('beneficiary_organization_id')
            if grantor not in groups or beneficiary not in groups or grantor == beneficiary:
                issue('DIPLOMACY_PARTIES', path, '通行拟议双方须为两个真实组织')
            targets = term.get('place_ids', [])
            if (term.get('name_key') not in texts or not isinstance(targets, list) or not targets
                    or any(not isinstance(p, str) or p not in places for p in targets)
                    or len(set(p for p in targets if isinstance(p, str))) != len(targets)):
                issue('DIPLOMACY_TERMS_SOURCE', path, '名称与明确通行场所须来自真实作者表，场所不得重复')
            if (not positive(term.get('duration_ticks')) or not positive(term.get('expiry_notice_ticks'))
                    or term['expiry_notice_ticks'] >= term['duration_ticks']):
                issue('DIPLOMACY_TERMS_TIME', path, '期限与提前通知须明确，通知须早于届满')
        for key, policy in policies.items():
            path = ('mission_policies', key)
            if not isinstance(policy, dict) or policy.get('id') != key:
                issue('DIPLOMACY_POLICY_ID', path, '政策键须对应真实稳定ID')
                continue
            issuer, target = policy.get('issuer_organization_id'), policy.get('target_organization_id')
            target_office = offices.get(policy.get('target_office_id'), {})
            if (issuer not in groups or target not in groups or issuer == target
                    or target_office.get('organization_id') != target):
                issue('DIPLOMACY_POLICY_AUTHORITY', path, '政策须指定两个真实组织和目标组织真实接收公职')
            source_site, target_site = policy.get('issuer_site_id'), policy.get('target_site_id')
            if policy.get('name_key') not in texts or source_site not in places or target_site not in places:
                issue('DIPLOMACY_POLICY_SOURCE', path, '政策名称与两侧约定场所须有真实来源')
            window = policy.get('offer_valid_ticks')
            if not positive(window):
                issue('DIPLOMACY_POLICY_TIME', path, '提议期限须为明确正整数')
            selected = policy.get('passage_terms_ids', [])
            if not isinstance(selected, list) or not selected or any(not isinstance(t, str) for t in selected):
                issue('DIPLOMACY_POLICY_TERMS', path, '政策须列出明确可提议的条款')
                selected = []
            if len(set(selected)) != len(selected):
                issue('DIPLOMACY_POLICY_TERMS', path, '政策条款不得重复')
            for term_id in selected:
                term = terms.get(term_id, {})
                if term.get('beneficiary_organization_id') != issuer or term.get('grantor_organization_id') != target:
                    issue('DIPLOMACY_POLICY_TERMS', path, '条款双方须与使者政策一致')
                referenced.add(term_id)
            task = policy.get('task_terms')
            if task is not None:
                if not isinstance(task, dict):
                    issue('DIPLOMACY_TASK_TERMS', path, '使者工时须由同一作者政策完整给出')
                else:
                    if config['version'] == 1 and (task.get('return_report_required') is not False or task.get('report_work_ticks') != 0):
                        issue('DIPLOMACY_RETURN_UNSUPPORTED', path, '当前可玩入口仅本人签发并亲自呈递，不启用未实现的返报责任')
                    if (type(task.get('return_report_required')) is not bool
                            or (task.get('return_report_required') is True and not positive(task.get('report_work_ticks')))
                            or (task.get('return_report_required') is False and task.get('report_work_ticks') != 0)
                            or not positive(task.get('acceptance_valid_ticks')) or not positive(task.get('delivery_work_ticks'))
                            or not positive(window) or task['acceptance_valid_ticks'] > window
                            or task['delivery_work_ticks'] >= window
                            or task.get('continuity_policy_id') != 'continue_under_organization'):
                        issue('DIPLOMACY_TASK_TERMS', path, '受领与交接工时须在原提议有效窗内且沿明确责任连续性')
            issuer_offices = [o for o in offices.values() if isinstance(o, dict) and o.get('organization_id') == issuer]
            required = ['diplomatic_propose_passage'] + (['diplomatic_commission_envoy'] if task is not None else [])
            if not any(all(authorized(o, command, source_site) for command in required) for o in issuer_offices):
                issue('DIPLOMACY_ISSUER_GRANT', path, '须有同一真实签发公职及覆盖原发令场所的专项授权')
            if not all(authorized(target_office, command, target_site) for command in ['diplomatic_receive_passage', 'diplomatic_reply_passage']):
                issue('DIPLOMACY_RECIPIENT_GRANT', path, '目标公职须有约定场所的接收及明确答复专项授权')
        if referenced != set(terms):
            issue('DIPLOMACY_UNREFERENCED_TERMS', ('passage_terms',), '每份条款须由具名政策引用')
    return issues
