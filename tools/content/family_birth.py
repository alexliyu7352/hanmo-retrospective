"""独立生育、原始谱系与有限照护作者门；不接受任何已生成儿童或已办理同意的种子。"""

SKILLS = {'infantry', 'cavalry', 'archery', 'martial', 'tactics', 'debate', 'negotiation', 'rites', 'commerce', 'transport', 'naval', 'espionage', 'fortification', 'agriculture', 'medicine', 'forging'}


def check_family_birth(pack, index):
    """结构、真实节点与原型资格分别核验；实际本人接受、预约及出生由原生事务负责。"""
    issues = []
    tables = {}
    for name in ('actors', 'roles', 'dialogues'):
        given = index.get(name, {})
        tables[name] = dict(given) if isinstance(given, dict) else {v['id']: v for v in given if isinstance(v, dict) and isinstance(v.get('id'), str)}
        tables[name].update({v['id']: v for v in pack.get(name, []) if isinstance(v, dict) and isinstance(v.get('id'), str)})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if not ('family_birth' in scenario or 'family_lineage' in scenario):
            continue
        path = ('scenarios', number, 'family_birth')
        def issue(code, detail):
            """保留具体作者路径；未知责任不改成允许默认值。"""
            issues.append((code, path, detail))
        birth, lineage = scenario.get('family_birth'), scenario.get('family_lineage')
        required = {'family', 'family_birth_v1', 'family_life_v1', 'native_dynamic_world', 'dialogue'}
        if not required <= set(pack.get('required_capabilities', [])) or scenario.get('skill_profile') != 'full_sixteen_v1' or scenario.get('family_life') != {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}:
            issue('BIRTH_PROFILE', '须显式启用生育、16技艺及原生命来源')
        if not isinstance(birth, dict) or set(birth) != {'version', 'profile', 'arrangements'} or type(birth.get('version')) is not int or birth['version'] != 1 or birth.get('profile') != 'adult_birth_personal_care_v1':
            issue('BIRTH_FIELDS', '生育策略不接受种子儿童、同意、怀孕或资产转移'); continue
        if not isinstance(lineage, dict) or set(lineage) != {'version', 'profile', 'origins', 'original_innate_levels', 'endowment'} or type(lineage.get('version')) is not int or lineage['version'] != 1 or lineage.get('profile') != 'biological_origin_innate_v1':
            issue('LINEAGE_FIELDS', '谱系仅接受独立原始亲代与有界禀赋策略'); continue
        enabled = set(scenario.get('enabled', {}).get('actors', []))
        origins = lineage['origins']
        if not isinstance(origins, dict) or len(origins) > 4096:
            issue('LINEAGE_ORIGINS', '原始谱系须为有限表'); continue
        graph = {}
        for actor, origin in origins.items():
            if not isinstance(origin, dict) or set(origin) != {'actor_id', 'parent_actor_ids'} or origin.get('actor_id') != actor or actor not in enabled or not isinstance(origin.get('parent_actor_ids'), list):
                issue('LINEAGE_ORIGIN', '亲代记录须指向本局实际原人物'); continue
            parents = origin['parent_actor_ids']
            if any(not isinstance(p, str) for p in parents) or len(parents) > 2 or len(set(parents)) != len(parents) or actor in parents or not set(parents) <= enabled:
                issue('LINEAGE_PARENTS', '原亲代最多两人，缺失保持未知，不能自指或伪造'); continue
            graph[actor] = parents
        # 有限显式栈拒循环，不随恶意谱系深度递归Python栈。
        done = set()
        for actor in graph:
            stack = [(actor, False)]; visiting = set()
            while stack:
                node, leave = stack.pop()
                if leave:
                    visiting.discard(node); done.add(node); continue
                if node in done: continue
                if node in visiting:
                    issue('LINEAGE_CYCLE', '原亲代谱系不能成环'); break
                visiting.add(node); stack.append((node, True))
                stack.extend((p, False) for p in graph.get(node, []))
        endowment = lineage['endowment']
        if not isinstance(endowment, dict) or set(endowment) != {'version', 'unweighted', 'single', 'different', 'same', 'families'} or endowment.get('version') != 'innate_birth_v1':
            issue('INNATE_FIELDS', '先天策略须明确原版本和各亲代组合概率'); continue
        values = endowment['unweighted']
        if not isinstance(values, list) or len(values) != 4 or any(type(v) is not int or v < 0 or v > 10000 for v in values) or sum(values) != 10000:
            issue('INNATE_DISTRIBUTION', '四档概率必须完整合计10000')
        for branch in ('single', 'different', 'same'):
            values = endowment[branch]
            if not isinstance(values, dict) or set(values) != {'absent', 'retained', 'strengthened'} or any(type(v) is not int or not 0 <= v <= 10000 for v in values.values()) or sum(values.values()) != 10000:
                issue('INNATE_WEIGHTS', '亲代加权概率须完整且合计10000')
        families = endowment['families']
        if not isinstance(families, dict) or not 1 <= len(families) <= 16:
            issue('INNATE_FAMILIES', '先天家系须为1至16个具名有限定义'); continue
        for key, family in families.items():
            if not isinstance(family, dict) or set(family) != {'id', 'learning_skill_ids', 'learning_bonus_bp'} or family.get('id') != key:
                issue('INNATE_FAMILY', '禀赋只定义真实学习潜力，不授后天技艺'); continue
            skills, bonus = family['learning_skill_ids'], family['learning_bonus_bp']
            if not isinstance(skills, list) or not skills or any(not isinstance(v, str) for v in skills) or len(skills) != len(set(skills)) or not set(skills) <= SKILLS:
                issue('INNATE_SKILLS', '先天效果只可引用本局16技艺')
            if not isinstance(bonus, list) or len(bonus) != 4 or any(type(v) is not int or not 0 <= v <= 1500 for v in bonus) or bonus[0] != 0 or bonus != sorted(bonus):
                issue('INNATE_BONUS', '学习增益须单调且最多15%，不赋予成品技能')
        original = lineage['original_innate_levels']
        if not isinstance(original, dict) or len(original) > 4096:
            issue('INNATE_ORIGINAL', '原先天记录须有限且保持未知'); continue
        for actor, levels in original.items():
            if actor not in enabled or not isinstance(levels, dict) or not set(levels) <= set(families) or any(type(v) is not int or not 0 <= v <= 3 for v in levels.values()):
                issue('INNATE_ORIGINAL', '原人物先天档位只能来自显式作者来源')
        arrangements = birth['arrangements']
        if not isinstance(arrangements, dict) or not 1 <= len(arrangements) <= 4096:
            issue('BIRTH_ARRANGEMENTS', '本人意愿须为有限非空表'); continue
        bindings = set()
        fields = {'id', 'speaker_actor_id', 'listener_actor_id', 'dialogue_id', 'node_id', 'terms_version', 'scope', 'terms'}
        term_fields = {'gestating_actor_id', 'caregiver_actor_id', 'payer_actor_id', 'place_id', 'source_inventory_id', 'food_item_id', 'child_prototype_id', 'birth_duration_ticks', 'care_period_ticks', 'food_per_period', 'prepaid_care_periods'}
        for key, row in arrangements.items():
            if not isinstance(row, dict) or set(row) != fields or row.get('id') != key or type(row.get('terms_version')) is not int or row['terms_version'] != 1 or row.get('scope') != 'biological_birth_and_finite_care':
                issue('BIRTH_ARRANGEMENT_FIELDS', '独立本人意愿不能附加控制、婚姻或财物处分'); continue
            if any(not isinstance(row[k], str) or not 1 <= len(row[k]) <= 256 for k in fields - {'terms_version', 'terms'}):
                issue('BIRTH_REFERENCE', '双方与节点须为具名引用'); continue
            first, second = row['speaker_actor_id'], row['listener_actor_id']
            if first == second or not {first, second} <= enabled:
                issue('BIRTH_PERSONS', '生育条款只能绑定两位真实原人物')
            terms = row['terms']
            if not isinstance(terms, dict) or set(terms) != term_fields:
                issue('BIRTH_TERMS', '照护人、本人私粮、原型和有限时间均须明确'); continue
            if any(not isinstance(terms[k], str) or not 1 <= len(terms[k]) <= 256 for k in term_fields - {'birth_duration_ticks', 'care_period_ticks', 'food_per_period', 'prepaid_care_periods'}):
                issue('BIRTH_TERM_REFERENCE', '原照护与原型引用须为稳定字符串'); continue
            if terms['gestating_actor_id'] not in {first, second} or terms['caregiver_actor_id'] not in {first, second} or terms['payer_actor_id'] != second or terms['source_inventory_id'] != 'bag:' + second:
                issue('BIRTH_PERSONAL_CARE', '只支持当事人明确承担的照护和原私人行囊')
            for name, maximum in [('birth_duration_ticks', 1440), ('care_period_ticks', 720), ('food_per_period', 10000), ('prepaid_care_periods', 360)]:
                if type(terms[name]) is not int or not 1 <= terms[name] <= maximum:
                    issue('BIRTH_DURATION', '生育及照护必须使用有界正整数')
            binding = (first, second, row['dialogue_id'], row['node_id'])
            if binding in bindings: issue('BIRTH_DUPLICATE_NODE', '同一节点不能重绑定不同生育条款')
            bindings.add(binding)
            if index:
                dialogue = tables['dialogues'].get(row['dialogue_id'], {})
                node = next((n for n in dialogue.get('nodes', []) if n.get('id') == row['node_id']), {})
                role = tables['roles'].get(dialogue.get('context_role'), {})
                if node.get('kind') not in {'say', 'choice'} or role.get('place_id') != terms['place_id']:
                    issue('BIRTH_NODE', '独立意愿必须来自原照护地点的真实普通节点')
                prototype = tables['actors'].get(terms['child_prototype_id'], {})
                if set(prototype.get('skills', {})) != SKILLS or any(v != 0 for v in prototype.get('skills', {}).values()):
                    issue('BIRTH_PROTOTYPE', '出生原型必须具有完整16技艺的零后天经验')
    return issues
