"""具名任职只检查作者主体、条款、资源与政策来源；不提前决定同意或制造工资。"""
from .director_registry_links import director_policies, initial_director_bindings


def check_named_membership(pack, index):
    """新能力显式开启；期限、个人政策与单一grant必须在同一正式情景闭合。"""
    issues = []
    capabilities = set(pack.get('required_capabilities', []))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        organization = world.get('organization', {})
        enabled = organization.get('named_membership_enabled', False)
        subjects = director_policies(world)
        fields_present = any(key in organization for key in
                             ('membership_terms', 'named_membership_policies'))
        has_personal = any('named_membership_policy_id' in policy or 'recruitment_posts' in policy
                           for policy in subjects.values())
        if not enabled and not fields_present and not has_personal:
            continue
        root = ('scenarios', number, 'dynamic_world')

        def issue(code, path, message):
            """诊断使用原Schema路径；错误不被静默补成默认同意或额外资源。"""
            issues.append((code, root + tuple(path), message))

        if not enabled or not {'native_dynamic_world', 'regional_life_v1', 'named_membership_v1'} <= capabilities:
            issue('NAMED_MEMBERSHIP_CAPABILITY', ('organization',),
                  '具名任职须在本情景显式启用，并声明原生、地方生活和具名任职能力')
        if 'regional_life' not in world:
            issue('NAMED_MEMBERSHIP_ENTRY', ('regional_life',), '具名接洽须有同代实际场所人物入口')
        actors = {row['actor_id']: row for row in scenario.get('initial', {}).get('actors', [])}
        wallets = {row['id']: row for row in scenario.get('initial', {}).get('wallets', [])}
        enabled_relations = set(scenario.get('enabled', {}).get('relations', []))
        initial_relations = scenario.get('initial', {}).get('relations', [])
        known_pairs = {(row['from_actor'], row['to_actor']) for row in initial_relations
                       if row['id'] in enabled_relations}
        offices = organization.get('offices', {})
        grants = organization.get('grants', {})
        terms = organization.get('membership_terms', {})
        policies = organization.get('named_membership_policies', {})
        rates = world.get('local_economy', {}).get('rates', {})
        if not terms or len(terms) > 64 or not policies or len(policies) > 64:
            issue('NAMED_MEMBERSHIP_TABLES', ('organization',), '任职条款和个人政策须各有1至64条明确目录记录')
        for key, term in terms.items():
            path = ('organization', 'membership_terms', key)
            rate = rates.get(term['wage_rate_profile_id'])
            if (term['organization_id'] not in organization.get('organizations', {})
                    or term['member_policy_id'] not in organization.get('member_policies', {})
                    or term['site_id'] not in scenario.get('enabled', {}).get('places', [])):
                issue('NAMED_MEMBERSHIP_TERMS', path, '组织、成员响应政策与当面场所须在本情景真实启用')
            if not rate or rate['per_capita'] or rate['numerator'] <= 0 or rate['denominator'] <= 0:
                issue('NAMED_MEMBERSHIP_RATE', path + ('wage_rate_profile_id',), '工资须引用正的非人口有理数率，单位为钱/条款周期')
                continue
            if wallets.get(term['payer_wallet_id'], {}).get('owner') != {'kind': 'organization', 'id': term['organization_id']}:
                issue('NAMED_MEMBERSHIP_PAYER', path + ('payer_wallet_id',), '工资仅可出自原雇主真实组织钱包')
            covering = []
            for office in offices.values():
                if office['organization_id'] != term['organization_id']:
                    continue
                for grant_id in office['grant_ids']:
                    grant = grants.get(grant_id, {})
                    if (grant.get('office_id') == office['id'] and grant.get('may_delegate')
                            and {'organization_offer_membership', 'contract_executor'} <= set(grant.get('allowed_command_kinds', []))
                            and term['payer_wallet_id'] in grant.get('wallet_ids', [])
                            and {'kind': 'site', 'id': term['site_id']} in grant.get('scopes', [])
                            and grant.get('amount_limit', -1) >= (rate['numerator'] + rate['denominator'] - 1) // rate['denominator']
                            and grant.get('continuity_policy_id') == term['continuity_policy_id']):
                        covering.append(office)
            if not covering:
                issue('NAMED_MEMBERSHIP_SINGLE_GRANT', path, '同一任职的一份grant须同时覆盖招揽、合同职责、场所、工资钱包及首期额度')
            interval = world.get('director', {}).get('review_interval_ticks', 1)
            if term['offer_duration_ticks'] <= interval:
                issue('NAMED_MEMBERSHIP_EXPIRY', path + ('offer_duration_ticks',), '邀请有效期须实际跨过一次正常Director复评')
            if term['terminal_fraction_policy_id'] != 'terminal_fraction_compensation_v1':
                issue('NAMED_MEMBERSHIP_TERMINAL_RULE', path, '正服务余数补偿须由条款显式冻结，不默改旧工资')
        for actor_id, policy, path in initial_director_bindings(scenario):
            if actor_id not in actors or actor_id == scenario.get('player_actor'):
                issue('NAMED_MEMBERSHIP_SUBJECT', path, '自主决定只属于本情景真实非玩家人物')
            personal = policy.get('named_membership_policy_id')
            if personal is not None and personal not in policies:
                issue('NAMED_MEMBERSHIP_PERSONAL_POLICY', path, '本人必须引用明确个人政策，不能把组织成员政策当作同意')
            posts = policy.get('recruitment_posts', [])
            if len(posts) > 8:
                issue('NAMED_MEMBERSHIP_POST_LIMIT', path, '每人最多8项明确本地招聘职事')
            for position, post in enumerate(posts):
                office, term = offices.get(post['office_id']), terms.get(post['terms_id'])
                target_path = path + ('recruitment_posts', position)
                if (not office or not term or office['holder_actor_id'] != actor_id
                        or office['organization_id'] != term['organization_id']):
                    issue('NAMED_MEMBERSHIP_RECRUITER', target_path, '招聘者须本人实际持有同一组织职务并使用本组织条款')
                if len(post['candidate_actor_ids']) > 16 or not post['candidate_actor_ids']:
                    issue('NAMED_MEMBERSHIP_CANDIDATE_LIMIT', target_path, '招聘职事须明确1至16名候选，不能自动搜全城')
                for candidate in post['candidate_actor_ids']:
                    if candidate not in actors or candidate == actor_id:
                        issue('NAMED_MEMBERSHIP_CANDIDATE', target_path, '候选必须是另一名实际启用人物')
                    if (actor_id, candidate) not in known_pairs:
                        issue('NAMED_MEMBERSHIP_ACQUAINTANCE', target_path, '作者招聘职事的点名熟人须在本情景enabled及initial关系中真实启用，包根关系不等于本局已知')
        # Schema引用注解负责真正跨表实体的namespace，条款/政策/动态office保情景内ID。
        for key, policy in policies.items():
            if policy['id'] != key:
                issue('NAMED_MEMBERSHIP_POLICY_ID', ('organization', 'named_membership_policies', key), '个人政策键须等于记录id')
    return issues
