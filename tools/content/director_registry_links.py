"""S0作者准入只解析真实初态身份；政策名称不是人物ID，也不授予任何业务权限。"""


def director_policies(world):
    """旧subjects保持原语义，新版本返回可复用政策定义而不伪造逐人目录。"""
    director = world.get('director', {})
    return director.get('policies' if 'registry_version' in director else 'subjects', {})


def initial_director_bindings(scenario):
    """仅按声明顺序匹配现有可行动人物；玩家、未成年、死亡和暂停人物不进入自动业务检查。"""
    world = scenario.get('dynamic_world', {})
    director = world.get('director', {})
    if 'registry_version' not in director:
        return [(actor, policy, ('director', 'subjects', actor))
                for actor, policy in director.get('subjects', {}).items()]
    policies = director_policies(world)
    organizations = world.get('organization', {}).get('organizations', {})
    members = world.get('organization', {}).get('members', {}).values()
    result = []
    for actor in scenario.get('initial', {}).get('actors', []):
        actor_id = actor['actor_id']
        for admission in director.get('admissions', {}).values():
            if actor_id in admission.get('excluded_actor_ids', []):
                continue
            kind = admission.get('kind')
            organization_id = admission.get('organization_id')
            if kind == 'organization_members':
                if organizations.get(organization_id, {}).get('status') != 'active':
                    continue
                if not any(member.get('actor_id') == actor_id and
                           member.get('organization_id') == organization_id and
                           member.get('response') not in ('departed', 'awaiting_release')
                           for member in members):
                    continue
            elif kind != 'all_actors':
                continue
            # 首个匹配即绑定；不因其年龄门未到而偷偷改用后续较宽松的政策。
            if (actor_id != scenario.get('player_actor') and actor.get('alive') and actor.get('available')
                    and actor.get('age_years', -1) >= admission.get('minimum_age_years', 0)
                    and admission.get('policy_id') in policies):
                policy_id = admission['policy_id']
                result.append((actor_id, policies[policy_id], ('director', 'policies', policy_id)))
            break
    return result


def check_director_registry(scenario):
    """字段组合、真实来源和年龄适用门分别核验，未知selector不可落入默认全员成功。"""
    world = scenario.get('dynamic_world', {})
    director = world.get('director', {})
    if 'registry_version' not in director:
        return []
    issues = []
    policies = director.get('policies', {})
    actors = {row['actor_id'] for row in scenario.get('initial', {}).get('actors', [])}
    organizations = world.get('organization', {}).get('organizations', {})
    if director.get('registry_version') != 1 or director.get('subjects'):
        issues.append(('DYNAMIC_DIRECTOR_REGISTRY_VERSION', ('director',), '新登记须为version1且不能混入旧逐人subjects'))
    for key, admission in director.get('admissions', {}).items():
        path = ('director', 'admissions', key)
        policy = policies.get(admission.get('policy_id'))
        if policy is None:
            issues.append(('DYNAMIC_DIRECTOR_POLICY', path, '准入须引用本目录真实policy，不能把人物ID当政策'))
            continue
        kind, organization = admission.get('kind'), admission.get('organization_id')
        if (kind not in ('all_actors', 'organization_members') or
                kind == 'all_actors' and organization or
                kind == 'organization_members' and organization not in organizations):
            issues.append(('DYNAMIC_DIRECTOR_ADMISSION_SCOPE', path, '准入类型及组织范围必须匹配，未知语义明确拒绝'))
        if (kind == 'organization_members' and policy.get('organization_id') and
                policy['organization_id'] != organization):
            issues.append(('DYNAMIC_DIRECTOR_POLICY_ORGANIZATION', path, '成员来源与政策引用须属于同一实际组织'))
        exclusions = admission.get('excluded_actor_ids', [])
        if len(set(exclusions)) != len(exclusions) or any(actor not in actors for actor in exclusions):
            issues.append(('DYNAMIC_DIRECTOR_EXCLUSIONS', path, '排除名单只能包含唯一真实初态人物'))
        adult = bool(policy.get('named_membership_policy_id') or policy.get('recruitment_posts') or
                     set(policy.get('templates', [])) & {'governance_commission', 'private_livelihood', 'replenish_supply', 'fulfill_contract', 'organize_activity', 'political_career', 'diplomatic_response', 'diplomatic_delegation'})
        minimum = admission.get('minimum_age_years', 0)
        if minimum < 0 or minimum > 120 or adult and minimum < 18:
            issues.append(('DYNAMIC_DIRECTOR_ADULT_POLICY_AGE', path, '成人自主模板须显式至少18岁；年龄未知不等于已成年'))
    return issues
