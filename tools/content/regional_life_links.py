"""核验正式地方生活的私人物资目标和当面来源；不计算工资、成交价或剧情结果。"""
from .director_registry_links import director_policies, initial_director_bindings
from collections import defaultdict


def _place_paths(enabled, index):
    """把原地方通道及城口道路转成有向图，只证明声明可达，不替运行旅行报价。"""
    places = set(enabled.get('places', []))
    edges = {key: set(index['places'][key]['passages']) & places for key in places}
    for route_id in enabled.get('routes', []):
        route = index['routes'][route_id]
        origin = index['settlements'][route['origin']]['entry_place']
        destination = index['settlements'][route['destination']]['entry_place']
        if origin in places and destination in places:
            edges[origin].add(destination)
    return edges


def _reachable(start, target, edges):
    """有限工作列表处理回路；不可达只产生作者诊断，不增补地图或道路。"""
    pending, seen = [start], set()
    while pending:
        current = pending.pop()
        if current == target:
            return True
        if current not in seen:
            seen.add(current)
            pending.extend(edges.get(current, set()) - seen)
    return False


def regional_dialogue_bindings(pack):
    """按本次情景显式声明查对白身份，空岗位本身不能赋予普通人物交易权限。"""
    actors, needs = defaultdict(list), defaultdict(list)
    for scenario in pack.get('scenarios', []):
        regional = scenario.get('dynamic_world', {}).get('regional_life', {})
        for topic in regional.get('actor_topics', []):
            actors[topic['dialogue_id']].append(topic)
        for topic in regional.get('need_topics', []):
            needs[topic['dialogue_id']].append(topic)
    return actors, needs


