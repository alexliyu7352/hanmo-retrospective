"""核验军事作者定义的人产来源与有限拓扑；不创建兵员、不扣供给、不改变作者包。"""

from .graphs import reachable
from .naval_links import check_naval
from .military_recovery_links import check_military_wounded_recovery
from .military_supply_attrition import check_military_supply_attrition
from .military_rout_retreat import check_military_rout_retreat
from .military_command_shock import check_military_command_shock
from .military_garrison_rest import check_military_garrison_rest
from .military_combat_conditions import check_military_combat_conditions
from .military_commanded_damage import check_military_commanded_damage
from .military_sequential_rounds import check_military_sequential_rounds
from .military_field_terrain import check_military_field_terrain
from .military_command_resources import check_military_command_resources
from .military_tactic_learning import check_military_tactic_learning
from .military_siege_engineering import check_military_siege_engineering

TABLES = ('battlefields', 'troop_types', 'military_objectives', 'offices',
          'garrisons', 'military_orders', 'strongholds', 'siege_topologies')
SERVICES = ('office_accept', 'office_resign', 'military_order', 'military_report')


def check_military(pack: dict, index: dict) -> list:
    """在通用 Schema 与引用校验之后返回字段元组；根管线继续统一生成记录 ID 和 JSON 指针。"""
    issues = []
    if (any(pack.get(table) for table in TABLES) or
            any(set(role['services']) & set(SERVICES) for role in pack.get('roles', [])) or
            any(actor.get('military_plan') for actor in pack.get('actors', [])) or
            any('military_condition_model' in row or 'military_request_policy' in row for row in pack.get('scenarios', []))):
        if 'military' not in pack.get('required_capabilities', []):
            issues.append(('MILITARY_CAPABILITY', ('required_capabilities',), '军事定义和服务须声明 military 能力'))
    for number, board in enumerate(pack.get('battlefields', [])):
        issues.extend(_board(board, index, ('battlefields', number)))
        if 'commander_losses' in board and 'military_field_commanders_v1' not in pack.get('required_capabilities', []):
            issues.append(('MILITARY_COMMANDER_CAPABILITY', ('battlefields', number, 'commander_losses'),
                           '陆战军官暴露须显式声明 military_field_commanders_v1'))
    for number, troop in enumerate(pack.get('troop_types', [])):
        issues.extend(_troop(troop, ('troop_types', number)))
    for number, goal in enumerate(pack.get('military_objectives', [])):
        path = ('military_objectives', number)
        if goal['kind'] in ('breakthrough', 'escort', 'cover_retreat') and not goal['allow_withdrawal']:
            issues.append(('MILITARY_OBJECTIVE_EXIT', path + ('allow_withdrawal',), '出口目标必须允许实际撤离'))
        if goal['kind'] == 'hold' and goal['hold_rounds'] > goal['max_rounds']:
            issues.append(('MILITARY_HOLD_ROUNDS', path + ('hold_rounds',), '占点轮数不能超过战场期限'))
    issues.extend(check_naval(pack, index))
    issues.extend(check_military_wounded_recovery(pack, index))
    issues.extend(check_military_supply_attrition(pack, index))
    issues.extend(check_military_rout_retreat(pack, index))
    issues.extend(check_military_command_shock(pack, index))
    issues.extend(check_military_garrison_rest(pack, index))
    issues.extend(check_military_combat_conditions(pack, index))
    issues.extend(check_military_commanded_damage(pack, index))
    issues.extend(check_military_sequential_rounds(pack, index))
    issues.extend(check_military_field_terrain(pack, index))
    issues.extend(check_military_command_resources(pack, index))
    issues.extend(check_military_tactic_learning(pack, index))
    issues.extend(check_military_siege_engineering(pack, index))
    issues.extend(_request_policies(pack))
    issues.extend(_condition_models(pack, index))
    issues.extend(_garrisons(pack, index))
    issues.extend(_offices_orders(pack, index))
    issues.extend(_military_plans(pack, index))
    issues.extend(_strongholds(pack, index))
    for number, topology in enumerate(pack.get('siege_topologies', [])):
        issues.extend(_topology(topology, index, ('siege_topologies', number)))
    return issues


