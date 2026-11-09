"""真实军阵教学作者仅配置原教师提议与学费；完成、掌握和付款不能由作者预写。"""


def check_tactic_lessons(pack, index):
    """逐原当面say和军事唯一定义核引用，条款不是本人报名或已学证明。"""
    issues = []
    tables = {}
    for name in ('actors', 'roles', 'dialogues', 'wallets'):
        supplied = index.get(name, {})
        table = dict(supplied) if isinstance(supplied, dict) else {r['id']: r for r in supplied if isinstance(r, dict) and isinstance(r.get('id'), str)}
        table.update({r['id']: r for r in pack.get(name, []) if isinstance(r, dict) and isinstance(r.get('id'), str)})
        tables[name] = table
    caps = set(pack.get('required_capabilities', []))
    required = {'growth_tactic_lessons_v1', 'military_tactic_learning_v1', 'family_life_v1', 'family', 'native_dynamic_world', 'cultivation', 'dialogue', 'multi_actor'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        dynamic = scenario.get('dynamic_world', {})
        if not isinstance(dynamic, dict) or 'growth_tactic_lessons' not in dynamic:
            continue
        path = ('scenarios', number, 'dynamic_world', 'growth_tactic_lessons')
        def fail(code, detail):
            """给出准确条款位置，不替缺失作者补教师、课时或免费学习。"""
            issues.append((code, path, detail))
        learning = scenario.get('military_tactic_learning', {})
        if not required <= caps or dynamic.get('profile') != 'local_society_v1' or not isinstance(learning, dict) or learning.get('version') != 1 or not isinstance(scenario.get('family_life'), dict):
            fail('TACTIC_LESSON_PAIR', '需要明确选择真实人物生命、原成长与军事掌握完整组合')
        definitions = learning.get('definitions', []) if isinstance(learning, dict) else []
        tactics = {row['id']: row for row in definitions if isinstance(row, dict) and isinstance(row.get('id'), str)} if isinstance(definitions, list) else {}
        config = dynamic['growth_tactic_lessons']
        if not isinstance(config, dict) or set(config) != {'version', 'definitions'} or type(config.get('version')) is not int or config['version'] != 1:
            fail('TACTIC_LESSON_FIELDS', '只能声明版本1和原教师授课条款'); continue
        rows = config['definitions']
        if not isinstance(rows, dict) or not 1 <= len(rows) <= 4096:
            fail('TACTIC_LESSON_DEFINITIONS', '条款必须是非空有限映射'); continue
        seen = set()
        for identifier, row in rows.items():
            fields = {'id', 'tactic_id', 'teacher_actor_id', 'dialogue_id', 'offer_node_id', 'recipient_wallet_id', 'duration', 'total_fee'}
            if not isinstance(row, dict) or set(row) != fields:
                fail('TACTIC_LESSON_TERM_FIELDS', '原人物、原说话节点、私钱包、课时和学费必须明确'); continue
            if any(not isinstance(row[k], str) or not 1 <= len(row[k]) <= 256 for k in fields - {'duration', 'total_fee'}) or identifier != row['id']:
                fail('TACTIC_LESSON_REFERENCE', '引用必须明确且条款键与原ID相等'); continue
            if type(row['duration']) is not int or not 1 <= row['duration'] <= 720 or type(row['total_fee']) is not int or not 0 <= row['total_fee'] <= 2147483647:
                fail('TACTIC_LESSON_PAYMENT', '课时和总学费须在实际整数上限内')
            teacher = row['teacher_actor_id']
            definition = tactics.get(row['tactic_id'])
            if not definition or teacher not in definition.get('teacher_actor_ids', []):
                fail('TACTIC_LESSON_TEACHER_KNOWLEDGE', '教师须列在本场景军事原定义，运行时仍重验真实掌握')
            talk = tables['dialogues'].get(row['dialogue_id'])
            node = next((n for n in talk.get('nodes', []) if isinstance(n, dict) and n.get('id') == row['offer_node_id']), None) if talk else None
            role = tables['roles'].get(talk.get('context_role')) if talk else None
            if not node or node.get('kind') != 'say' or node.get('speaker') not in ('$provider', teacher) or not role or role.get('initial_actor') != teacher or role.get('organization_id'):
                fail('TACTIC_LESSON_PERSONAL_SAY', '必须是原教师私人岗位的当前真实say，choice/action或听者发言均不能代签')
            if teacher not in scenario.get('enabled', {}).get('actors', []) or (role and role['id'] not in scenario.get('enabled', {}).get('roles', [])):
                fail('TACTIC_LESSON_ACTOR_DISABLED', '原教师及私人岗位必须在本情景真实启用')
            wallet = tables['wallets'].get(row['recipient_wallet_id'])
            if not wallet or wallet.get('owner') != {'kind': 'actor', 'id': teacher}:
                fail('TACTIC_LESSON_PRIVATE_ACCOUNT', '只允许教师原私人钱包，不能以机构公库代收')
            if index and teacher not in tables['actors']:
                fail('TACTIC_LESSON_ACTOR', '教师必须是正规原目录人物')
            key = (teacher, row['dialogue_id'], row['offer_node_id'])
            if key in seen:
                fail('TACTIC_LESSON_AMBIGUOUS_OFFER', '同一真实节点不能同时对应两份不同授课条款')
            seen.add(key)
    return issues
