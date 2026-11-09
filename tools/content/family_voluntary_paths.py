"""真实生前本人声明的目录检查；只定义条款，不创建关系、许可或玩家控制。"""
from .voluntary_succession import _id


def check_family_voluntary_paths(pack, index):
    """与旧岗位条款并存；新请求必须绑定原Known私人提供者的不同接受/撤回SAY。"""
    issues = []
    talks = dict(index.get('dialogues', {}))
    items = dict(index.get('items', {}))
    for table, target in [('dialogues', talks), ('items', items)]:
        target.update({r['id']: r for r in pack.get(table, []) if isinstance(r, dict) and _id(r.get('id'))})
    required = {'family', 'family_life_v1', 'native_dynamic_world', 'known_personal_dialogues_v1', 'family_voluntary_paths_v1'}
    for number, scenario in enumerate(pack.get('scenarios', [])):
        profile = scenario.get('voluntary_succession', {})
        selected = isinstance(profile, dict) and profile.get('profile') == 'living_accepted_family_paths_v2'
        if 'family_voluntary_paths' not in scenario and not selected:
            continue
        path = ('scenarios', number, 'family_voluntary_paths')
        def issue(code, message):
            """诊断原配置位置，不通过默认参数补授权。"""
            issues.append((code, path, message))
        config = scenario.get('family_voluntary_paths')
        if not selected or not isinstance(config, dict) or set(config) != {'version', 'profile', 'requests'} or type(config.get('version')) is not int or config.get('version') != 1 or config.get('profile') != 'living_accepted_family_paths_v1':
            issue('VOLUNTARY_PATH_PAIR', '生前路径1须与原自愿接续2显式配对')
            continue
        dynamic = scenario.get('dynamic_world', {})
        known = dynamic.get('known_personal_dialogues', {}) if isinstance(dynamic, dict) else {}
        if not isinstance(known, dict) or known.get('version') != 1 or not required <= set(pack.get('required_capabilities', [])) or not isinstance(scenario.get('family_life'), dict):
            issue('VOLUNTARY_PATH_DEPENDENCIES', '须完整启用原家庭、生命和真实私人会面')
            known = known if isinstance(known, dict) else {}
        rows = config['requests']
        if not isinstance(rows, list) or not 1 <= len(rows) <= 4096:
            issue('VOLUNTARY_PATH_REQUESTS', '只接受有限非空请求')
            continue
        fields = {'id', 'template_id', 'accept_node_id', 'withdraw_node_id', 'relation_kind', 'disciple_skill_id', 'terms_version', 'consent_kind', 'scope', 'personal'}
        seen, declarations = set(profile.get('request_ids', [])), set()
        templates = known.get('templates', [])
        for row in rows:
            if not isinstance(row, dict) or set(row) != fields or not all(_id(row.get(k)) for k in ('id', 'template_id', 'accept_node_id', 'withdraw_node_id')):
                issue('VOLUNTARY_PATH_FIELDS', '必须明确原模板、两句本人声明、关系来源和分项财产')
                continue
            if row['id'] in seen:
                issue('VOLUNTARY_PATH_DUPLICATE', '新旧接续请求ID不得重复')
            seen.add(row['id'])
            if type(row['terms_version']) is not int or row['terms_version'] != 1 or row['consent_kind'] != 'successor_accepts_living_voluntary_succession_v1' or row['scope'] != 'player_control_and_selected_personal_assets':
                issue('VOLUNTARY_PATH_SCOPE', '生前许可只含本人接续与原列私产，不授死后或公职代理')
            relation = row['relation_kind']
            if relation not in ('adoption', 'biological_child', 'accepted_disciple', 'recognized_collateral') or (relation == 'accepted_disciple' and not _id(row['disciple_skill_id'])) or (relation != 'accepted_disciple' and row['disciple_skill_id'] != ''):
                issue('VOLUNTARY_PATH_RELATION', '须明确四种原关系之一，弟子须给原受教学艺')
            dependency = {'biological_child': 'family_lineage', 'accepted_disciple': 'family_apprenticeship', 'recognized_collateral': 'family_collateral_legacy'}.get(relation)
            if dependency and not isinstance(scenario.get(dependency), dict):
                issue('VOLUNTARY_PATH_RELATION_DEPENDENCY', '所选关系必须有其真实已启用原账')
            found = [t for t in templates if isinstance(t, dict) and t.get('id') == row['template_id']]
            talk = talks.get(found[0].get('dialogue_id'), {}) if len(found) == 1 else {}
            if len(found) != 1 or talk.get('context_role') or talk.get('id') not in scenario.get('enabled', {}).get('dialogues', []):
                issue('VOLUNTARY_PATH_TEMPLATE', '须是本场景唯一实际启用的Known私人对白')
                continue
            for key in ('accept_node_id', 'withdraw_node_id'):
                nodes = [n for n in talk.get('nodes', []) if isinstance(n, dict) and n.get('id') == row[key]]
                if len(nodes) != 1 or nodes[0].get('kind') != 'say' or nodes[0].get('speaker') != '$provider':
                    issue('VOLUNTARY_PATH_PROVIDER_SAY', '候选本人必须真实发言，choice和玩家台词不能代签')
                pair = (talk['id'], row[key])
                if pair in declarations:
                    issue('VOLUNTARY_PATH_DUPLICATE_DECLARATION', '每句接受或撤回只承担唯一生前条款')
                declarations.add(pair)
                # 已启用Legacy/旁支的同一句节点不能隐含另一种控制许可。
                collateral = scenario.get('family_collateral_legacy', {})
                for other in collateral.get('requests', []) if isinstance(collateral, dict) else []:
                    if other.get('template_id') == row['template_id'] and row[key] in [other.get(k) for k in ('recognize_node_id', 'accept_node_id', 'revoke_node_id')]:
                        issue('VOLUNTARY_PATH_DISTINCT_DECLARATION', '血缘认可、死后同意和生前接受必须分别陈述')
                # 带回书的本人声明仍只授原交付，不能隐含生前接续许可。
                returns = scenario.get('task_message_returns', {})
                for other in returns.get('policies', []) if isinstance(returns, dict) else []:
                    if isinstance(other, dict) and (other.get('dialogue_id'), other.get('node_id')) == pair:
                        issue('VOLUNTARY_PATH_DISTINCT_DECLARATION', '带回书与生前接续必须分别陈述')
            personal = row['personal']
            if not isinstance(personal, dict) or set(personal) != {'money', 'goods'} or type(personal.get('money')) is not int or not 0 <= personal['money'] <= 9223372036854775807 or not isinstance(personal.get('goods'), list) or len(personal['goods']) > 4096:
                issue('VOLUNTARY_PATH_PERSONAL', '只能约定有限非负私钱与原品类数量')
                continue
            goods = set()
            for good in personal['goods']:
                if not isinstance(good, dict) or set(good) != {'item_id', 'quantity'} or good.get('item_id') not in items or good.get('item_id') in goods or type(good.get('quantity')) is not int or not 1 <= good['quantity'] <= 9223372036854775807:
                    issue('VOLUNTARY_PATH_GOODS', '品类须实际存在、只列一次且数量为正')
                else:
                    goods.add(good['item_id'])
    return issues