def _request_policies(pack: dict) -> list:
    """只允许明确动态军旅请求政策；旧情景不补政策，未知字段不被当默认值。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'military_request_policy' not in scenario:
            continue
        policy = scenario['military_request_policy']
        path = ('scenarios', number, 'military_request_policy')
        if (not isinstance(policy, dict) or set(policy) != {'profile', 'per_actor_round_limit'} or
                policy.get('profile') != 'request_policy/1' or
                type(policy.get('per_actor_round_limit')) is not int or policy['per_actor_round_limit'] < 1):
            issues.append(('MILITARY_REQUEST_POLICY', path, '请求政策须明确request_policy/1和正整数每轮额度'))
        if (scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1' or
                'military' not in pack.get('required_capabilities', [])):
            issues.append(('MILITARY_REQUEST_PROFILE', path, '部将请求只支持已声明military的动态军旅profile'))
    return issues


def _condition_models(pack: dict, index: dict) -> list:
    """只核本剧本的真实地形与有效驻军初态；旧道路不启用，原型和缺失条件不猜补。"""
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'military_condition_model' not in scenario:
            continue
        path = ('scenarios', number)
        if scenario['military_condition_model'] != 'terrain_march_v1':
            issues.append(('MILITARY_CONDITION_MODEL', path + ('military_condition_model',), '军事条件只支持显式 terrain_march_v1 模型'))
            continue
        map_id = scenario.get('world_map_id')
        if not isinstance(map_id, str) or map_id not in index.get('world_maps', {}):
            issues.append(('MILITARY_CONDITION_MAP', path + ('world_map_id',), '地形行军条件须指向本剧本的真实地形地图，不支持旧道路模式'))
            continue
        # Snapshot已按启用集合核验并投影；其明确初态覆盖整份units，不回落全局原型。
        camps = scenario['initial'].get('garrisons', []) if 'initial' in scenario else pack.get('garrisons', [])
        prefix = path + ('initial', 'garrisons') if 'initial' in scenario else ('garrisons',)
        for camp_number, camp in enumerate(camps):
            for unit_number, unit in enumerate(camp['units']):
                if any(type(unit.get(field)) is not int or not 0 <= unit[field] <= 100
                       for field in ('initial_training', 'initial_fatigue')):
                    issues.append(('MILITARY_TRAINING_PROFILE', prefix + (camp_number, 'units', unit_number),
                                   '启用地形行军的部队须明确提供0..100整数训练和疲劳：' + unit['id']))
    return issues


def _board(board: dict, index: dict, path: tuple) -> list:
    """核验重复六角格、真实入口和默认目标；不在作者侧另写寻路或战斗算法。"""
    issues, cells = [], {}
    model = board.get('damage_model', {})
    if model.get('kind') == 'legacy_base_factors_v1' and model['rear_bp'] < model['side_bp']:
        issues.append(('MILITARY_DAMAGE_MODEL', path + ('damage_model',), '显式后方倍率不能低于侧方倍率'))
    for number, cell in enumerate(board['cells']):
        key = (cell['q'], cell['r'])
        if key in cells:
            issues.append(('MILITARY_HEX_DUPLICATE', path + ('cells', number), '六角地形坐标不能重复'))
        cells[key] = cell
    for field in ('spawns', 'exits'):
        seen = set()
        for number, entry in enumerate(board[field]):
            key = tuple(entry['position']); cell = cells.get(key)
            if cell is None or cell.get('blocks_movement', cell['terrain'] == 'wall'):
                issues.append(('MILITARY_HEX_ENTRY', path + (field, number, 'position'), '入口须位于实际可站立六角格'))
            if field == 'spawns' and key in seen:
                issues.append(('MILITARY_HEX_SPAWN', path + (field, number, 'position'), '两部不能共用初始出生格'))
            seen.add(key)
        if field == 'spawns' and {entry['side'] for entry in board[field]} != {0, 1}:
            issues.append(('MILITARY_HEX_SIDES', path + (field,), '场地须具有两个阵营的实际出生格'))
    goal = index['military_objectives'][board['objective_id']]
    if goal['kind'] == 'hold' and tuple(goal['point']) not in cells:
        issues.append(('MILITARY_HOLD_POINT', path + ('objective_id',), '默认占点目标必须位于当前战场'))
    if goal['kind'] in ('breakthrough', 'escort', 'cover_retreat') and not any(e['side'] == goal['side'] for e in board['exits']):
        issues.append(('MILITARY_OBJECTIVE_EXIT', path + ('objective_id',), '默认出口目标的阵营须有实际退出格'))
    return issues


def _troop(troop: dict, path: tuple) -> list:
    """兵种与战法只核对已实现的类别组合、射程及共同预算，不计算命中伤亡。"""
    issues, seen = [], set()
    if troop['minimum_range'] > troop['maximum_range']:
        issues.append(('MILITARY_TROOP_RANGE', path + ('minimum_range',), '兵种内射程不能超过外射程'))
    for number, tactic in enumerate(troop['tactics']):
        at = path + ('tactics', number)
        if tactic['id'] in seen:
            issues.append(('MILITARY_TACTIC_DUPLICATE', at + ('id',), '同兵种战法 ID 不能重复'))
        seen.add(tactic['id'])
        if ((tactic['kind'] == 'volley' and troop['category'] != 'archer') or
                (tactic['kind'] == 'charge' and troop['category'] == 'archer') or
                (tactic['kind'] == 'support' and troop['category'] != 'infantry') or
                (tactic['kind'] == 'flank' and troop['category'] != 'cavalry') or
                (tactic['kind'] == 'suppress' and troop['category'] != 'archer')):
            issues.append(('MILITARY_TACTIC_TROOP', at + ('kind',), '齐射和牵制属于弓兵，弓兵不能冲锋，协防属于步兵，侧击属于骑兵'))
        if tactic['point_cost'] > troop['tactic_capacity'] or troop['movement'] + tactic['move_bonus'] > 24:
            issues.append(('MILITARY_TACTIC_COST', at + ('point_cost',), '战法消耗和移动须在当前兵种能力内'))
        if ((tactic['kind'] == 'rally' and (tactic['power_bp'] != 10000 or tactic['accuracy_bonus'] or tactic['move_bonus'])) or
                (tactic['kind'] == 'volley' and tactic['move_bonus']) or
                (tactic['kind'] == 'support' and (tactic['power_bp'] != 10000 or tactic['accuracy_bonus']
                 or tactic['move_bonus'] or tactic['morale_effect'] or tactic['status_rounds'] != 1)) or
                (tactic['kind'] == 'march' and (tactic['power_bp'] != 10000 or tactic['accuracy_bonus']
                 or tactic['move_bonus'] != 2 or tactic['morale_effect'] or tactic['status_rounds'] != 1)) or
                (tactic['kind'] in ('flank', 'feint', 'suppress') and (tactic['power_bp'] != {'flank': 12500, 'feint': 10000, 'suppress': 8000}[tactic['kind']]
                 or tactic['accuracy_bonus'] or tactic['move_bonus'] or tactic['morale_effect'] or tactic['status_rounds'] != 1))):
            issues.append(('MILITARY_TACTIC_EFFECT', at, '当前战法没有声明之外的攻击或移动效果'))
    return issues


def _garrisons(pack: dict, index: dict) -> list:
    """初始名册须有唯一同城军官和独立公产；短粮、空钱柜允许存在，运行时会实际拒绝资金不足或记录缺粮。"""
    issues, units, commanders, assets = [], set(), set(), set()
    private = {estate[field] for estate in pack.get('estates', []) for field in ('inventory_id', 'wallet_id')}
    for number, camp in enumerate(pack.get('garrisons', [])):
        path = ('garrisons', number); owner = {'kind': 'organization', 'id': camp['organization_id']}
        city = index['places'][camp['place_id']]['settlement_id']
        if ('training_wallet_id' in camp) != ('training_fatigue_per_tick' in camp):
            issues.append(('MILITARY_TRAINING_CONFIG', path, '训练服务钱包和每课疲劳必须显式成对声明'))
        if 'training_wallet_id' in camp and (camp['training_wallet_id'] == camp['wallet_id'] or index['wallets'][camp['training_wallet_id']]['owner'] != owner):
            issues.append(('MILITARY_TRAINING_PAYMENT', path + ('training_wallet_id',), '训练收款必须为同组织另一真实公共钱包'))
        for table, field in [('inventories', 'inventory_id'), ('wallets', 'wallet_id')]:
            if index[table][camp[field]]['owner'] != owner or camp[field] in assets or camp[field] in private:
                issues.append(('MILITARY_PUBLIC_ASSET', path + (field,), '驻军公库须独立属于本军组织，不能借用私庄或重复驻军容器'))
            assets.add(camp[field])
            if any(anchor != city for anchor in _asset_places(pack, index, table, camp[field])):
                issues.append(('MILITARY_ASSET_PLACE', path + (field,), '驻军不能占用已锚定外城的组织公库'))
        if index['items'][camp['food_item_id']]['kind'] != 'commodity':
            issues.append(('MILITARY_COMMODITY', path + ('food_item_id',), '驻军口粮必须是实际商品'))
        for offset, unit in enumerate(camp['units']):
            at = path + ('units', offset)
            if ('initial_training' in unit) != ('initial_fatigue' in unit):
                issues.append(('MILITARY_TRAINING_PROFILE', at, '部队训练和疲劳必须由作者成对提供，不猜缺省值'))
            if unit['id'] in units:
                issues.append(('MILITARY_UNIT_DUPLICATE', at + ('id',), '初始部队 ID 必须跨所有驻军唯一'))
            units.add(unit['id'])
            actor = index['actors'][unit['commander_actor_id']]
            if unit['commander_actor_id'] in commanders or index['places'][actor['initial_place']]['settlement_id'] != city:
                issues.append(('MILITARY_COMMANDER', at + ('commander_actor_id',), '军官须真实在本城，不能同时统领另一初始部队；编组时仍须实际到营'))
            commanders.add(unit['commander_actor_id'])
            goods = set()
            for position, equipment in enumerate(unit['equipment']):
                item_id = equipment['item_id']
                if item_id in goods or index['items'][item_id]['kind'] != 'commodity':
                    issues.append(('MILITARY_EQUIPMENT', at + ('equipment', position, 'item_id'), '每种兵装仅有一份实际商品配额'))
                goods.add(item_id)
    return issues


def _role(role_id: str, service: str, organization: str, city: str, index: dict, path: tuple) -> list:
    """授任、授命和报告岗位须代表同城组织，初始办理人实际在岗，不能借私人角色签发公令。"""
    role = index['roles'][role_id]
    if (role['organization_id'] != organization or service not in role['services'] or
            index['places'][role['place_id']]['settlement_id'] != city or
            index['actors'][role['initial_actor']]['initial_place'] != role['place_id']):
        return [('MILITARY_ROLE_AUTHORITY', path, '军事办理人须在本城实际岗位，具有相应服务和组织授权')]
    return []


def _offices_orders(pack: dict, index: dict) -> list:
    """战略目标保持独立，受任权限和交付组织必须由真实岗位与驻军定义相互证明。"""
    issues, services = [], set()
    for number, office in enumerate(pack.get('offices', [])):
        path = ('offices', number); camp = index['garrisons'][office['garrison_id']]
        if office['organization_id'] != camp['organization_id']:
            issues.append(('MILITARY_OFFICE_ORGANIZATION', path + ('garrison_id',), '公职不能统辖另一个组织的驻军'))
        city = index['places'][camp['place_id']]['settlement_id']
        for service in ('office_accept', 'office_resign'):
            services.add((office['appointing_role_id'], service))
            issues.extend(_role(office['appointing_role_id'], service, office['organization_id'], city, index, path + ('appointing_role_id',)))
    for number, order in enumerate(pack.get('military_orders', [])):
        path = ('military_orders', number); office = index['offices'][order['office_id']]
        camp = index['garrisons'][order['garrison_id']]
        if office['garrison_id'] != order['garrison_id'] or office['organization_id'] != camp['organization_id']:
            issues.append(('MILITARY_ORDER_GARRISON', path + ('garrison_id',), '军令必须使用该公职真正统辖的驻军'))
        city = index['places'][camp['place_id']]['settlement_id']
        for field, service in [('issuer_role_id', 'military_order'), ('report_role_id', 'military_report')]:
            services.add((order[field], service))
            issues.extend(_role(order[field], service, office['organization_id'], city, index, path + (field,)))
        goal = order['objective']
        if goal['kind'] == 'deliver_supply':
            if index['items'][goal['item_id']]['kind'] != 'commodity':
                issues.append(('MILITARY_COMMODITY', path + ('objective', 'item_id'), '军需交付必须使用真实商品'))
            if goal.get('recipient', 'garrison') == 'garrison':
                target = index['garrisons'][index['strongholds'][order['target_stronghold_id']]['garrison_id']]
                if target['organization_id'] != goal.get('recipient_organization_id', office['organization_id']):
                    issues.append(('MILITARY_SUPPLY_AUTHORITY', path + ('objective',), '默认仅供本组织驻军，援助其他组织须明示授权'))
    for number, role in enumerate(pack.get('roles', [])):
        for service in set(role['services']) & set(SERVICES):
            if (role['id'], service) not in services:
                issues.append(('MILITARY_SERVICE_SOURCE', ('roles', number, 'services'), '军事服务须有实际公职或军令定义作为来源'))
    return issues


def _military_plans(pack: dict, index: dict) -> list:
    """NPC 只可依本人公职与初始驻军开始真实受任流程，计划不等于已经产生军团。"""
    issues = []
    players = {scenario['player_actor'] for scenario in pack.get('scenarios', [])}
    for number, actor in enumerate(pack.get('actors', [])):
        plan = actor.get('military_plan')
        if plan is None: continue
        path = ('actors', number, 'military_plan')
        office = index['offices'][plan['office_id']]; order = index['military_orders'][plan['order_id']]
        camp = index['garrisons'][office['garrison_id']]
        if actor['id'] in players or 'travel_plan' in actor:
            issues.append(('MILITARY_PLAN_ACTOR', path, '初始玩家或另有出行计划的人物不能同时自动军政行动'))
        if order['office_id'] != plan['office_id'] or order['garrison_id'] != office['garrison_id'] or camp['organization_id'] != office['organization_id']:
            issues.append(('MILITARY_PLAN_ORDER', path + ('order_id',), '自动军令必须属于本人计划公职的同一驻军与组织'))
        units = {unit['id']: unit for unit in camp['units']}
        for offset, unit_id in enumerate(plan['unit_ids']):
            if unit_id not in units:
                issues.append(('MILITARY_PLAN_UNIT', path + ('unit_ids', offset), '计划部队必须是本驻军初始真实名册'))
        own = [unit for unit in plan['unit_ids'] if units.get(unit, {}).get('commander_actor_id') == actor['id']]
        if not own or (office['command_scope'] == 'unit' and len(plan['unit_ids']) != 1):
            issues.append(('MILITARY_PLAN_SCOPE', path + ('unit_ids',), '本人须实际指挥至少一部，部将权限仅可选择本人一部'))
        role = index['roles'][office['appointing_role_id']]
        if actor['initial_place'] != role['place_id'] or actor['id'] == role['initial_actor']:
            issues.append(('MILITARY_PLAN_PRESENCE', path, '自动受任者须已在授任岗位，且不能自己给自己受任'))
    return issues


def _asset_places(pack: dict, index: dict, table: str, asset: str) -> set:
    """复用已知商店、驻军及军政岗位的场所锚点，未有锚点的公库留给据点清单明确声明。"""
    field = 'inventory_id' if table == 'inventories' else 'wallet_id'
    places = {index['places'][row['place_id']]['settlement_id']
              for name in ('shops', 'garrisons') for row in pack.get(name, []) if row[field] == asset}
    if table == 'wallets':
        for role in pack.get('roles', []):
            terms = list(role.get('activities', {}).values()) + role.get('lessons', [])
            terms += [role[key] for key in ('family', 'sparring') if key in role]
            if any(row['wallet_id'] == asset for row in terms):
                places.add(index['places'][role['place_id']]['settlement_id'])
        for offer in pack.get('offers', []):
            if offer['funding_wallet'] == asset:
                places.add(index['places'][index['roles'][offer['accept_role']]['place_id']]['settlement_id'])
    return places


def _strongholds(pack: dict, index: dict) -> list:
    """城防公产清单须包含本驻军库，并排除私产、共享据点和已有外城归属。"""
    issues, assets, camps = [], set(), set()
    private = {estate[field] for estate in pack.get('estates', []) for field in ('inventory_id', 'wallet_id')}
    for number, hold in enumerate(pack.get('strongholds', [])):
        path = ('strongholds', number); camp = index['garrisons'][hold['garrison_id']]
        if hold['garrison_id'] in camps or index['places'][camp['place_id']]['settlement_id'] != hold['settlement_id']:
            issues.append(('MILITARY_STRONGHOLD_GARRISON', path + ('garrison_id',), '城防须锚定本城唯一真实驻军'))
        camps.add(hold['garrison_id'])
        owner = {'kind': 'organization', 'id': camp['organization_id']}
        for table, field, source in [('inventories', 'public_inventory_ids', 'inventory_id'), ('wallets', 'public_wallet_ids', 'wallet_id')]:
            if camp[source] not in hold[field]:
                issues.append(('MILITARY_STRONGHOLD_ASSETS', path + (field,), '城防公产清单须包括本城驻军供给来源'))
            for offset, asset in enumerate(hold[field]):
                if asset in assets or asset in private or index[table][asset]['owner'] != owner:
                    issues.append(('MILITARY_PUBLIC_ASSET', path + (field, offset), '城防公产须属于本驻军组织，且不能重列或侵占私人财产'))
                assets.add(asset)
                if any(city != hold['settlement_id'] for city in _asset_places(pack, index, table, asset)):
                    issues.append(('MILITARY_ASSET_PLACE', path + (field, offset), '不能把实际位于外城的组织仓库或钱柜列入本城'))
    return issues


def _topology(topology: dict, index: dict, path: tuple) -> list:
    """核对局部节点引用、双向连通和真实材料；坐标运动仍由正式六角规则处理。"""
    issues, nodes = [], {}
    for number, node in enumerate(topology['nodes']):
        if node['id'] in nodes:
            issues.append(('MILITARY_NODE_DUPLICATE', path + ('nodes', number, 'id'), '同一城防的节点 ID 不能重复'))
        nodes[node['id']] = node
        if node['kind'] in ('gate', 'side', 'wall') and node['max_hp'] == 0:
            issues.append(('MILITARY_STRUCTURE_HP', path + ('nodes', number, 'max_hp'), '门墙建筑须具有实际耐久'))
    for field in ('attacker_entry_id', 'defender_entry_id', 'inner_id'):
        if topology[field] not in nodes:
            issues.append(('MILITARY_NODE_REFERENCE', path + (field,), '当前城防中不存在此节点'))
    if (topology['attacker_entry_id'] == topology['defender_entry_id'] or
            nodes.get(topology['inner_id'], {}).get('kind') != 'inner'):
        issues.append(('MILITARY_TOPOLOGY_ENTRY', path + ('inner_id',), '双方入口不能相同，内城须指向内城节点'))
    for number, node in enumerate(topology['supply_node_ids']):
        if nodes.get(node, {}).get('kind') != 'supply':
            issues.append(('MILITARY_SUPPLY_PORT', path + ('supply_node_ids', number), '补给口须指向实际补给节点'))
    edges, neighbors = set(), {node: set() for node in nodes}
    for number, edge in enumerate(topology['edges']):
        at = path + ('edges', number); pair = tuple(sorted((edge['from'], edge['to'])))
        if pair in edges or edge['from'] == edge['to']:
            issues.append(('MILITARY_EDGE_DUPLICATE', at, '双向边不能重复或自连'))
        edges.add(pair)
        for field in ('from', 'to'):
            if edge[field] not in nodes:
                issues.append(('MILITARY_NODE_REFERENCE', at + (field,), '道路端点必须属于当前城防'))
        if edge['from'] in nodes and edge['to'] in nodes:
            neighbors[edge['from']].add(edge['to']); neighbors[edge['to']].add(edge['from'])
        blocker = edge.get('blocker_id', '')
        if blocker and nodes.get(blocker, {}).get('kind') not in ('gate', 'side'):
            issues.append(('MILITARY_BLOCKER', at + ('blocker_id',), '阻挡须引用当前城防的正门或侧门'))
    if reachable(topology['attacker_entry_id'], neighbors) != set(nodes):
        issues.append(('MILITARY_TOPOLOGY_DISCONNECTED', path + ('edges',), '城防全部节点必须由真实双向边连通'))
    rules = topology['rules']
    if index['items'][rules['engine_item_id']]['kind'] != 'commodity':
        issues.append(('MILITARY_COMMODITY', path + ('rules', 'engine_item_id'), '攻城器械须为真实商品'))
    for field in ('engine_materials', 'repair_materials'):
        seen = set()
        for number, material in enumerate(rules[field]):
            item_id = material['item_id']
            if item_id in seen or index['items'][item_id]['kind'] != 'commodity':
                issues.append(('MILITARY_MATERIAL', path + ('rules', field, number, 'item_id'), '每种建造修复材料须为唯一真实商品'))
            seen.add(item_id)
    return issues
