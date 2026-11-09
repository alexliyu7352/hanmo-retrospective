"""首个内河船只只核显式来源与原产权引用；不在作者工具创造人物、货物或航路。"""


def check_naval(pack: dict, index: dict) -> list:
    """Schema之后核同情景船型/泊地/货舱关系；尚未实现沉没、救生、漂失和初始舰队。"""
    issues = []
    for n, scenario in enumerate(pack.get('scenarios', [])):
        naval = scenario.get('military_naval')
        if naval is None:
            continue
        path = ('scenarios', n, 'military_naval')
        if (naval['profile'] != 'afloat_inland_v1' or
                scenario.get('dynamic_world', {}).get('profile') != 'local_society_v1' or
                scenario.get('skill_profile') != 'full_sixteen_v1' or
                not {'military', 'military_naval_v1'} <= set(pack.get('required_capabilities', []))):
            issues.append(('NAVAL_PROFILE', path, '实船只支持显式内河动态军旅与完整十六技能'))
        if naval['world_map_id'] != scenario.get('world_map_id'):
            issues.append(('NAVAL_MAP', path + ('world_map_id',), '舰船必须绑定当前情景同一实际地形地图'))
        hulls, berths, vessels, holds = {}, {}, set(), set()
        for j, hull in enumerate(naval['hulls']):
            at = path + ('hulls', j)
            if hull['id'] in hulls or hull['minimum_crew'] > hull['person_capacity']:
                issues.append(('NAVAL_HULL', at, '船型须唯一且工作船员不能超过人员容量'))
            hulls[hull['id']] = hull
        for j, berth in enumerate(naval['berths']):
            if berth['place_id'] in berths:
                issues.append(('NAVAL_BERTH', path + ('berths', j), '泊地不得重复声明'))
            berths[berth['place_id']] = berth
        combat = naval.get('combat_policy')
        if combat is not None:
            at = path + ('combat_policy',)
            if 'military_naval_combat_v1' not in pack.get('required_capabilities', []):
                issues.append(('NAVAL_COMBAT_CAPABILITY', at, '舟战必须显式声明独立战斗能力'))
            if combat.get('commander_losses') is not None and 'military_naval_commanders_v1' not in pack.get('required_capabilities', []):
                issues.append(('NAVAL_COMMANDER_CAPABILITY', at, '具名军官暴露必须显式声明独立伤亡能力'))
            board = index.get('battlefields', {}).get(combat['battlefield_id'])
            if board is None:
                issues.append(('NAVAL_COMBAT_BOARD', at, '舟战必须引用现有军事六角战场'))
            else:
                cells = {(c['q'], c['r']): c for c in board['cells']}
                for entry in board['spawns'] + board.get('exits', []):
                    cell = cells.get(tuple(entry['position']))
                    if not cell or cell['terrain'] != 'water' or cell.get('blocks_movement', False):
                        issues.append(('NAVAL_WATER_ENTRY', at, '舟战入口和退路必须是实际可通行水格'))
                if combat['allow_withdrawal'] and {e['side'] for e in board.get('exits', [])} != {0, 1}:
                    issues.append(('NAVAL_WITHDRAWAL', at, '允许撤退的舟战须具备双方真实水上出口'))
            ram_hulls = set()
            for ram in combat['ram']:
                if ram['hull_id'] not in hulls or ram['hull_id'] in ram_hulls:
                    issues.append(('NAVAL_RAM_HULL', at, '冲撞定义须引用本情景船型且每型只定义一次'))
                ram_hulls.add(ram['hull_id'])
        repair = naval.get('repair_policy')
        if repair is not None:
            at = path + ('repair_policy',)
            if combat is None or 'military_naval_repairs_v1' not in pack.get('required_capabilities', []):
                issues.append(('NAVAL_REPAIR_CAPABILITY', at, '修缮必须显式声明Naval3工法且保原舟战前置'))
            rules = {}
            for j, rule in enumerate(repair['rules']):
                where = at + ('rules', j)
                hull = hulls.get(rule['hull_id'])
                if (rule['id'] in rules or hull is None or
                        rule['minimum_crew'] > hull['person_capacity'] or
                        rule['integrity_gain'] > hull['max_integrity'] or
                        rule['afloat_integrity_limit'] > hull['max_integrity']):
                    issues.append(('NAVAL_REPAIR_RULE', where, '工法须引用本情景实船型并保持人员、增量和浮修上限'))
                rules[rule['id']] = rule
                materials = set()
                for material in rule['materials']:
                    if material['item_id'] in materials or material['item_id'] not in index.get('items', {}):
                        issues.append(('NAVAL_REPAIR_MATERIAL', where, '修缮实物须是唯一具名Common物品'))
                    materials.add(material['item_id'])
            plans = set()
            for j, plan in enumerate(repair.get('npc_plans', [])):
                key = (plan['actor_id'], plan['rule_id'])
                rule = rules.get(plan['rule_id'])
                hull = hulls.get(rule['hull_id']) if rule else None
                if (key in plans or plan['actor_id'] not in index.get('actors', {}) or
                        not hull or plan['repair_below'] > hull['max_integrity']):
                    issues.append(('NAVAL_REPAIR_NPC_PLAN', at + ('npc_plans', j), 'NPC维修意图只接受实际人物、同工法及船损范围'))
                plans.add(key)
        plans_seen, assigned_ships = set(), set()
        ship_definitions = {ship['id']: ship for ship in naval['vessels']}
        for j, plan in enumerate(naval.get('fleet_plans', [])):
            at = path + ('fleet_plans', j)
            actor = index.get('actors', {}).get(plan['actor_id'], {})
            schedule = actor.get('military_plan', {})
            order = index.get('military_orders', {}).get(plan['order_id'], {})
            fort = index.get('strongholds', {}).get(order.get('target_stronghold_id'), {})
            camp = index.get('garrisons', {}).get(order.get('garrison_id'), {})
            if (combat is None or plan['actor_id'] in plans_seen or
                    schedule.get('order_id') != plan['order_id'] or
                    order.get('objective', {}).get('kind') != 'relieve' or
                    not camp or fort.get('garrison_id') != camp.get('id') or
                    len(plan['vessel_ids']) != len(schedule.get('unit_ids', []))):
                issues.append(('NAVAL_DEFENSE_PLAN', at, '水上部署须绑定本NPC原本营防守军令与真实部队'))
            if any(type(plan.get(key, 0)) is not int or not 0 <= plan.get(key, 0) <= 1000000 for key in ('departure_food', 'departure_money')):
                issues.append(('NAVAL_DEPARTURE_SUPPLY', at, '驻舟粮钱目标须在原补给命令范围内，仍从本营实际公库支付'))
            plans_seen.add(plan['actor_id'])
            for ship_id in plan['vessel_ids']:
                ship = ship_definitions.get(ship_id, {})
                if (ship_id in assigned_ships or not ship or
                        ship.get('berth_id') != camp.get('place_id') or
                        ship.get('owner') != {'kind': 'organization', 'id': camp.get('organization_id')}):
                    issues.append(('NAVAL_PLAN_SHIP', at, '计划只能引用同营同主实船且不可重用'))
                assigned_ships.add(ship_id)
        # 使用Snapshot同一初态覆盖；无对应初态时才读原定义，绝不另设货账。
        inventories = dict(index.get('inventories', {}))
        inventories.update({row['id']: row for row in scenario.get('initial', {}).get('inventories', [])})
        for j, ship in enumerate(naval['vessels']):
            at = path + ('vessels', j)
            hull, berth = hulls.get(ship['hull_id']), berths.get(ship['berth_id'])
            inventory = inventories.get(ship['cargo_inventory_id'])
            if ship['id'] in vessels or ship['cargo_inventory_id'] in holds:
                issues.append(('NAVAL_ASSET_DUPLICATE', at, '实船与货舱只能占用一次'))
            vessels.add(ship['id']); holds.add(ship['cargo_inventory_id'])
            if (hull is None or berth is None or ship['integrity'] > hull['max_integrity'] or
                    berth['water_kind'] not in hull['waters']):
                issues.append(('NAVAL_VESSEL_SOURCE', at, '实船须命中本情景船型与兼容水域泊地，船体不能超额'))
            if inventory is None or inventory['owner'] != ship['owner']:
                issues.append(('NAVAL_HOLD_OWNER', at + ('cargo_inventory_id',), '实船货舱必须是同Owner的真实Common容器'))
    return issues
