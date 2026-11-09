"""只核本人合同回访的作者来源与叙事边界；不制造交付、到期或结算结果。"""


def check_regional_outcomes(pack, index):
    """沿当前情景实际启用集核对原采购模板、办理岗位和只读对白，旧章无新增要求。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        regional = world.get('regional_life', {})
        topics = regional.get('outcome_topics', [])
        if not topics:
            continue
        root = ('scenarios', number, 'dynamic_world', 'regional_life')

        def issue(code, position, field, message):
            """保留作者数组与字段的真实位置，不以编译后的ID替代源定位。"""
            issues.append((code, root + ('outcome_topics', position) + tuple(field), message))

        if not {'native_dynamic_world', 'regional_life_v1'} <= set(pack.get('required_capabilities', [])):
            issues.append(('REGIONAL_OUTCOME_CAPABILITY', root, '合同回访必须使用同代原生动态世界与地方生活能力'))
        enabled = scenario.get('enabled', {})
        assignments = {row['role_id']: row for row in scenario.get('initial', {}).get('roles', [])}
        ids = {row['id'] for row in regional.get('actor_topics', []) + regional.get('need_topics', [])}
        dialogues = {row['dialogue_id'] for row in regional.get('actor_topics', []) + regional.get('need_topics', [])}
        for position, topic in enumerate(topics):
            if topic['id'] in ids or topic['dialogue_id'] in dialogues:
                issue('REGIONAL_OUTCOME_DUPLICATE', position, (), '人物、需求与结果主题共同去重，不能复用同一主题或对白身份')
            ids.add(topic['id'])
            dialogues.add(topic['dialogue_id'])
            if len(ids) > 64:
                issue('REGIONAL_OUTCOME_LIMIT', position, (), '三类地方主题合计最多64项')
            if topic['cooldown_ticks'] > 720:
                issue('REGIONAL_OUTCOME_COOLDOWN', position, ('cooldown_ticks',), '回访冷却不得超过原地方会话支持的720半日')
            role = index.get('roles', {}).get(topic['role_id'])
            template = index.get('task_templates', {}).get(topic['task_template_id'])
            dialogue = index.get('dialogues', {}).get(topic['dialogue_id'])
            assignment = assignments.get(topic['role_id'])
            if (not role or topic['role_id'] not in enabled.get('roles', []) or not assignment
                    or not assignment['enabled'] or 'report_offer' not in role['services']):
                issue('REGIONAL_OUTCOME_ROLE', position, ('role_id',), '回访只绑定当前启用且有原验收权限的实际岗位')
            if (not template or topic['task_template_id'] not in enabled.get('task_templates', [])
                    or template['template'] != 'procurement' or template['report_role'] != topic['role_id']):
                issue('REGIONAL_OUTCOME_TEMPLATE', position, ('task_template_id',), '结果须指向本情景启用的同岗位原采购模板')
            if role and template:
                owner = ({'kind': 'organization', 'id': role['organization_id']} if role['organization_id']
                         else {'kind': 'actor', 'id': role['initial_actor']})
                if template['issuer'] != owner:
                    issue('REGIONAL_OUTCOME_ISSUER', position, ('role_id',), '回访办理岗位须代表原合同签发主体')
            if (not dialogue or topic['dialogue_id'] not in enabled.get('dialogues', [])
                    or dialogue['context_role'] != topic['role_id']):
                issue('REGIONAL_OUTCOME_DIALOGUE', position, ('dialogue_id',), '回访对白须显式启用并使用同一真实岗位上下文')
                continue
            for node in dialogue['nodes']:
                if node['kind'] not in ('say', 'choice', 'end') or 'command' in node or 'condition' in node:
                    issue('REGIONAL_OUTCOME_COMMAND', position, ('dialogue_id',), '结果回访只有say/choice/end，不能代报到、发奖或改任务状态')
                if node['kind'] == 'say' and node['speaker'] not in ('$provider', '$player'):
                    issue('REGIONAL_OUTCOME_SPEAKER', position, ('dialogue_id',), '结果对白只能由本人或原办理人说出，不替另一角色宣称知情')
    return issues
