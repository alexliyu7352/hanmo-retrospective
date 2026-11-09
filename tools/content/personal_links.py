"""校验个人战的定义关系、出生几何和真实装备来源；不补入缺省表，不执行世界规则。"""

BODY_RADIUS = 18
BAG_CAPACITY = 20
TEACH_MASTERY = 80


def check_personal(pack: dict, index: dict) -> list:
    """在 Schema 之后补充跨字段语义，未知引用返回字段问题而非索引异常。"""
    issues = []
    for number, item in enumerate(pack.get('items', [])):
        if 'armor' in item and (item['kind'] != 'commodity' or 'weapon' in item):
            issues.append(('PERSONAL_ARMOR_ITEM', ('items', number, 'armor'), '护具必须为单一实物类别，不能兼作武器'))
        weapon = item.get('weapon')
        if weapon is not None and (weapon['minimum_range'] > weapon['maximum_range'] or item['kind'] != 'commodity'):
            issues.append(('PERSONAL_WEAPON_RANGE', ('items', number, 'weapon'), '武器必须是实物商品，且内射程不能超过外射程'))
    for number, skill in enumerate(pack.get('personal_skills', [])):
        for field in ('requires_stationary', 'range_modifier'):
            if field in skill and skill['kind'] != 'strike':
                issues.append(('PERSONAL_STRIKE_CONDITION', ('personal_skills', number, field), '原地与射程条件仅允许主动进攻技能'))
        if 'area' in skill and (skill['kind'] != 'strike' or skill['weapons'] != ['spear']):
            issues.append(('PERSONAL_STRIKE_AREA', ('personal_skills', number, 'area'), '有限区域仅属于长兵主动进攻，不能扩为任意武器'))
        if 'guard_facing' in skill and skill['kind'] != 'guard':
            issues.append(('PERSONAL_GUARD_FACING', ('personal_skills', number, 'guard_facing'), '正面守势仅属于守势技能'))
        if 'defend_recovery_bonus' in skill and skill['kind'] != 'heart':
            issues.append(('PERSONAL_DEFEND_RECOVERY', ('personal_skills', number, 'defend_recovery_bonus'), '额外防御恢复仅属于实际配置的心法'))
        if 'hit_bonus_bp' in skill and skill['kind'] != 'strike':
            issues.append(('PERSONAL_HIT_BONUS', ('personal_skills', number, 'hit_bonus_bp'), '命中修正只能属于主动进攻技能'))
        if 'counter' in skill and (skill['kind'] != 'guard' or 'bow' in skill['weapons'] or skill.get('required_skill_level') != {'skill_id': 'martial', 'level': 3}):
            issues.append(('PERSONAL_COUNTER_REQUIREMENT', ('personal_skills', number), '反击仅用于近身守势且须明确武艺三级前置'))
        if (skill['status'] == 'none') != (skill['status_rounds'] == 0):
            issues.append(('PERSONAL_STATUS_DURATION', ('personal_skills', number, 'status_rounds'), '无状态期限必须为零，有状态必须具有有效轮数'))
        if len(set(skill['weapons'])) != len(skill['weapons']):
            issues.append(('PERSONAL_DUPLICATE_WEAPON', ('personal_skills', number, 'weapons'), '功法适用武器不能重复'))
    for number, objective in enumerate(pack.get('personal_objectives', [])):
        if 'escort_model' in objective and objective['kind'] != 'escort':
            issues.append(('PERSONAL_ESCORT_MODEL', ('personal_objectives', number, 'escort_model'), '出口护送模型只能由护送目标显式启用'))
        if 'facing_damage' in objective and objective['facing_damage'] != {'side_bp': 11000, 'back_bp': 12500}:
            issues.append(('PERSONAL_FACING_DAMAGE', ('personal_objectives', number, 'facing_damage'), '仅支持当前显式侧背候选倍率'))
    for number, arena in enumerate(pack.get('personal_battlefields', [])):
        issues.extend(_battlefield(arena, ('personal_battlefields', number)))
    for number, actor in enumerate(pack.get('actors', [])):
        issues.extend(_actor(actor, index, ('actors', number)))
    for number, role in enumerate(pack.get('roles', [])):
        issues.extend(_role(role, index, ('roles', number)))
    return issues


def _battlefield(arena: dict, path: tuple) -> list:
    """出生身体和退出中心必须可站立，双方须有空间；触发圆可伸到场地边缘。"""
    issues = []
    for number, obstacle in enumerate(arena['obstacles']):
        if obstacle['x'] + obstacle['width'] > arena['width'] or obstacle['y'] + obstacle['height'] > arena['height']:
            issues.append(('PERSONAL_OBSTACLE_BOUNDS', path + ('obstacles', number), '障碍范围超出当前场地'))
    for field in ('spawns', 'exits'):
        rows = arena[field]
        if {row['side'] for row in rows} != {0, 1}:
            issues.append(('PERSONAL_BOTH_SIDES', path + (field,), '双方都必须具备出生位和退出点'))
        for number, row in enumerate(rows):
            if not _point_valid(arena, row['position']):
                issues.append(('PERSONAL_POINT_BLOCKED', path + (field, number, 'position'), '人物身体不能在此落点完整站立'))
            if field == 'spawns':
                for previous in rows[:number]:
                    if _distance_squared(row['position'], previous['position']) < (BODY_RADIUS * 2) ** 2:
                        issues.append(('PERSONAL_POINT_OVERLAP', path + (field, number, 'position'), '出生人物的身体不能相互覆盖'))
                        break
    return issues


