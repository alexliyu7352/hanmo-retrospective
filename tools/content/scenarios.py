"""从已校验目录构造独立剧本初态视图；不创建世界、分配实例ID或消费随机流。"""
from copy import deepcopy

SNAPSHOT_FIELDS = ('playable_actor_ids', 'start_date', 'enabled', 'initial')
BASE_SKILLS = ('infantry', 'cavalry', 'archery', 'martial', 'tactics', 'debate', 'negotiation', 'rites', 'commerce', 'transport')
EXTENDED_SKILLS = ('naval', 'espionage', 'fortification', 'agriculture', 'medicine', 'forging')

def _skills_match(values, extended):
    """显式政策决定精确技艺集合；不按输入额外字段推测升级。"""
    keys = BASE_SKILLS + (EXTENDED_SKILLS if extended else ())
    return isinstance(values, dict) and set(values) == set(keys) and all(type(values[k]) is int and 0 <= values[k] <= 1200 for k in keys)


def _fail(code, path):
    """统一返回作者可定位的失败，不向调用方泄漏半完成目录。"""
    return {'ok': False, 'code': code, 'path': path}


def _ids(value):
    """识别不重复的显式ID集合，拒绝字符串代替数组或隐式去重。"""
    return (isinstance(value, list) and all(isinstance(item, str) and item for item in value)
            and len(value) == len(set(value)))


def _whole(value, low, high):
    """保持作者整数边界，布尔值不能作为日期或年龄进入初态。"""
    return type(value) is int and low <= value <= high


def _date(value):
    """历史日期只作为剧本起始投影，不把年代转换成世界半日计数。"""
    return (isinstance(value, dict) and set(value) == {'year', 'day_of_year', 'half_day'}
            and _whole(value.get('year'), 1, 2147483647)
            and _whole(value.get('day_of_year'), 1, 360)
            and _whole(value.get('half_day'), 0, 1))


def _legacy(catalog, scenario_id, player_actor_id):
    """旧剧本保留定义枚举顺序和缺失年龄，只附可重建的全部静态人物集合。"""
    data = catalog['data']
    player = data['scenarios'][scenario_id]['player_actor']
    if player_actor_id and player_actor_id != player:
        return _fail('InvalidPlayableActor', '/scenarios/' + scenario_id + '/player_actor')
    effective = deepcopy(catalog)
    effective['data']['active_static_actor_ids'] = sorted(data['actors'])
    initial = {'legacy': True, 'player_actor_id': player, 'actors': []}
    for actor in data['actors'].values():
        if not _skills_match(actor.get('skills'), False):
            return _fail('InvalidScenarioSkillProfile', '/actors/' + actor['id'] + '/skills')
        row = {'actor_id': actor['id'], 'place_id': actor['initial_place'], 'alive': True,
               'available': True, 'attributes': deepcopy(actor['attributes']),
               'skills': deepcopy(actor['skills']), 'combat': deepcopy(actor.get('combat', {}))}
        if 'initial_age_years' in actor:
            row['age_years'] = actor['initial_age_years']
        if 'appearance_definition' in actor:
            row['appearance_definition'] = deepcopy(actor['appearance_definition'])
        initial['actors'].append(row)
    return {'ok': True, 'catalog': effective, 'initial': initial}


