"""已出生儿童的有限志愿接手作者门；仅声明真实提供者节点，不预造孩子、同意或已完成照护。"""


def check_child_care_relief(pack, index):
    """明确本人私粮、有限期和原节点；原父母与孩子在运行时沿唯一出生来源选择。"""
    issues = []
    tables = {}
    for name in ('roles', 'dialogues'):
        raw = index.get(name, {})
        tables[name] = dict(raw) if isinstance(raw, dict) else {r['id']: r for r in raw if isinstance(r, dict) and isinstance(r.get('id'), str)}
        tables[name].update({r['id']: r for r in pack.get(name, []) if isinstance(r, dict) and isinstance(r.get('id'), str)})
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_care_relief' not in scenario:
            continue
        path = ('scenarios', number, 'family_care_relief')
        def issue(code, detail):
            """坏引用给具体作者诊断，不自动降级同意或资源门。"""
            issues.append((code, path, detail))
        config = scenario['family_care_relief']
        required = {'family', 'family_birth_v1', 'family_life_v1', 'family_care_relief_v1', 'native_dynamic_world', 'dialogue'}
        if not required <= set(pack.get('required_capabilities', [])) or not isinstance(scenario.get('family_birth'), dict) or not isinstance(scenario.get('family_lineage'), dict) or not isinstance(scenario.get('family_life'), dict) or not isinstance(scenario.get('dynamic_world'), dict):
            issue('CARE_RELIEF_DEPENDENCIES', '需要本局真实出生、谱系、生命史与本人对白')
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'offers'} or type(config.get('version')) is not int or config['version'] != 1 or config.get('profile') != 'living_parent_private_relief_v1':
            issue('CARE_RELIEF_FIELDS', '策略不能写入已有儿童、同意、余额或照护完成'); continue
        offers = config['offers']
        if not isinstance(offers, dict) or not 1 <= len(offers) <= 4096:
            issue('CARE_RELIEF_OFFERS', '原本人意愿须为非空有限表'); continue
        enabled = set(scenario.get('enabled', {}).get('actors', []))
        fields = {'id', 'provider_actor_id', 'dialogue_id', 'node_id', 'terms_version', 'scope', 'terms'}
        term_fields = {'place_id', 'source_inventory_id', 'food_item_id', 'period_ticks', 'food_per_period', 'prepaid_periods'}
        bound = set()
        for key, row in offers.items():
            if not isinstance(row, dict) or set(row) != fields or row.get('id') != key or type(row.get('terms_version')) is not int or row['terms_version'] != 1 or row.get('scope') != 'voluntary_finite_care_own_food':
                issue('CARE_RELIEF_OFFER_FIELDS', '只允许本人可撤回的有限志愿照护，不附加监护或原粮处分'); continue
            if any(not isinstance(row[k], str) or not 1 <= len(row[k]) <= 256 for k in fields - {'terms_version', 'terms'}):
                issue('CARE_RELIEF_REFERENCE', '提供者和原节点必须是具名引用'); continue
            provider = row['provider_actor_id']
            if provider not in enabled:
                issue('CARE_RELIEF_PROVIDER', '提供者须是本局真实原人物')
            terms = row['terms']
            if not isinstance(terms, dict) or set(terms) != term_fields:
                issue('CARE_RELIEF_TERMS', '地点、本人粮源和有限工期须完整给出'); continue
            if any(not isinstance(terms[k], str) or not 1 <= len(terms[k]) <= 256 for k in ('place_id', 'source_inventory_id', 'food_item_id')):
                issue('CARE_RELIEF_TERM_REFERENCE', '粮源不能是默认家庭、公库或空引用'); continue
            if terms['source_inventory_id'] != 'bag:' + provider:
                issue('CARE_RELIEF_PRIVATE_FOOD', '只支持提供者本人原私人行囊，不借原出生预约或死者财物')
            for name, maximum in [('period_ticks', 720), ('food_per_period', 10000), ('prepaid_periods', 360)]:
                if type(terms[name]) is not int or not 1 <= terms[name] <= maximum:
                    issue('CARE_RELIEF_DURATION', '志愿工期与私粮须为有界正整数')
            binding = (provider, row['dialogue_id'], row['node_id'])
            if binding in bound:
                issue('CARE_RELIEF_DUPLICATE_NODE', '同一个本人节点不能重绑定另一份条款')
            bound.add(binding)
            if index:
                dialogue = tables['dialogues'].get(row['dialogue_id'], {})
                node = next((n for n in dialogue.get('nodes', []) if n.get('id') == row['node_id']), {})
                role = tables['roles'].get(dialogue.get('context_role'), {})
                if node.get('kind') not in {'say', 'choice'} or role.get('place_id') != terms['place_id'] or row['dialogue_id'] not in scenario.get('enabled', {}).get('dialogues', []) or dialogue.get('context_role') not in scenario.get('enabled', {}).get('roles', []):
                    issue('CARE_RELIEF_NODE', '须为本局选定、实际同场的独立本人节点')
    return issues