def check_regional_life(pack, index):
    """来源、服务与启用集均取真实作者记录；错误路径保留到同一正式校验入口。"""
    issues = []
    capabilities = set(pack.get('required_capabilities', []))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        world = scenario.get('dynamic_world', {})
        regional = world.get('regional_life')
        subjects = director_policies(world)
        has_private = any('private_livelihood' in policy or 'private_livelihood' in policy['templates']
                          for policy in subjects.values())
        if regional is None and not has_private:
            continue
        root = ('scenarios', number, 'dynamic_world')

        def issue(code, path, message):
            """用原字段定位汇报静态错误，不把本函数的派生量写回内容。"""
            issues.append((code, root + tuple(path), message))

        if not {'native_dynamic_world', 'regional_life_v1'} <= capabilities:
            issue('REGIONAL_LIFE_CAPABILITY', (), '地方生活须声明原生动态世界及regional_life_v1能力')
        if regional is None:
            issue('REGIONAL_LIFE_BINDINGS', ('regional_life',), '私人物资目标须显式提供同代地方近况配置，可用空主题数组')
            regional = {'actor_topics': [], 'need_topics': []}
        enabled, initial = scenario.get('enabled', {}), scenario.get('initial', {})
        actors = {row['actor_id']: row for row in initial.get('actors', [])}
        assignments = {row['role_id']: row for row in initial.get('roles', [])}
        inventories = {row['id']: row for row in initial.get('inventories', [])}
        edges = _place_paths(enabled, index)
        sources = world.get('local_economy', {}).get('supply_sources', {})
        for actor_id, policy, path in initial_director_bindings(scenario):
            private = policy.get('private_livelihood')
            template = 'private_livelihood' in policy['templates']
            if template != (private is not None):
                issue('REGIONAL_LIFE_POLICY', path, 'private_livelihood模板与完整物资准备策略必须成对声明')
            if private is None:
                continue
            person = actors.get(actor_id)
            if person is None or actor_id == scenario['player_actor']:
                issue('REGIONAL_LIFE_SUBJECT', path, '自主物资目标只能绑定本情景真实非玩家人物')
                continue
            start, destination = person['place_id'], private['return_place_id']
            if destination not in edges or not _reachable(start, destination, edges):
                issue('REGIONAL_LIFE_DESTINATION', path + ('private_livelihood', 'return_place_id'), '目标场所须启用且可沿真实通道和城口道路到达')
            item = index['items'][private['item_id']]
            if item['kind'] != 'commodity' or item.get('base_price', 0) <= 0:
                issue('REGIONAL_LIFE_ITEM', path + ('private_livelihood', 'item_id'), '私人购买只接受原交易支持的有价商品')
            work = False
            for role_id in private['known_service_role_ids']:
                role = index['roles'][role_id]
                assignment = assignments.get(role_id)
                activities = role.get('activities', {})
                if (role_id not in enabled.get('roles', []) or assignment is None
                        or not assignment['enabled'] or assignment['actor_id'] not in actors
                        or not any(activity in role['services'] and activity in activities for activity in ('work', 'rest'))):
                    issue('REGIONAL_LIFE_SERVICE', path + ('private_livelihood', 'known_service_role_ids'), '已知服务须引用本情景启用、实际有人办理的工作或休息岗位')
                    continue
                if assignment['actor_id'] == actor_id:
                    issue('REGIONAL_LIFE_SELF_SERVICE', path + ('private_livelihood', 'known_service_role_ids'), '不能把本人岗位声明为给自己付薪的服务来源')
                if not _reachable(start, role['place_id'], edges) or not _reachable(role['place_id'], destination, edges):
                    issue('REGIONAL_LIFE_SERVICE_ROUTE', path + ('private_livelihood', 'known_service_role_ids'), '工作或休息场所及其后续目的地须有实际可走路径')
                terms = activities.get('work', {})
                work = work or ('work' in role['services'] and terms.get('wage', 0) > terms.get('cost', 0))
            if not work:
                issue('REGIONAL_LIFE_WORK_SOURCE', path + ('private_livelihood', 'known_service_role_ids'), '至少声明一处原条款净报酬为正的真实短工来源')
            known_sources = {key for key in policy.get('report_source_keys', [])}
            available = False
            for source_id, source in sources.items():
                if (source['item_id'] != private['item_id'] or not source.get('public_report')
                        or source_id not in known_sources and source['site_id'] != start):
                    continue
                for shop_id in enabled.get('shops', []):
                    shop = index['shops'][shop_id]
                    if (shop['inventory_id'] != source['inventory_id'] or shop['place_id'] != source['site_id']
                            or source['inventory_id'] not in inventories):
                        continue
                    if _reachable(start, shop['place_id'], edges) and _reachable(shop['place_id'], destination, edges):
                        available = True
            if not available:
                issue('REGIONAL_LIFE_MARKET_SOURCE', path + ('private_livelihood', 'item_id'), '目标须有已声明可知、公开且可到达的真实商店货源；当前缺货可合法等待')
        topic_ids = set()
        for table in ('actor_topics', 'need_topics'):
            for position, topic in enumerate(regional[table]):
                path = ('regional_life', table, position)
                if topic['id'] in topic_ids:
                    issue('REGIONAL_LIFE_TOPIC_ID', path + ('id',), '同一情景的人物与岗位主题ID必须共同唯一')
                topic_ids.add(topic['id'])
                dialogue = index['dialogues'][topic['dialogue_id']]
                if topic['dialogue_id'] not in enabled.get('dialogues', []):
                    issue('REGIONAL_LIFE_DIALOGUE_ENABLED', path + ('dialogue_id',), '主题对白必须在当前情景显式启用')
                if table == 'actor_topics':
                    if topic['actor_id'] not in actors or topic['actor_id'] == scenario['player_actor']:
                        issue('REGIONAL_LIFE_TOPIC_ACTOR', path + ('actor_id',), '当面近况绑定真实已启用的另一人物，不能临时生成人物')
                    if dialogue['context_role'] or any(node['kind'] not in ('say', 'choice', 'end') for node in dialogue['nodes']):
                        issue('REGIONAL_LIFE_NARRATIVE', path + ('dialogue_id',), '人物近况仅可用空岗位的say/choice/end叙述，不能夹带服务或业务命令')
                    continue
                role = index['roles'][topic['role_id']]
                if (topic['role_id'] not in assignments or not assignments[topic['role_id']]['enabled']
                        or dialogue['context_role'] != topic['role_id'] or 'accept_offer' not in role['services']):
                    issue('REGIONAL_LIFE_NEED_ROLE', path + ('role_id',), '共同需求必须由当前启用且有接单权限的同一岗位对白办理')
                policies = world.get('affairs', {}).get('need_policies', {}).values()
                if not any(policy['accept_role'] == topic['role_id'] and policy['item_id'] == topic['item_id'] for policy in policies):
                    issue('REGIONAL_LIFE_NEED_SOURCE', path + ('item_id',), '主题必须引用同一岗位和物品的真实共享需求政策')
                for node in dialogue['nodes']:
                    if (node['kind'] not in ('say', 'choice', 'action', 'end')
                            or node['kind'] == 'action' and node['command'] != {'kind': 'accept_offer'}):
                        issue('REGIONAL_LIFE_NEED_COMMAND', path + ('dialogue_id',), '共同需求对白只允许绑定会话提供的原accept_offer，不固定另一货单或任意业务命令')
    return issues
