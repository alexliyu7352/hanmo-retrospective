"""生命终点作者风险显式准入；不把启用死因记录等同已授继承或公权。"""
PROFILE = {'profile': 'verified_military_deaths_v1',
           'player_death': 'await_explicit_continuation'}


def check_family_life(pack, index):
    """只核所选情景的明确策略及能力；实际人物死亡必须在运行时由原伤亡账证明。"""
    del index
    issues = []
    for number, scenario in enumerate(pack.get('scenarios', [])):
        if 'family_life' not in scenario:
            continue
        path = ('scenarios', number, 'family_life')
        if scenario['family_life'] != PROFILE:
            issues.append(('FAMILY_LIFE_PROFILE', path,
                           '只支持已核军事伤亡与死亡后等待明确接续；不支持自动授继承或自然寿终'))
        if not isinstance(scenario.get('dynamic_world'), dict):
            issues.append(('FAMILY_LIFE_NATIVE', path, '生命终点记录须使用同源原生动态世界'))
        if 'family_life_v1' not in pack.get('required_capabilities', []):
            issues.append(('FAMILY_LIFE_CAPABILITY', path, '须明确声明 family_life_v1 能力，旧核心不能忽略'))
    return issues
