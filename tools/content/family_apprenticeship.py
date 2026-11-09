"""成年师承作者来源：固定本人私人岗位的真实发言，不预设关系、技能或继任。"""
from .scenarios import BASE_SKILLS, EXTENDED_SKILLS


def check_family_apprenticeship(pack, index):
    """检查独立策略、有限原技艺及准确说话者；实际双方决定留给原生同候选办理。"""
    issues = []
    tables = {}
    for name in ('actors', 'roles', 'dialogues'):
        supplied = index.get(name, {})
        tables[name] = dict(supplied) if isinstance(supplied, dict) else {row['id']: row for row in supplied if isinstance(row, dict) and isinstance(row.get('id'), str)}
        for row in pack.get(name, []):
            if isinstance(row, dict) and isinstance(row.get('id'), str):
                tables[name][row['id']] = row
    capabilities = set(pack.get('required_capabilities', []))
    required = {'family', 'family_apprenticeship_v1', 'family_life_v1', 'native_dynamic_world', 'dialogue', 'cultivation'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_apprenticeship' not in scenario:
            continue
        skills = BASE_SKILLS + (EXTENDED_SKILLS if scenario.get('skill_profile') == 'full_sixteen_v1' else ())
        path = ('scenarios', number, 'family_apprenticeship')
        def issue(code, text):
            """本情景策略错误带准确路径，不替作者补同意或默认身份。"""
            issues.append((code, path, text))
        config = scenario['family_apprenticeship']
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'statements'}:
            issue('APPRENTICESHIP_FIELDS', '只接受明确来源配置，不接受种子关系、自动经验或继任许可')
            continue
        if type(config['version']) is not int or config['version'] != 1 or config['profile'] != 'adult_personal_apprenticeship_v1':
            issue('APPRENTICESHIP_PROFILE', '须明确选成年本人师承版本1')
        if not required <= capabilities:
            issue('APPRENTICESHIP_CAPABILITY', '须明确原生命、原课程和成年师承完整能力')
        if scenario.get('family_life') != {'profile': 'verified_military_deaths_v1', 'player_death': 'await_explicit_continuation'}:
            issue('APPRENTICESHIP_LIFE', '本人决定须借真实原死亡来源')
        statements = config['statements']
        if not isinstance(statements, dict) or not 1 <= len(statements) <= 4096:
            issue('APPRENTICESHIP_STATEMENTS', '意愿表须非空且有限')
            continue
        bindings = []
        for key, row in statements.items():
            required_fields = {'id', 'speaker_actor_id', 'speaker_side', 'skill_id', 'dialogue_id', 'node_id', 'terms_version'}
            if not isinstance(row, dict) or not required_fields <= set(row) <= required_fields | {'listener_actor_id'}:
                issue('APPRENTICESHIP_STATEMENT_FIELDS', '条款只限本人、师生位置、原节点和一项技艺')
                continue
            if any(not isinstance(row[k], str) or not 1 <= len(row[k]) <= 256 for k in set(row) - {'terms_version'}):
                issue('APPRENTICESHIP_REFERENCE', '人物与节点引用须明确有限')
                continue
            if row['id'] != key or type(row['terms_version']) is not int or row['terms_version'] != 1 or row['speaker_side'] not in {'teacher', 'student'} or row['skill_id'] not in skills:
                issue('APPRENTICESHIP_STATEMENT', '技艺须原成长目录且不能猜测版本或师生身份')
            speaker, listener = row['speaker_actor_id'], row.get('listener_actor_id')
            if speaker == listener:
                issue('APPRENTICESHIP_PARTICIPANTS', '两位真实人物必须不同')
            for old in bindings:
                if old[:3] == (speaker, row['dialogue_id'], row['node_id']) and (old[3] is None or listener is None or old[3] == listener):
                    issue('APPRENTICESHIP_AMBIGUOUS_SOURCE', '泛听者与定向条款不能重叠')
            bindings.append((speaker, row['dialogue_id'], row['node_id'], listener))
            dialogue = tables['dialogues'].get(row['dialogue_id'])
            node = next((n for n in dialogue.get('nodes', []) if n.get('id') == row['node_id']), None) if dialogue else None
            role = tables['roles'].get(dialogue.get('context_role')) if dialogue else None
            if not dialogue or not node or node.get('kind') != 'say' or node.get('speaker') != '$provider' or not role or role.get('initial_actor') != speaker or role.get('organization_id'):
                issue('APPRENTICESHIP_PERSONAL_SAY', '必须由原私人岗位本人在实际say节点表示，公职或选择题不能代签')
            if index and (speaker not in tables['actors'] or (listener is not None and listener not in tables['actors'])):
                issue('APPRENTICESHIP_ACTOR', '指定人物必须来自正规原目录')
    return issues
