"""显式NPC本人赴原院接册；意图不授职、不代公共责任批准、不释放原通缉。"""

def check_political_shop_registry_intents(pack, index):
    issues = []
    actors = {row.get('id') for row in pack.get('actors', []) if isinstance(row, dict)}
    indexed = index.get('actors', {}) if isinstance(index, dict) else {}
    if isinstance(indexed, dict):
        actors.update(indexed)
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        director = dynamic.get('director', {})
        config = director.get('political_shop_registry_intents')
        policies = director.get('policies', director.get('subjects', {}))
        selected = [(key, row) for key, row in policies.items()
                    if 'shop_registry_acceptance' in row.get('templates', []) or 'shop_registry_intent_id' in row]
        if config is None and not selected:
            continue
        root = ('scenarios', number, 'dynamic_world', 'director', 'political_shop_registry_intents')
        def issue(code, message):
            issues.append((code, root, message))
        if (not isinstance(config, dict) or set(config) != {'version', 'intents'}
                or type(config.get('version')) is not int or config['version'] != 1
                or not isinstance(config.get('intents'), dict) or not 1 <= len(config['intents']) <= 64):
            issue('SHOP_REGISTRY_INTENT_PROFILE', '接册意图须为显式1版有界目录')
            continue
        registries = dynamic.get('political_shop_registry_successions', {})
        if (dynamic.get('profile') != 'local_society_v1'
                or registries.get('enabled') is not True or registries.get('version') != 1
                or dynamic.get('political_takeover', {}).get('enabled') is not True
                or not dynamic.get('political_shop_licensing') or not dynamic.get('private_shops')
                or not {'political_shop_registry_intents_v1', 'political_shop_registry_successions_v1'} <= set(pack.get('required_capabilities', []))):
            issue('SHOP_REGISTRY_INTENT_DEPENDENCIES', '须显式原接册、法律、首店、接管及独立NPC意图能力')
        registry_policies = registries.get('policies', {})
        seen = set()
        for key, intent in config['intents'].items():
            if (not isinstance(intent, dict) or set(intent) != {'id', 'actor_id', 'registry_policy_id'}
                    or any(not isinstance(v, str) or not v or len(v) > 4096 for v in intent.values())
                    or intent['id'] != key):
                issue('SHOP_REGISTRY_INTENT_SOURCE', '意图仅含同ID、原人物和接册政策')
                continue
            target = registry_policies.get(intent['registry_policy_id'], {})
            if not target or intent['actor_id'] not in actors:
                issue('SHOP_REGISTRY_INTENT_SOURCE', '须引用本情景真实人物及原法院政策')
            scope = (intent['actor_id'], target.get('court_id'))
            if scope in seen:
                issue('SHOP_REGISTRY_INTENT_DUPLICATE', '同一人物与同一法院不得声明竞争意图')
            seen.add(scope)
        used = set()
        for key, policy in selected:
            intent_id = policy.get('shop_registry_intent_id')
            intent = config['intents'].get(intent_id, {})
            target = registry_policies.get(intent.get('registry_policy_id'), {})
            if ('shop_registry_acceptance' not in policy.get('templates', []) or not intent
                    or policy.get('organization_id') != target.get('organization_id')
                    or (director.get('registry_version') is None and key != intent.get('actor_id'))):
                issue('SHOP_REGISTRY_INTENT_POLICY', '策略须选择同机构的具名本人接册意图')
            used.add(intent_id)
        if not selected or used != set(config['intents']):
            issue('SHOP_REGISTRY_INTENT_UNUSED', '每项意图须由实际Director策略采用')
    return issues