def resolve(catalog, scenario_id, player_actor_id=''):
    """解析唯一选中剧本为可丢弃深副本；新旧模式不可混用，失败不改任何源数据。"""
    if not isinstance(catalog, dict) or not catalog.get('ok') or not isinstance(catalog.get('data'), dict):
        return _fail('NoCatalog', '')
    data = catalog['data']
    path = '/scenarios/' + str(scenario_id)
    scenario = data.get('scenarios', {}).get(scenario_id)
    if not isinstance(scenario, dict):
        return _fail('UnknownScenario', path)
    if scenario.get('dynamic_world', {}).get('founder_policy') in ('fixed_actor', 'fixed_actor_appearance_v1', 'fixed_actor_appearance_composition_v1'):
        if scenario.get('playable_actor_ids') != [scenario.get('player_actor')] or player_actor_id and player_actor_id != scenario.get('player_actor'):
            return _fail('DynamicFixedActorRequired', path + '/playable_actor_ids')
    from .family_innate_learning import check_family_innate_learning
    if check_family_innate_learning({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
        return _fail('InvalidInnateLearningCatalog', path + '/family_innate_learning')
    from .tactic_lessons import check_tactic_lessons
    if check_tactic_lessons({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
        return _fail('InvalidGrowthTacticLessonCatalog', path + '/dynamic_world/growth_tactic_lessons')
    from .personal_recovery import check_personal_recovery
    if check_personal_recovery({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
        return _fail('InvalidPersonalRecoveryCatalog', path + '/dynamic_world/personal_injury_recovery')
    if 'family_care_relief' in scenario:
        from .child_care_relief import check_child_care_relief
        if check_child_care_relief({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidChildCareReliefCatalog', path + '/family_care_relief')
    if 'family_birth' in scenario or 'family_lineage' in scenario:
        from .family_birth import check_family_birth
        if check_family_birth({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidFamilyBirthCatalog', path + '/family_birth')
    if 'family_disciple_legacy' in scenario:
        from .family_disciple_legacy import check_family_disciple_legacy
        if check_family_disciple_legacy({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidDiscipleControlCatalog', path + '/family_disciple_legacy')
    if 'family_apprenticeship' in scenario:
        from .family_apprenticeship import check_family_apprenticeship
        if check_family_apprenticeship({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidApprenticeshipCatalog', path + '/family_apprenticeship')
    if 'family_marriage' in scenario:
        from .family_marriage import check_family_marriage
        if check_family_marriage({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidFamilyMarriageCatalog', path + '/family_marriage')
    if 'family_inheritance' in scenario:
        from .family_inheritance import check_family_inheritance
        if check_family_inheritance({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidFamilyInheritanceCatalog', path + '/family_inheritance')
    if 'family_bequest' in scenario:
        from .family_bequest import check_family_bequest
        if check_family_bequest({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidFamilyBequestCatalog', path + '/family_bequest')
    if 'family_collateral_legacy' in scenario:
        from .family_collateral_legacy import check_family_collateral_legacy
        if check_family_collateral_legacy({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidCollateralControlCatalog', path + '/family_collateral_legacy')
    if 'family_biological_legacy' in scenario:
        from .family_biological_legacy import check_family_biological_legacy
        if check_family_biological_legacy({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidBiologicalControlCatalog', path + '/family_biological_legacy')
    if 'family_legacy' in scenario:
        from .family_legacy import check_family_legacy
        if check_family_legacy({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidLegacyControlConsentCatalog', path + '/family_legacy')
    if 'family_life' in scenario:
        life = scenario['family_life']
        if life != {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}:
            return _fail('UnsupportedFamilyDeathProfile', path + '/family_life')
        if not isinstance(scenario.get('dynamic_world'), dict) or 'family_life_v1' not in data.get('required_capabilities', []):
            return _fail('FamilyLifeCapabilityRequired', path + '/family_life')
    if 'family_voluntary_paths' in scenario or scenario.get('voluntary_succession', {}).get('profile') == 'living_accepted_family_paths_v2':
        from .family_voluntary_paths import check_family_voluntary_paths
        if check_family_voluntary_paths({'scenarios': [scenario], 'required_capabilities': data.get('required_capabilities', [])}, data):
            return _fail('InvalidVoluntaryPathCatalog', path + '/family_voluntary_paths')
    if 'voluntary_succession' in scenario:
        config = scenario['voluntary_succession']
        if not isinstance(config, dict) or config.get('profile') not in ('authored_voluntary_succession_v1', 'living_accepted_family_paths_v2'):
            return _fail('UnsupportedVoluntarySuccessionProfile', path + '/voluntary_succession')
        if not isinstance(scenario.get('dynamic_world'), dict):
            return _fail('VoluntarySuccessionNativeProfileRequired', path + '/voluntary_succession')
        from .voluntary_succession import check_voluntary_succession
        if check_voluntary_succession({'scenarios': [scenario]}, data):
            return _fail('InvalidSuccessionConsentCatalog', path + '/voluntary_succession')
    if 'adult_adoption' in scenario:

        config = scenario['adult_adoption']
        if not isinstance(config, dict) or config.get('profile') != 'authored_adult_adoption_requests_v1':
            return _fail('UnsupportedAdultAdoptionProfile', path + '/adult_adoption')
        if not isinstance(scenario.get('dynamic_world'), dict):
            return _fail('AdultAdoptionNativeProfileRequired', path + '/adult_adoption')
        from .adult_adoption import check_adult_adoption
        if check_adult_adoption({'scenarios': [scenario]}, data):
            return _fail('InvalidAdultAdoptionCatalog', path + '/adult_adoption')
    if 'household_control' in scenario:
        control = scenario['household_control']
        if not isinstance(control, dict) or control.get('profile') != 'initial_household_stewardship_v1':
            return _fail('UnsupportedHouseholdControlProfile', path + '/household_control')
        if not isinstance(scenario.get('dynamic_world'), dict):
            return _fail('HouseholdControlNativeProfileRequired', path + '/household_control')
    if 'skill_profile' in scenario:
        if scenario['skill_profile'] != 'full_sixteen_v1':
            return _fail('UnsupportedSkillProfile', path + '/skill_profile')
        if not isinstance(scenario.get('dynamic_world'), dict):
            return _fail('ExtendedSkillsNativeProfileRequired', path + '/skill_profile')
        if 'initial' not in scenario:
            return _fail('ExtendedSkillsSnapshotRequired', path + '/initial')
    present = [field for field in SNAPSHOT_FIELDS if field in scenario]
    if not present:
        return _legacy(catalog, scenario_id, player_actor_id)
    if len(present) != len(SNAPSHOT_FIELDS):
        return _fail('PartialScenarioSnapshot', path)
    if not _date(scenario['start_date']):
        return _fail('InvalidScenarioStartDate', path + '/start_date')
    creating = scenario.get('dynamic_world', {}).get('founder_policy') == 'created_character_v1'
    if not _ids(scenario['playable_actor_ids']) or (not scenario['playable_actor_ids'] and not creating):
        return _fail('InvalidPlayableActor', path + '/playable_actor_ids')
    return _resolve_snapshot(catalog, scenario, player_actor_id, path)


_REFERENCE_RULES = {}
INITIAL_FIELDS = {
    'wallets': ('id', 'owner', 'initial_balance'),
    'inventories': ('id', 'owner', 'capacity', 'goods'),
    'roles': ('role_id', 'actor_id', 'enabled'),
    'relations': ('id', 'from_actor', 'to_actor', 'affinity', 'trust'),
    'estates': ('id', 'owner_actor_id', 'manager_actor_id', 'resident_actor_ids',
                'initial_facilities', 'initial_duties'),
    'garrisons': ('id', 'organization_id', 'units'),
    'offices': ('id',), 'strongholds': ('id',),
}
ACTOR_FIELDS = ('actor_id', 'place_id', 'alive', 'available', 'age_years',
                'attributes', 'skills', 'combat', 'appearance_definition')
PASSIVE_TABLES = ('legacy_control_consent_requests', 'succession_consent_requests', 'adult_adoption_requests', 'insight_cards', 'teaching_requests', 'study_teaching_requests', 'actor_cards', 'cards', 'items', 'dialogues', 'personal_skills', 'personal_battlefields',
                  'personal_objectives', 'battlefields', 'troop_types',
                  'military_objectives', 'siege_topologies', 'etiquette_challenges', 'bargain_challenges', 'drill_challenges', 'texts')


def _reference_rule(rule):
    """只缓存类型判别及Schema引用边，不把整份校验Schema塞入世界目录热路径。"""
    if not isinstance(rule, dict):
        return {}
    result = {key: deepcopy(rule[key]) for key in ('type', 'const', '$ref', 'x-hanmo-ref', 'x-hanmo-key-ref') if key in rule}
    for key in ('oneOf', 'anyOf'):
        if key in rule:
            result[key] = [_reference_rule(child) for child in rule[key]]
    if 'properties' in rule:
        result['properties'] = {key: _reference_rule(child) for key, child in rule['properties'].items()}
    for key in ('items', 'additionalProperties'):
        if isinstance(rule.get(key), dict):
            result[key] = _reference_rule(rule[key])
    return result


def register_schema(schema_bytes):
    """登记装载器已验证受信来源的确切Schema字节，以原字节摘要缓存轻量引用描述。"""
    import hashlib
    import json
    raw = schema_bytes.encode('utf8') if isinstance(schema_bytes, str) else schema_bytes
    expected_sha256 = hashlib.sha256(raw).hexdigest()
    try:
        schema = json.loads(raw)
        tables = list(schema['x-hanmo-tables'])
        rules = {table: _reference_rule(schema['properties'][table]['items']) for table in tables}
        defs = {key: _reference_rule(value) for key, value in schema['$defs'].items()}
    except (ValueError, TypeError, KeyError):
        return _fail('InvalidScenarioSchema', '/schema_sha256')
    _REFERENCE_RULES[expected_sha256] = {'tables': tables, 'rules': rules, 'defs': defs}
    return {'ok': True, 'schema_sha256': expected_sha256}


def _possible(value, rule, rules):
    """按原Schema的类型和判别常量选分支，不把普通文本猜成实体引用。"""
    if '$ref' in rule:
        return _possible(value, rules['defs'][rule['$ref'].rsplit('/', 1)[-1]], rules)
    if 'const' in rule:
        return type(value) is type(rule['const']) and value == rule['const']
    checks = {'object': isinstance(value, dict), 'array': isinstance(value, list),
              'string': isinstance(value, str), 'integer': type(value) is int,
              'boolean': type(value) is bool, 'null': value is None}
    if rule.get('type') in checks and not checks[rule['type']]:
        return False
    return not isinstance(value, dict) or all(
        key not in value or 'const' not in child or value[key] == child['const']
        for key, child in rule.get('properties', {}).items())


def _references(value, rule, rules, path=''):
    """沿联合及其公共字段收集真实引用，同路径只出现一次，不丢失军籍分支的兵种与装备。"""
    if '$ref' in rule:
        yield from _references(value, rules['defs'][rule['$ref'].rsplit('/', 1)[-1]], rules, path)
        return
    found = []
    for union in ('oneOf', 'anyOf'):
        if union in rule:
            options = [child for child in rule[union] if _possible(value, child, rules)]
            literals = [child for child in options if 'const' in child]
            for child in literals or options:
                found.extend(_references(value, child, rules, path))
    if 'x-hanmo-ref' in rule and isinstance(value, str):
        found.append((rule['x-hanmo-ref'], value, path))
    if isinstance(value, dict):
        for key, child in value.items():
            if 'x-hanmo-key-ref' in rule:
                found.append((rule['x-hanmo-key-ref'], key, path + '/' + key))
            sub = rule.get('properties', {}).get(key, rule.get('additionalProperties', {}))
            found.extend(_references(child, sub, rules, path + '/' + key))
    elif isinstance(value, list):
        for number, child in enumerate(value):
            found.extend(_references(child, rule.get('items', {}), rules, path + '/' + str(number)))
    yield from dict.fromkeys(found)


def _rows(value, key, fields, path, optional_fields=()):
    """初态是有界明确记录，禁止任意字段覆盖或重复ID最后写入获胜。"""
    if not isinstance(value, list):
        return _fail('InvalidScenarioInitial', path)
    records = {}
    for number, row in enumerate(value):
        if (not isinstance(row, dict) or not set(fields) <= set(row) <= set(fields) | set(optional_fields)
                or not isinstance(row.get(key), str) or not row[key]):
            return _fail('InvalidScenarioInitial', path + '/' + str(number))
        if row[key] in records:
            return _fail('DuplicateScenarioInitial', path + '/' + str(number))
        records[row[key]] = deepcopy(row)
    return {'ok': True, 'rows': {ident: records[ident] for ident in sorted(records)}}


def _resolve_snapshot(catalog, scenario, player_actor_id, path):
    """严格区分显式活动集、定义闭包与初态，所有投影都只写独立候选副本。"""
    rules = _REFERENCE_RULES.get(catalog.get('schema_sha256'))
    if rules is None:
        return _fail('ScenarioReferenceRulesMissing', '/schema_sha256')
    source = catalog['data']
    enabled, authored_initial = scenario['enabled'], scenario['initial']
    tables = rules['tables'] + ['texts', 'flags']
    if not isinstance(enabled, dict) or any(table not in tables for table in enabled):
        return _fail('InvalidScenarioEnabled', path + '/enabled')
    active = {}
    for table in tables:
        ids = enabled.get(table, [])
        if not _ids(ids):
            return _fail('InvalidScenarioEnabled', path + '/enabled/' + table)
        for ident in ids:
            if ident not in source.get(table, {}):
                return _fail('UnknownScenarioEnabled', path + '/enabled/' + table + '/' + ident)
        active[table] = sorted(ids)
    active['scenarios'] = [scenario['id']]
    if not isinstance(authored_initial, dict) or any(
            key not in ('actors', 'offer_ids', *INITIAL_FIELDS) for key in authored_initial):
        return _fail('InvalidScenarioInitial', path + '/initial')
    projected = deepcopy(source)
    normalized = {}
    checked = _rows(authored_initial.get('actors'), 'actor_id', ACTOR_FIELDS,
                    path + '/initial/actors', ('initial_goods',))
    if not checked['ok']:
        return checked
    people = checked['rows']
    for ident, row in people.items():
        location = path + '/initial/actors/' + ident
        if ident not in active['actors']:
            return _fail('ScenarioInitialNotEnabled', location)
        if (row['place_id'] not in active['places'] or type(row['alive']) is not bool
                or type(row['available']) is not bool or not _whole(row['age_years'], 0, 120)
                or any(not isinstance(row[field], dict) for field in ('attributes', 'skills', 'combat', 'appearance_definition'))):
            return _fail('InvalidScenarioActorInitial', location)
        if not _skills_match(row['skills'], scenario.get('skill_profile') == 'full_sixteen_v1'):
            return _fail('InvalidScenarioSkillProfile', location + '/skills')
        goods = row.get('initial_goods', [])
        if not isinstance(goods, list) or len(goods) > 20:
            return _fail('InvalidScenarioActorInitial', location + '/initial_goods')
        checked_goods = _rows(goods, 'item_id', ('item_id', 'quantity'), location + '/initial_goods')
        if not checked_goods['ok']:
            return checked_goods
        for good in checked_goods['rows'].values():
            if not _whole(good['quantity'], 1, 100):
                return _fail('InvalidScenarioActorInitial', location + '/initial_goods/' + good['item_id'] + '/quantity')
        # 新剧本缺省空袋；不能从人物基础定义继承另一年代的初始物资。
        row['initial_goods'] = list(checked_goods['rows'].values())
        actor = projected['actors'][ident]
        for key, target in [('place_id', 'initial_place'), ('age_years', 'initial_age_years'),
                            ('attributes', 'attributes'), ('skills', 'skills'), ('combat', 'combat'),
                            ('appearance_definition', 'appearance_definition'), ('initial_goods', 'initial_goods')]:
            actor[target] = deepcopy(row[key])
    normalized['actors'] = list(people.values())
    player = player_actor_id or scenario['player_actor']
    creating = scenario.get('dynamic_world', {}).get('founder_policy') == 'created_character_v1'
    if creating:
        from .character_creation import check_character_creation
        issues = check_character_creation({'scenarios': [scenario]}, source)
        if issues or player_actor_id or scenario['playable_actor_ids']:
            return _fail('InvalidCharacterCreationCatalog', path + '/character_creation')
    elif scenario['player_actor'] not in scenario['playable_actor_ids'] or player not in scenario['playable_actor_ids']:
        return _fail('InvalidPlayableActor', path + '/playable_actor_ids')
    for ident in scenario['playable_actor_ids']:
        if ident not in people:
            return _fail('MissingScenarioInitial', path + '/initial/actors/' + ident)
        if not people[ident]['alive'] or not people[ident]['available'] or ('travel_plan' in projected['actors'][ident] and scenario.get('dynamic_world', {}).get('founder_policy') not in ('selected_existing_actor_v1', 'selected_existing_actor_roster_v1')):
            return _fail('InvalidPlayableActor', path + '/playable_actor_ids/' + ident)
    for table, fields in INITIAL_FIELDS.items():
        key = 'role_id' if table == 'roles' else 'id'
        checked = _rows(authored_initial.get(table, []), key, fields, path + '/initial/' + table)
        if not checked['ok']:
            return checked
        rows = checked['rows']
        for ident in active[table]:
            if ident not in rows:
                return _fail('MissingScenarioInitial', path + '/initial/' + table + '/' + ident)
        for ident, row in rows.items():
            _order_initial(row, table)
            if ident not in active[table]:
                return _fail('ScenarioInitialNotEnabled', path + '/initial/' + table + '/' + ident)
            if table == 'roles':
                if type(row['enabled']) is not bool or row['actor_id'] not in people:
                    return _fail('MissingScenarioInitial', path + '/initial/roles/' + ident)
                projected[table][ident]['initial_actor'] = row['actor_id']
            else:
                projected[table][ident].update(deepcopy(row))
        normalized[table] = list(rows.values())
    offers = authored_initial.get('offer_ids')
    if not _ids(offers) or any(ident not in active['offers'] for ident in offers):
        return _fail('InvalidScenarioOffers', path + '/initial/offer_ids')
    for ident in active['offers']:
        if ident not in offers:
            return _fail('MissingScenarioInitial', path + '/initial/offer_ids/' + ident)
    normalized['offer_ids'] = sorted(offers)
    if 'world_map_id' in scenario and scenario['world_map_id'] not in active['world_maps']:
        return _fail('ScenarioMapNotEnabled', path + '/world_map_id')
    current = projected['scenarios'][scenario['id']]
    current['player_actor'] = player
    current['playable_actor_ids'] = sorted(scenario['playable_actor_ids'])
    current['enabled'] = deepcopy(active)
    current['initial'] = deepcopy(normalized)
    closure = _closure(projected, active, people, rules)
    if not closure['ok']:
        return closure
    definitions = closure['definitions']
    effective = deepcopy(catalog)
    data = {'required_capabilities': deepcopy(source.get('required_capabilities', []))}
    for table in tables:
        ids = definitions[table] if table in PASSIVE_TABLES or table == 'actors' else active[table]
        data[table] = {ident: deepcopy(projected[table][ident]) for ident in sorted(ids)}
    data['definitions'] = definitions
    data['active_entity_ids'] = active
    data['active_static_actor_ids'] = sorted(people)
    effective['data'] = data
    normalized.update(legacy=False, player_actor_id=player, start_date=deepcopy(scenario['start_date']))
    return {'ok': True, 'catalog': effective, 'initial': normalized}


def _closure(data, active, people, rules):
    """引用闭包仅带回定义；原型的旧地点依赖不能变成当前世界的活动地点或资产。"""
    definitions = {table: {} for table in active}
    queue = [(table, ident, table != 'actors' or ident in people)
             for table in active for ident in active[table]]
    visited = set()
    while queue:
        table, ident, live = queue.pop(0)
        key = (table, ident, live)
        if key in visited:
            continue
        visited.add(key)
        if ident not in data.get(table, {}):
            return _fail('UnknownScenarioReference', '/' + table + '/' + ident)
        record = data[table][ident]
        definitions[table][ident] = deepcopy(record)
        if table in ('texts', 'flags'):
            continue
        for target_table, target_id, edge in _references(record, rules['rules'][table], rules):
            if target_table not in definitions or target_id not in data.get(target_table, {}):
                return _fail('UnknownScenarioReference', '/' + table + '/' + ident + edge)
            next_live = live and (target_table != 'actors' or target_id in people)
            if live and target_table not in (*PASSIVE_TABLES, 'actors') and target_id not in active[target_table]:
                return _fail('ScenarioDependencyNotEnabled', '/' + table + '/' + ident + edge)
            queue.append((target_table, target_id, next_live))
    return {'ok': True, 'definitions': {table: {ident: definitions[table][ident]
            for ident in sorted(definitions[table])} for table in definitions}}



def _order_initial(row, table):
    """仅排序已知无序初态记录，保留对白选项、路线、候选外貌和计划步骤的作者顺序。"""
    if table == 'inventories':
        row['goods'].sort(key=lambda good: good['item_id'])
    elif table == 'garrisons':
        row['units'].sort(key=lambda unit: unit['id'])
        for unit in row['units']:
            unit['equipment'].sort(key=lambda good: good['item_id'])
    elif table == 'estates':
        row['resident_actor_ids'].sort()
        row['initial_duties'].sort(key=lambda duty: duty['actor_id'])
