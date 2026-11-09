"""检查同行授权和道路对白出处；只校验静态定义，不模拟人物行旅。"""

def check_companions(pack: dict, index: dict) -> list:
    """定位错误角色、远程服务动作与不合理的玩家自动行程，保留可编辑草稿。"""
    issues=[]
    players={scenario['player_actor'] for scenario in pack['scenarios']}
    for number,role in enumerate(pack['roles']):
        if 'companion_cost_per_tick' in role and 'join_party' not in role['services']:
            issues.append(('COMPANION_AUTHORITY',('roles',number,'companion_cost_per_tick'),'同行费用必须有当面同行约定'))
    for number,actor in enumerate(pack['actors']):
        if actor.get('travel_plan') and actor['id'] in players:
            issues.append(('PLAYER_AUTO_PLAN',('actors',number,'travel_plan'),'玩家行旅由输入授权，不能配置NPC自动行程'))
        road=actor.get('road_dialogue_id')
        if not road:
            continue
        dialogue=index['dialogues'][road]
        role=index['roles'][dialogue['context_role']]
        if role['initial_actor']!=actor['id']:
            issues.append(('ROAD_DIALOGUE_OWNER',('actors',number,'road_dialogue_id'),'道路对白必须属于实际说话人物'))
        for node in dialogue['nodes']:
            if node['kind']=='action':
                issues.append(('ROAD_DIALOGUE_ACTION',('actors',number,'road_dialogue_id'),'道路会面不允许执行原岗位服务；离开由会面命令处理'))
            if node['kind']=='say' and node['speaker'] not in ('$provider','$player'):
                issues.append(('ROAD_DIALOGUE_SPEAKER',('actors',number,'road_dialogue_id'),'道路对白只能引用当前双方，不能指定远处说话人'))
    return issues
