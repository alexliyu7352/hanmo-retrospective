"""弟子控制只选择原本人许可和师承关系；没有种子同意、死亡、资产或另一本控制账。"""
from .scenarios import BASE_SKILLS, EXTENDED_SKILLS


def check_family_disciple_legacy(pack, index):
    """原目录、私人说话者、原双边关系配置和精确许可引用必须共同存在。"""
    issues = []
    tables = {key: dict(index.get(key, {})) for key in ('roles', 'dialogues', 'legacy_control_consent_requests')}
    for key, table in tables.items():
        for row in pack.get(key, []):
            if isinstance(row, dict) and isinstance(row.get('id'), str):
                table[row['id']] = row
    required = {'family', 'family_life_v1', 'family_legacy_v1', 'family_apprenticeship_v1', 'family_disciple_legacy_v1', 'native_dynamic_world'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_disciple_legacy' not in scenario:
            continue
        path = ('scenarios', number, 'family_disciple_legacy')
        def issue(code, message):
            """给原编辑器准确路径，不把不可达许可补成自动接受。"""
            issues.append((code, path, message))
        config = scenario['family_disciple_legacy']
        if not isinstance(config, dict) or set(config) != {'version', 'profile', 'requests'}:
            issue('DISCIPLE_CONTROL_FIELDS', '只接受原请求选择，不接受预设同意、死亡或财产权')
            continue
        if type(config['version']) is not int or config['version'] != 1 or config['profile'] != 'accepted_disciple_posthumous_control_v1':
            issue('DISCIPLE_CONTROL_PROFILE', '须明确选择弟子本人死后条件控制版本1')
        legacy_version2 = isinstance(scenario.get('family_legacy'), dict) and scenario['family_legacy'].get('profile') == 'posthumous_control_sources_v2' and isinstance(scenario.get('family_biological_legacy'), dict)
        selected_required = (required - {'family_legacy_v1'}) | {'family_legacy_v2'} if legacy_version2 else required
        if not selected_required <= set(pack.get('required_capabilities', [])):
            issue('DISCIPLE_CONTROL_CAPABILITIES', '原生命、师承、条件许可及弟子完整能力必须齐全')
        legacy = scenario.get('family_legacy', {})
        if not isinstance(legacy, dict):
            legacy = {}
        apprentice = scenario.get('family_apprenticeship', {})
        if not isinstance(apprentice, dict):
            apprentice = {}
        if legacy.get('profile') != ('posthumous_control_sources_v2' if legacy_version2 else 'authored_posthumous_control_consent_v1') or apprentice.get('profile') != 'adult_personal_apprenticeship_v1' or not isinstance(scenario.get('dynamic_world'), dict):
            issue('DISCIPLE_CONTROL_ORIGINAL_PROFILES', '须依原动态世界、本人条件许可和真实师承')
        rows = config['requests']
        if not isinstance(rows, dict) or not 1 <= len(rows) <= 4096:
            issue('DISCIPLE_CONTROL_REQUESTS', '须选择有限原请求')
            continue
        skills = BASE_SKILLS + (EXTENDED_SKILLS if scenario.get('skill_profile') == 'full_sixteen_v1' else ())
        for key, row in rows.items():
            if not isinstance(row, dict) or set(row) != {'request_id', 'skill_id'} or row.get('request_id') != key or row.get('skill_id') not in skills:
                issue('DISCIPLE_CONTROL_REQUEST_FIELDS', '请求索引和原技艺须精确匹配，不附带资产范围')
                continue
            original = tables['legacy_control_consent_requests'].get(key)
            if key not in legacy.get('request_ids', []) or not isinstance(original, dict):
                issue('DISCIPLE_CONTROL_ORIGINAL_REQUEST', '须引用同情景已选择的原本人条件请求')
                continue
            teacher, student = original.get('predecessor_actor_id'), original.get('successor_actor_id')
            if teacher == student:
                issue('DISCIPLE_CONTROL_PARTIES', '师生须两位真实人物')
            talk = tables['dialogues'].get(original.get('dialogue_id'), {})
            role = tables['roles'].get(talk.get('context_role'), {})
            if role.get('initial_actor') != student or role.get('organization_id'):
                issue('DISCIPLE_CONTROL_PERSONAL_SPEAKER', '接受者必须在自己的私人岗位亲自陈述')
            for field in ('accept_node_id', 'revoke_node_id'):
                nodes = [n for n in talk.get('nodes', []) if n.get('id') == original.get(field)]
                if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') != '$provider':
                    issue('DISCIPLE_CONTROL_PERSONAL_SAY', '接受与撤回都必须由候选本人实际say，不以玩家选择代签')
            possible = False
            for statement in apprentice.get('statements', {}).values():
                if not isinstance(statement, dict) or statement.get('skill_id') != row['skill_id']:
                    continue
                side = statement.get('speaker_side')
                speaker, listener = (teacher, student) if side == 'teacher' else (student, teacher)
                if side in ('teacher', 'student') and statement.get('speaker_actor_id') == speaker and statement.get('listener_actor_id') in (None, listener):
                    possible = True
            if not possible:
                issue('DISCIPLE_CONTROL_ORIGINAL_RELATION', '须另有真实双方同技艺的师承办理来源，不能把条件许可当拜师')
    return issues
