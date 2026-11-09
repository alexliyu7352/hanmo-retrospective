"""具名邀学的作者合同；实际到达节点与教师接受仍须原生世界核验。"""

FIELDS = {'id', 'teacher_actor_id', 'student_actor_id', 'skill_id', 'dialogue_id', 'node_id'}
PROFILE = 'authored_teaching_requests_v1'


def _id(value):
    """沿共同稳定ID上限，避免作者与原生边界对中文字节数理解不同。"""
    return isinstance(value, str) and 0 < len(value.encode('utf-8')) <= 256


def check_teaching_requests(pack, index):
    """只激活场景明确选中的求教来源；目录条目本身不形成同意或教学事实。"""
    issues = []
    definitions = dict(index.get('teaching_requests', {}))
    actors, skills = index.get('actors', {}), index.get('personal_skills', {})
    dialogues, roles = index.get('dialogues', {}), index.get('roles', {})
    for table, values in (('actors', actors), ('personal_skills', skills), ('dialogues', dialogues), ('roles', roles)):
        for row in pack.get(table, []):
            if isinstance(row, dict) and _id(row.get('id')):
                values = dict(values)
                values[row['id']] = row
        if table == 'actors': actors = values
        elif table == 'personal_skills': skills = values
        elif table == 'dialogues': dialogues = values
        else: roles = values

    def issue(code, path, message):
        """返回现有作者诊断格式，保留稳定错误码与具体来源位置。"""
        issues.append((code, path, message))

    def definition(value, path):
        """作者声明必须具体绑定双方、本领和节点，不能以台词或自动接受开关代替。"""
        if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in FIELDS):
            issue('TEACHING_REQUEST_FIELDS', path, '邀学须完整指明请求ID、教师、学生、本领和对白节点')
            return None
        teacher, student = value['teacher_actor_id'], value['student_actor_id']
        if teacher == student or teacher not in actors or student not in actors:
            issue('TEACHING_REQUEST_PARTIES', path, '邀学双方须是不同的真实作者人物')
        if value['skill_id'] not in skills:
            issue('TEACHING_REQUEST_SKILL', path + ('skill_id',), '所学须是已有武功或心法')
        dialogue = dialogues.get(value['dialogue_id'])
        role_id = dialogue.get('context_role') if isinstance(dialogue, dict) else None
        if not _id(role_id) or role_id not in roles:
            issue('TEACHING_REQUEST_DIALOGUE', path + ('dialogue_id',), '本版邀学只支持明确岗位的普通当面对白')
            return None
        nodes = [node for node in dialogue.get('nodes', []) if isinstance(node, dict) and node.get('id') == value['node_id']]
        if len(nodes) != 1 or nodes[0].get('kind') not in ('say', 'choice', 'action'):
            issue('TEACHING_REQUEST_NODE', path + ('node_id',), '邀学须绑定可实际到达且未开工的具体节点')
            return None
        node = nodes[0]
        if node['kind'] == 'action':
            command = node.get('command', {})
            if not isinstance(command, dict) or command.get('kind') != 'teach_skill' or command.get('skill_id') != value['skill_id']:
                issue('TEACHING_REQUEST_ACTION', path + ('node_id',), '待确认节点必须是传授同一本领的原命令')
        return role_id

    declared = set()
    for number, row in enumerate(pack.get('teaching_requests', [])):
        path = ('teaching_requests', number)
        definition(row, path)
        if isinstance(row, dict) and _id(row.get('id')):
            if row['id'] in declared:
                issue('TEACHING_REQUEST_IDS', path, '请求ID不能重复')
            declared.add(row['id'])
            definitions[row['id']] = row

    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'teaching_requests' not in scenario:
            continue
        config = scenario['teaching_requests']
        path = ('scenarios', number, 'teaching_requests')
        if not isinstance(config, dict) or set(config) != {'profile', 'request_ids'} or config.get('profile') != PROFILE:
            issue('TEACHING_REQUEST_PROFILE', path, '场景须明确选择本版邀学政策与请求名单')
            continue
        selected = config['request_ids']
        if not isinstance(selected, list) or not 1 <= len(selected) <= 4096 or not all(_id(value) for value in selected) or len(selected) != len(set(selected)):
            issue('TEACHING_REQUEST_IDS', path + ('request_ids',), '所选请求须为1至4096个不同稳定ID')
            continue
        enabled = set(scenario.get('enabled', {}).get('actors', []))
        actual = {row.get('actor_id') for row in scenario.get('initial', {}).get('actors', []) if isinstance(row, dict)}
        bindings = set()
        for request_id in selected:
            value = definitions.get(request_id)
            if value is None:
                issue('TEACHING_REQUEST_IDS', path + ('request_ids', request_id), '缺少所选请求定义')
                continue
            role_id = definition(value, path + ('request_ids', request_id))
            if not isinstance(value, dict) or set(value) != FIELDS or not all(_id(value[key]) for key in FIELDS):
                continue
            if value['id'] != request_id:
                issue('TEACHING_REQUEST_IDS', path + ('request_ids', request_id), '请求字典ID与稳定ID不一致')
            if not {value['teacher_actor_id'], value['student_actor_id']} <= enabled & actual:
                issue('TEACHING_REQUEST_ACTORS', path + ('request_ids', request_id), '邀学双方须是本情景真实启用且有初态的人物')
            binding = tuple(value[key] for key in ('teacher_actor_id', 'student_actor_id', 'skill_id', 'dialogue_id', 'node_id'))
            if binding in bindings:
                issue('TEACHING_REQUEST_BINDING', path + ('request_ids', request_id), '同一实际节点只能指向一份具体邀学')
            bindings.add(binding)
            if role_id and role_id not in scenario.get('enabled', {}).get('roles', []):
                issue('TEACHING_REQUEST_ROLE', path + ('request_ids', request_id), '所选邀学的岗位须在本情景启用')
            if value['dialogue_id'] not in scenario.get('enabled', {}).get('dialogues', []):
                issue('TEACHING_REQUEST_DIALOGUE_DISABLED', path + ('request_ids', request_id), '所选邀学的对白须在本情景启用')
    return issues