def _point_valid(arena: dict, point: list) -> bool:
    """按半径十八的圆形身体检查矩形障碍；与运行几何一致地允许相切和圆角余地。"""
    x, y = point
    if x < BODY_RADIUS or y < BODY_RADIUS or x > arena['width'] - BODY_RADIUS or y > arena['height'] - BODY_RADIUS:
        return False
    for rect in arena['obstacles']:
        if not rect['blocks_movement']:
            continue
        nearest = [max(rect['x'], min(x, rect['x'] + rect['width'])), max(rect['y'], min(y, rect['y'] + rect['height']))]
        if _distance_squared(point, nearest) < BODY_RADIUS * BODY_RADIUS:
            return False
    return True


def _distance_squared(a: list, b: list) -> int:
    """使用整数平方距离比较接触，避免作者预览与运行边界因开方取整不同。"""
    return (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2


def _actor(actor: dict, index: dict, path: tuple) -> list:
    """装备须来自本人初始背包；知识独立于物品，不因没有相应武器而否定已学功法。"""
    issues = []
    goods = {}
    weight = 0
    for number, good in enumerate(actor.get('initial_goods', [])):
        item = index.get('items', {}).get(good['item_id'])
        at = path + ('initial_goods', number, 'item_id')
        if item is None:
            issues.append(_reference(at))
            continue
        if good['item_id'] in goods:
            issues.append(('PERSONAL_DUPLICATE_GOODS', at, '本人初始背包每种物品只配置一个数量'))
        goods[good['item_id']] = good['quantity']
        weight += good['quantity'] * item['weight']
    if weight > BAG_CAPACITY:
        issues.append(('PERSONAL_BAG_CAPACITY', path + ('initial_goods',), '初始随身物品超过个人背包容量二十'))
    if actor.get('combat'):
        combat = actor['combat']
        if combat['weapon_id']:
            weapon = index.get('items', {}).get(combat['weapon_id'])
            if weapon is None:
                issues.append(_reference(path + ('combat', 'weapon_id')))
            elif 'weapon' not in weapon or goods.get(combat['weapon_id'], 0) < 1:
                issues.append(('PERSONAL_WEAPON_OWNERSHIP', path + ('combat', 'weapon_id'), '初始装备必须是本人背包实际拥有的武器'))
        if combat.get('armor_id'):
            armor = index.get('items', {}).get(combat['armor_id'], {})
            if 'armor' not in armor or goods.get(combat['armor_id'], 0) < 1:
                issues.append(('PERSONAL_ARMOR_OWNERSHIP', path + ('combat', 'armor_id'), '初始护具必须是本人背包实际物品'))
        learned = set()
        for number, row in enumerate(combat['learned']):
            at = path + ('combat', 'learned', number, 'skill_id')
            if row['skill_id'] not in index.get('personal_skills', {}):
                issues.append(_reference(at))
            if row['skill_id'] in learned:
                issues.append(('PERSONAL_DUPLICATE_SKILL', at, '初始已学功法不能以重复记录制造不同熟练度'))
            learned.add(row['skill_id'])
    if 'road_conflict' in actor:
        if not actor.get('combat', {}).get('weapon_id'):
            issues.append(('PERSONAL_COMBAT_REQUIRED', path + ('road_conflict',), '道路对敌人物必须具备真实武器与战斗配置'))
        issues.extend(_battle_link(actor['road_conflict'], index, path + ('road_conflict',), False))
        policy = actor['road_conflict'].get('cargo_policy')
        if policy is not None:
            at = path + ('road_conflict', 'cargo_policy')
            template = index.get('task_templates', {}).get(policy['task_template_id'], {})
            if template.get('template') != 'procurement' or template.get('goal', {}).get('item_id') != policy['item_id']:
                issues.append(('CARGO_POLICY_TEMPLATE', at + ('task_template_id',), '道路货物政策须指向同一实物的采购模板'))
            if index.get('items', {}).get(policy['item_id'], {}).get('kind') != 'commodity':
                issues.append(('CARGO_POLICY_ITEM', at + ('item_id',), '道路处置只作用于真实商品，不接受消息物品'))
            if policy['transfer_cap'] + policy['destroy_cap'] <= 0:
                issues.append(('CARGO_POLICY_CAPS', at, '显式道路货物政策至少声明一项正的处置上限；无处置应省略政策'))
            if not actor.get('travel_plan'):
                issues.append(('CARGO_POLICY_TRAVEL_SOURCE', at, '首批来源必须由真实道路行旅人物的相遇触发，不能只声明战场'))
    return issues


def _role(role: dict, index: dict, path: tuple) -> list:
    """授课与切磋须有对应当面服务、真实教师和收费授权；仅声明服务也不能凭空生成条款。"""
    issues = []
    lessons = role.get('lessons', [])
    courses = role.get('skill_courses', [])
    if bool(courses) != ('study_skill' in role['services']):
        issues.append(('SKILL_STUDY_AUTHORITY', path + ('skill_courses',), '普通技艺课程与修业服务必须同时声明'))
    seen_courses = set()
    for number, course in enumerate(courses):
        at = path + ('skill_courses', number)
        if course['skill_id'] in seen_courses:
            issues.append(('SKILL_STUDY_DUPLICATE', at + ('skill_id',), '同一岗位不能声明冲突的同技能课程'))
        seen_courses.add(course['skill_id'])
        issues.extend(_wallet(role, course['wallet_id'], index, at + ('wallet_id',)))
    if ('sparring' in role) != ('duel_start' in role['services']):
        issues.append(('PERSONAL_SERVICE_AUTHORITY', path + ('sparring',), '切磋条款与切磋服务必须同时声明'))
    if bool(lessons) != ('learn_skill' in role['services']):
        issues.append(('PERSONAL_SERVICE_AUTHORITY', path + ('lessons',), '授课条款与学习服务必须同时声明'))
    if 'sparring' not in role and not lessons and not courses:
        return issues
    actor = index.get('actors', {}).get(role['initial_actor'])
    if actor is None:
        issues.append(_reference(path + ('initial_actor',)))
        return issues
    if actor['initial_place'] != role['place_id']:
        issues.append(('PERSONAL_PROVIDER_ABSENT', path + ('place_id',), '教师初始位置必须与实际服务场所一致'))
    if 'sparring' in role:
        if not actor.get('combat', {}).get('weapon_id'):
            issues.append(('PERSONAL_COMBAT_REQUIRED', path + ('sparring',), '切磋对象必须具有真实战斗配置'))
        issues.extend(_battle_link(role['sparring'], index, path + ('sparring',), True))
        issues.extend(_wallet(role, role['sparring']['wallet_id'], index, path + ('sparring', 'wallet_id')))
    learned = {row['skill_id']: row['mastery'] for row in actor.get('combat', {}).get('learned', [])}
    seen = set()
    for number, lesson in enumerate(lessons):
        at = path + ('lessons', number)
        skill = lesson['skill_id']
        if skill not in index.get('personal_skills', {}):
            issues.append(_reference(at + ('skill_id',)))
        elif learned.get(skill, 0) < TEACH_MASTERY:
            issues.append(('PERSONAL_TEACHER_MASTERY', at + ('skill_id',), '教师须实际掌握本功法至少八成熟练度'))
        if skill in seen:
            issues.append(('PERSONAL_DUPLICATE_LESSON', at + ('skill_id',), '同一岗位不能为同一功法声明相互冲突的授课条款'))
        seen.add(skill)
        issues.extend(_wallet(role, lesson['wallet_id'], index, at + ('wallet_id',)))
    return issues


def _battle_link(terms: dict, index: dict, path: tuple, sparring: bool) -> list:
    """核对实际场地和目标类别，护卫额外保留主角与保护对象各自的出生位。"""
    issues = []
    arena = index.get('personal_battlefields', {}).get(terms['battlefield_id'])
    objective = index.get('personal_objectives', {}).get(terms['objective_id'])
    if arena is None:
        issues.append(_reference(path + ('battlefield_id',)))
    if objective is None:
        issues.append(_reference(path + ('objective_id',)))
    elif (objective['kind'] == 'sparring') != sparring:
        issues.append(('PERSONAL_OBJECTIVE_KIND', path + ('objective_id',), '切磋和道路危险冲突不能互用目标定义'))
    elif objective['kind'] == 'escort' and arena is not None and sum(row['side'] == 0 for row in arena['spawns']) < 2:
        issues.append(('PERSONAL_ESCORT_SPAWNS', path + ('battlefield_id',), '护卫场地必须同时容纳本人和保护对象'))
    return issues


def _wallet(role: dict, wallet_id: str, index: dict, path: tuple) -> list:
    """收费接收方只能是当前岗位代表的个人或组织，零费用也不允许声明外人钱包。"""
    wallet = index.get('wallets', {}).get(wallet_id)
    if wallet is None:
        return [_reference(path)]
    owner = {'kind': 'organization', 'id': role['organization_id']} if role['organization_id'] is not None else {'kind': 'actor', 'id': role['initial_actor']}
    if wallet['owner'] != owner:
        return [('PERSONAL_FEE_AUTHORITY', path, '收费钱包必须属于岗位实际代表的个人或组织')]
    return []


def _reference(path: tuple) -> tuple:
    """统一返回未知关键定义诊断，保持作者字段位置可供编辑器导航。"""
    return ('PERSONAL_REFERENCE', path, '个人战引用的关键定义不存在')
