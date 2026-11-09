"""核验地形入口与庄园人产关系；与运行装载器一样拒绝没有实际来源的配置。"""


def check_world(pack, index):
    """检查已通过字段和引用校验的定义，输出可定位作者记录的问题。"""
    issues = []
    for number, role in enumerate(pack['roles']):
        family = role.get('family')
        if family:
            owner = {'kind': 'actor', 'id': role['initial_actor']} if role['organization_id'] is None else {'kind': 'organization', 'id': role['organization_id']}
            if 'family_adopt' not in role['services'] or index['wallets'][family['wallet_id']]['owner'] != owner:
                issues.append(('FAMILY_AUTHORITY', ('roles', number, 'family'), '收养服务须由在场办理人代表，费用进入其真实钱柜'))
            if family.get('enable_travel_plan', False) and 'travel_plan' not in index['actors'][family['prototype_id']]:
                issues.append(('FAMILY_TRAVEL_PLAN', ('roles', number, 'family', 'enable_travel_plan'), '启用收养人物行旅须明确引用具有合法行旅计划的原型'))
        elif 'family_adopt' in role['services']:
            issues.append(('FAMILY_SERVICE', ('roles', number, 'services'), '收养服务须配置原型、年龄、手续时间和钱柜'))
    for number, world in enumerate(pack.get('world_maps', [])):
        path = ('world_maps', number)
        size = world['cell_size']
        if world['width'] % size or world['height'] % size or len(world['cells']) != (world['width'] // size) * (world['height'] // size):
            issues.append(('MAP_CELLS', path, '地图宽高必须整除导航尺度，地形数量须覆盖整个区域'))
        for offset, cell in enumerate(world['cells']):
            crossing = cell.get('crossing')
            if crossing and cell['terrain'] != ('mountain' if crossing == 'pass' else 'water'):
                issues.append(('MAP_CROSSING', path + ('cells', offset), '山关须在山地，渡口和桥须在水面'))
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'world_map_id' not in scenario:
            continue
        world = index['world_maps'][scenario['world_map_id']]
        size = world['cell_size']
        if world['width'] % size or world['height'] % size or len(world['cells']) != (world['width'] // size) * (world['height'] // size):
            continue
        for town in pack['settlements']:
            if not _foot_entry(world, town['map_xy']):
                issues.append(('MAP_ENTRY', ('scenarios', number, 'world_map_id'), '据点入口必须在本地图的可步行地形上：' + town['id']))
    people = set()
    assets = set()
    for number, estate in enumerate(pack.get('estates', [])):
        path = ('estates', number)
        owner = {'kind': 'actor', 'id': estate['owner_actor_id']}
        if estate['manager_actor_id'] not in estate['resident_actor_ids']:
            issues.append(('ESTATE_MANAGER', path + ('manager_actor_id',), '初始管事须为真实庄园居民'))
        for table, field in [('wallets', 'wallet_id'), ('inventories', 'inventory_id')]:
            if index[table][estate[field]]['owner'] != owner or estate[field] in assets:
                issues.append(('ESTATE_ASSETS', path + (field,), '钱柜和仓储须独立归庄园主所有'))
            assets.add(estate[field])
        for offset, person in enumerate(estate['resident_actor_ids']):
            if person in people or index['actors'][person]['initial_place'] != estate['place_id']:
                issues.append(('ESTATE_RESIDENT', path + ('resident_actor_ids', offset), '居民须真实在此，不能重复分属两庄园'))
            people.add(person)
        duties = set()
        for offset, row in enumerate(estate['initial_duties']):
            if row['actor_id'] not in estate['resident_actor_ids'] or row['actor_id'] in duties or (row['actor_id'] == estate['manager_actor_id'] and row['duty'] != 'idle'):
                issues.append(('ESTATE_DUTY', path + ('initial_duties', offset), '每位真实居民仅有一项分工，管事不兼领劳动'))
            duties.add(row['actor_id'])
        for item in [estate['food_item_id'], estate['output_item_id'], estate['construction']['material_item_id']]:
            if index['items'][item]['kind'] != 'commodity': issues.append(('ESTATE_ITEM', path, '庄园口粮、产出和建材须为实际商品'))
        if any(level > estate['construction']['max_level'] for level in estate['initial_facilities'].values()):
            issues.append(('ESTATE_LEVEL', path + ('initial_facilities',), '初始设施不能超过本庄园建设上限'))
    return issues


def _foot_entry(world, point):
    """只核验整数据点入口的相邻格通行，寻路与行程积分仍由运行地形模块负责。"""
    x, y = point
    if not 0 <= x < world['width'] or not 0 <= y < world['height']:
        return False
    size = world['cell_size']
    columns, rows = [x // size], [y // size]
    if x and x % size == 0:
        columns.append(columns[0] - 1)
    if y and y % size == 0:
        rows.append(rows[0] - 1)
    defaults = {'plain': 10, 'road': 8, 'forest': 18, 'mountain': 0,
                'water': 0, 'ford': 26, 'bridge': 8, 'pass': 18}
    rates = world.get('costs', {}).get('foot', {})
    for row in rows:
        for column in columns:
            cell = world['cells'][row * (world['width'] // size) + column]
            if cell['terrain'] == 'water' and cell.get('crossing') not in ('ford', 'bridge'):
                return False
            kind = cell.get('crossing', cell['terrain'])
            if rates.get(kind, defaults[kind]) == 0:
                return False
    return True
