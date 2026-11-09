"""普通技艺授课的具名学生条款；当前节点与教师接受仍由同一原生世界证明。"""
from .scenarios import BASE_SKILLS, EXTENDED_SKILLS

PROFILE = 'authored_study_teaching_requests_v1'
ID_FIELDS = ('id', 'teacher_actor_id', 'student_actor_id', 'skill_id', 'course_role_id', 'dialogue_id', 'node_id')
FIELDS = set(ID_FIELDS) | {'duration', 'payment_policy', 'tariff'}


def _id(value):
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def _shape(row):
    return (isinstance(row, dict) and set(row) == FIELDS
            and all(_id(row[key]) for key in ID_FIELDS)
            and type(row['duration']) is int
            and isinstance(row['payment_policy'], str) and isinstance(row['tariff'], str))


def check_study_teaching_requests(pack, index):
    """只激活所选声明，原费用由实际学生等级与原课程决定，不在作者叶重算。"""
    issues = []
    tables = {name: dict(index.get(name, {})) for name in ('actors', 'roles', 'dialogues', 'study_teaching_requests')}
    for name in ('actors', 'roles', 'dialogues'):
        for row in pack.get(name, []):
            if isinstance(row, dict) and _id(row.get('id')):
                tables[name][row['id']] = row

    def issue(code, path, message):
        issues.append((code, path, message))

    def definition(row, path):
        if not _shape(row):
            issue('STUDY_TEACHING_FIELDS', path, '须指明真实双方、技能、教师岗位、请求节点、整数课时与支付条款')
            return None
        if row['teacher_actor_id'] == row['student_actor_id'] or any(row[key] not in tables['actors'] for key in ('teacher_actor_id', 'student_actor_id')):
            issue('STUDY_TEACHING_PARTIES', path, '师生须为不同的真实作者人物')
        if not 2 <= row['duration'] <= 6:
            issue('STUDY_TEACHING_DURATION', path + ('duration',), '普通课程须为2至6半日')
        if row['payment_policy'] != 'student_personal_tuition_v1' or row['tariff'] != 'ordinary_next_level_v1':
            issue('STUDY_TEACHING_PAYMENT', path, '学生须明确请求按原下一技艺级公式支付本人课程费')
        if row['skill_id'] not in BASE_SKILLS + EXTENDED_SKILLS:
            issue('STUDY_TEACHING_SKILL', path + ('skill_id',), '所学须为正式十六技艺之一')
        course_role = tables['roles'].get(row['course_role_id'], {})
        matching = [course for course in course_role.get('skill_courses', []) if isinstance(course, dict) and course.get('skill_id') == row['skill_id']]
        if 'study_skill' not in course_role.get('services', []) or len(matching) != 1 or matching[0].get('mode') != 'teacher':
            issue('STUDY_TEACHING_COURSE', path + ('course_role_id',), '须绑定真实教师课程，设施模式不能代替教师资格')
        dialogue = tables['dialogues'].get(row['dialogue_id'], {})
        request_role = tables['roles'].get(dialogue.get('context_role'))
        if not request_role or dialogue.get('context_role') == row['course_role_id']:
            issue('STUDY_TEACHING_DIALOGUE', path + ('dialogue_id',), '学生请求须使用独立且明确的普通当面岗位')
            return None
        nodes = [node for node in dialogue.get('nodes', []) if isinstance(node, dict) and node.get('id') == row['node_id']]
        if len(nodes) != 1 or nodes[0].get('kind') not in ('say', 'choice'):
            issue('STUDY_TEACHING_NODE', path + ('node_id',), '本版明确请求绑定实际说话或选择节点，接受使用独立课程报价')
        if request_role.get('place_id') != course_role.get('place_id'):
            issue('STUDY_TEACHING_PLACE', path + ('course_role_id',), '学生当面请求与实际教师课程须位于同一场所')
        return dialogue.get('context_role')

    declared = set()
    for number, row in enumerate(pack.get('study_teaching_requests', [])):
        path = ('study_teaching_requests', number)
        definition(row, path)
        if isinstance(row, dict) and _id(row.get('id')):
            if row['id'] in declared:
                issue('STUDY_TEACHING_IDS', path, '请求ID不能重复')
            declared.add(row['id'])
            tables['study_teaching_requests'][row['id']] = row

    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'study_teaching_requests' not in scenario:
            continue
        config = scenario['study_teaching_requests']
        path = ('scenarios', number, 'study_teaching_requests')
        if not isinstance(config, dict) or set(config) != {'profile', 'request_ids'} or config.get('profile') != PROFILE:
            issue('STUDY_TEACHING_PROFILE', path, '须显式选择普通授课请求政策及名单')
            continue
        if not isinstance(scenario.get('dynamic_world'), dict):
            issue('STUDY_TEACHING_NATIVE_PROFILE', path, '普通授课须有实际原生动态世界profile')
        selected = config['request_ids']
        if not isinstance(selected, list) or not 1 <= len(selected) <= 4096 or not all(_id(value) for value in selected) or len(selected) != len(set(selected)):
            issue('STUDY_TEACHING_IDS', path + ('request_ids',), '须选择1至4096个不同的稳定请求ID')
            continue
        enabled = scenario.get('enabled', {})
        actual = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict)}
        allowed = BASE_SKILLS + (EXTENDED_SKILLS if scenario.get('skill_profile') == 'full_sixteen_v1' else ())
        bindings = set()
        for request_id in selected:
            row = tables['study_teaching_requests'].get(request_id)
            row_path = path + ('request_ids', request_id)
            if row is None:
                issue('STUDY_TEACHING_IDS', row_path, '缺少所选请求定义')
                continue
            request_role = definition(row, row_path)
            if not _shape(row):
                continue
            if row['id'] != request_id:
                issue('STUDY_TEACHING_IDS', row_path, '请求表键与稳定ID不一致')
            if not {row['teacher_actor_id'], row['student_actor_id']} <= set(enabled.get('actors', [])) & actual:
                issue('STUDY_TEACHING_ACTORS', row_path, '师生须在本情景真实启用且具有初态')
            if row['skill_id'] not in allowed:
                issue('STUDY_TEACHING_SKILL_PROFILE', row_path, '所选技艺须已由本情景真实技能profile启用')
            if any(value not in enabled.get('roles', []) for value in (request_role, row['course_role_id']) if value):
                issue('STUDY_TEACHING_ROLE', row_path, '学生请求岗位与教师课程岗位均须启用')
            if row['dialogue_id'] not in enabled.get('dialogues', []):
                issue('STUDY_TEACHING_DIALOGUE_DISABLED', row_path, '所选对白须真实启用')
            binding = tuple(row[key] for key in ('teacher_actor_id', 'student_actor_id', 'skill_id', 'dialogue_id', 'node_id', 'duration'))
            if binding in bindings:
                issue('STUDY_TEACHING_BINDING', row_path, '同一节点和有限课时不能产生重叠请求')
            bindings.add(binding)
    return issues
