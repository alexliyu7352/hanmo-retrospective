"""Explicit organization response is author intent, never a permission or a treaty effect."""


def check_diplomatic_responses(pack, index):
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        director = world.get('director', {})
        policies = director.get('policies' if 'registry_version' in director else 'subjects', {})
        enabled = 'diplomatic_response_version' in director
        root = ('scenarios', number, 'dynamic_world', 'director')

        def issue(code, path, message):
            issues.append((code, root + tuple(path), message))

        diplomacy = world.get('diplomacy', {})
        if enabled and (type(director.get('diplomatic_response_version')) is not int
                        or director['diplomatic_response_version'] != 1
                        or type(director.get('registry_version')) is not int
                        or director['registry_version'] != 1
                        or diplomacy.get('enabled') is not True
                        or type(diplomacy.get('version')) is not int or diplomacy['version'] not in (1, 2)
                        or not world.get('regional_life')
                        or world.get('profile') != 'local_society_v1'):
            issue('DIPLOMATIC_RESPONSE_PROFILE', (), '明确答复策略须随真实外交、原人物登记和地方生活一同启用版本1')
        offices = world.get('organization', {}).get('offices', {})
        missions = diplomacy.get('mission_policies', {})
        terms = diplomacy.get('passage_terms', {})
        for key, policy in policies.items():
            path = ('policies' if 'registry_version' in director else 'subjects', key)
            active = 'diplomatic_response' in policy.get('templates', [])
            response = policy.get('diplomatic_response')
            if active != (response is not None) or ((active or response is not None) and not enabled):
                issue('DIPLOMATIC_RESPONSE_BINDING', path, '新模板、明确策略与版本须完整绑定；旧情景缺省不自动答复')
                continue
            if response is None:
                continue
            if not isinstance(response, dict):
                issue('DIPLOMATIC_RESPONSE_POLICY', path, '答复策略须为具名作者记录')
                continue
            office = offices.get(response.get('office_id'), {})
            if not response.get('id') or not office or policy.get('organization_id') != office.get('organization_id'):
                issue('DIPLOMATIC_RESPONSE_AUTHORITY', path, '策略须绑定本组织真实目标公职，名称不代替专项Grant')
            directives = response.get('directives', [])
            if not isinstance(directives, list) or len(directives) > 256:
                issue('DIPLOMATIC_RESPONSE_DIRECTIVES', path, '有限答复指令不得超过256项')
                continue
            seen = set()
            for n, directive in enumerate(directives):
                at = path + ('diplomatic_response', 'directives', n)
                if not isinstance(directive, dict):
                    issue('DIPLOMATIC_RESPONSE_DIRECTIVE', at, '指令须引用原提议政策与准确条款')
                    continue
                pair = (directive.get('mission_policy_id'), directive.get('terms_id'))
                valid_pair = all(isinstance(value, str) and value for value in pair)
                mission = missions.get(pair[0], {}) if valid_pair else {}
                if (not valid_pair
                        or directive.get('choice') not in ('defer', 'accept', 'refuse')
                        or pair in seen or not mission or pair[1] not in terms
                        or pair[1] not in mission.get('passage_terms_ids', [])
                        or mission.get('target_office_id') != response.get('office_id')
                        or mission.get('target_organization_id') != office.get('organization_id')):
                    issue('DIPLOMATIC_RESPONSE_DIRECTIVE', at, '须为唯一的本公职可答复提议及条款；接触和接受尚不生条约效力')
                if valid_pair:
                    seen.add(pair)
    return issues
