"""委任和返报的作者来源；本人知情仍须运行时真实当面签发。"""


def check_diplomatic_delegations(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        diplomacy = world.get('diplomacy', {})
        director = world.get('director', {})
        policies = director.get('policies' if 'registry_version' in director else 'subjects', {})
        version = diplomacy.get('version')
        v2 = type(version) is int and version == 2
        root = ('scenarios', number, 'dynamic_world')

        def issue(code, path, message):
            issues.append((code, root + tuple(path), message))

        extension = diplomacy.get('v2')
        if (v2 != (extension is not None)
                or (v2 and (not isinstance(extension, dict)
                            or type(extension.get('version')) is not int
                            or extension.get('version') != 2))):
            issue('DIPLOMACY_V2_PROFILE', ('diplomacy',), '外交版本2须明确带同版本委任目录，旧版本不得夹带')
            continue
        delegations = extension.get('delegation_policies', {}) if isinstance(extension, dict) else {}
        if v2 and (not isinstance(delegations, dict) or not 1 <= len(delegations) <= 4096
                   or director.get('registry_version') != 1 or not world.get('regional_life')):
            issue('DIPLOMACY_V2_PROFILE', ('diplomacy', 'v2'), '具名委任须随原人物登记及地方生活启用，条目有界')
            continue
        organizations = world.get('organization', {})
        offices, grants = organizations.get('offices', {}), organizations.get('grants', {})
        missions, terms = diplomacy.get('mission_policies', {}), diplomacy.get('passage_terms', {})
        actors = {x.get('actor_id') for x in scenario.get('initial', {}).get('actors', [])}
        actors.update(index.get('actors', {}))
        for key, policy in delegations.items():
            at = ('diplomacy', 'v2', 'delegation_policies', key)
            if not isinstance(policy, dict) or key != policy.get('id'):
                issue('DIPLOMATIC_DELEGATION_ID', at, '委任须使用同键稳定ID')
                continue
            mission = missions.get(policy.get('mission_policy_id'), {})
            office = offices.get(policy.get('issuer_office_id'), {})
            task = mission.get('task_terms', {})
            if (not mission or not office or office.get('organization_id') != mission.get('issuer_organization_id')
                    or policy.get('envoy_actor_id') not in actors
                    or policy.get('terms_id') not in terms
                    or policy.get('terms_id') not in mission.get('passage_terms_ids', [])
                    or task.get('return_report_required') is not True
                    or type(task.get('report_work_ticks')) is not int or task['report_work_ticks'] <= 0):
                issue('DIPLOMATIC_DELEGATION_SOURCE', at, '委任须绑定真实使者、发令公职、原条款和明确返报工时')
                continue
            required = {'diplomatic_propose_passage', 'diplomatic_commission_envoy', 'diplomatic_receive_envoy_report'}
            actual = set()
            for grant_id in office.get('grant_ids', []):
                grant = grants.get(grant_id, {})
                if (grant.get('office_id') == office.get('id')
                        and {'kind': 'site', 'id': mission.get('issuer_site_id')} in grant.get('scopes', [])):
                    actual.update(grant.get('allowed_command_kinds', []))
            if not required <= actual:
                issue('DIPLOMATIC_REPORT_GRANT', at, '原发令公职须有同场所提议、使者委任及接收返报专项权责')
        used = set()
        for key, policy in policies.items():
            active = 'diplomatic_delegation' in policy.get('templates', [])
            binding = policy.get('diplomatic_delegation_policy_id')
            at = ('director', 'policies' if 'registry_version' in director else 'subjects', key)
            if active != (binding is not None) or (active and not v2):
                issue('DIPLOMATIC_DELEGATION_BINDING', at, '委任模板、具名策略和版本2须完整绑定')
                continue
            if not active:
                continue
            delegation = delegations.get(binding, {})
            office = offices.get(delegation.get('issuer_office_id'), {})
            if not delegation or not office or policy.get('organization_id') != office.get('organization_id') or binding in used:
                issue('DIPLOMATIC_DELEGATION_BINDING', at, '每份委任只由同组织明确NPC策略引用一次')
            used.add(binding)
        if v2 and used != set(delegations):
            issue('DIPLOMATIC_DELEGATION_UNBOUND', ('diplomacy', 'v2'), '全部委任须有实际人物登记策略来源')
    return issues
