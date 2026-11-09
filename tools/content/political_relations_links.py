"""原P3逐对初值；校验真实组合后组织ID，不重排、不补Neutral记录、不选P2。"""


def check_political_relations(pack, index):
    del index  # 全局注解引用另验；这里必须在本情景真实Organization目录中存在。
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'political_relations' not in scenario:
            continue
        path = ('scenarios', number, 'political_relations')
        selected = scenario['political_relations']
        if (not isinstance(selected, dict)
                or set(selected) != {'schema_version', 'initial_relations'}
                or type(selected.get('schema_version')) is not int
                or selected['schema_version'] != 1
                or not isinstance(selected.get('initial_relations'), list)):
            issues.append(('POLITICAL_RELATIONS1_SHAPE', path,
                           '须明示schema_version:1与完整原initial_relations数组'))
            continue
        dynamic, clock = scenario.get('dynamic_world'), scenario.get('world_time')
        field = dynamic.get('field_battle') if isinstance(dynamic, dict) else None
        if (not isinstance(clock, dict)
                or clock.get('profile') != 'rational_world_complete_v2'
                or not isinstance(field, dict)
                or field.get('version') != 'field_battle/1'):
            issues.append(('POLITICAL_RELATIONS1_ORIGINAL_PAIR', path,
                           'P3显式选型须原Complete与Field1，不能自动选择任一前置'))
        organization = dynamic.get('organization') if isinstance(dynamic, dict) else None
        groups = organization.get('organizations') if isinstance(organization, dict) else None
        groups = groups if isinstance(groups, dict) else {}
        seen = set()
        for offset, row in enumerate(selected['initial_relations']):
            at = path + ('initial_relations', offset)
            if (not isinstance(row, dict) or set(row) != {'factions', 'relation'}
                    or not isinstance(row.get('factions'), dict)
                    or set(row['factions']) != {'first', 'second'}
                    or row.get('relation') not in ('hostile', 'neutral', 'allied')):
                issues.append(('POLITICAL_RELATIONS1_ROW', at, '原初值行/双方身份/三态必须完整且无未知字段'))
                continue
            first, second = row['factions']['first'], row['factions']['second']
            if (not isinstance(first, str) or not isinstance(second, str)
                    or not first or not second or not first < second):
                issues.append(('POLITICAL_RELATIONS1_CANONICAL_PAIR', at,
                               '必须按组合后的真实StableId显式first<second；不自动重排或把错误当Neutral'))
                continue
            pair = (first, second)
            if pair in seen:
                issues.append(('POLITICAL_RELATIONS1_DUPLICATE_PAIR', at, '同一真实canonical pair只能出现一次'))
            seen.add(pair)
            if first not in groups or second not in groups:
                issues.append(('POLITICAL_RELATIONS1_ORGANIZATION_SOURCE', at,
                               '双方必须是本情景原OrganizationCatalog的真实组织行，不仅全局同名ID'))
    return issues
