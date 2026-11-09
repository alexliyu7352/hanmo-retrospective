"""固定军阵作者题面的有限见证校验；只检查四步抽象布置，不读取真实兵员或运行训练。"""


def check_drills(pack: dict, index: dict) -> list:
    """Schema后核组角色、坐标和四步满分见证；军营引用仍沿统一typed ref。"""
    issues = []
    for number, definition in enumerate(pack.get('drill_challenges', [])):
        code = _definition(definition, pack['texts'])
        if code:
            issues.append((code, ('drill_challenges', number), '军阵题面必须具有唯一角色、合法5×4坐标和四步100分见证'))
    for number, camp in enumerate(pack.get('garrisons', [])):
        if 'training_drill_id' in camp and ('training_wallet_id' not in camp or camp['training_drill_id'] not in index.get('drill_challenges', {})):
            issues.append(('MILITARY_DRILL_REFERENCE', ('garrisons', number, 'training_drill_id'), '军阵必须属于显式训练服务并引用实际题面'))
    return issues


def _cell(value):
    """固定棋盘第二轴仅0至3；整数严格来自同一作者Schema，不将四舍五入当合法格。"""
    return isinstance(value, list) and len(value) == 2 and all(type(x) is int for x in value) and 0 <= value[0] < 5 and 0 <= value[1] < 4


def _cells(values):
    """公开格集不重复且有界，不允许空集合伪装可达目标。"""
    return 1 <= len(values) <= 20 and all(_cell(v) for v in values) and len({tuple(v) for v in values}) == len(values)


def _definition(value, texts):
    """只执行固定四步见证和四项布尔目标，无表达式、搜索或任意脚本。"""
    groups = {g['id']: g for g in value['groups']}
    if len(groups) != 3 or {g['kind'] for g in groups.values()} != {'foot', 'ranged', 'cavalry'}:
        return 'DRILL_GROUPS'
    board = {key: tuple(g['origin']) for key, g in groups.items()}
    if len(set(board.values())) != 3 or any(not _cell(g['origin']) or not _cells(g['targets']) for g in groups.values()):
        return 'DRILL_CELLS'
    goals = value['objectives']; supply = goals['supply']; passage = goals['passage']; screen = goals['screen']; cavalry = goals['cavalry']
    if (groups.get(supply['group_id'], {}).get('kind') != 'foot' or passage['group_id'] not in groups or
            groups.get(screen['front_group_id'], {}).get('kind') != 'foot' or groups.get(screen['ranged_group_id'], {}).get('kind') != 'ranged' or
            groups.get(cavalry['group_id'], {}).get('kind') != 'cavalry'):
        return 'DRILL_ROLE'
    if not _cell(supply['point']) or any(not _cells(values) for values in [supply['guard_cells'], passage['cells'], cavalry['bad_cells']]):
        return 'DRILL_CELLS'
    pairs = [(tuple(pair['front']), tuple(pair['rear'])) for pair in screen['pairs']]
    if len(set(pairs)) != len(pairs) or any(not _cell(pair['front']) or not _cell(pair['rear']) or pair['front'] == pair['rear'] for pair in screen['pairs']):
        return 'DRILL_SCREEN'
    keys = [value['prompt_key']] + [g['text_key'] for g in groups.values()] + [g['text_key'] for g in goals.values()]
    if any(not isinstance(texts.get(k), str) or not texts[k] or len(texts[k]) > 4096 for k in keys):
        return 'DRILL_TEXT'
    horse = cavalry['group_id']; bad = {tuple(c) for c in cavalry['bad_cells']}; safe = board[horse] not in bad
    for move in value['solution']:
        group = move['group_id']; target = tuple(move['target'])
        if group not in groups or not _cell(move['target']) or move['target'] not in groups[group]['targets'] or target in board.values():
            return 'DRILL_SOLUTION'
        board[group] = target
        if group == horse and target in bad: safe = False
    if not (list(board[supply['group_id']]) in supply['guard_cells'] and list(board[passage['group_id']]) in passage['cells'] and
            (board[screen['front_group_id']], board[screen['ranged_group_id']]) in pairs and safe):
        return 'DRILL_UNSOLVED'
    return ''
