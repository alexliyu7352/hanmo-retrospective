"""入战疲劳只启用原03四档，不藉新规则修改旧攻守或随机。"""
def check_military_combat_conditions(pack: dict, index: dict) -> list:
    """作者显式依赖完整12；实际选中野战的伤害模型仍由原生目录严格核验。"""
    issues=[]
    for number,scenario in enumerate(pack.get('scenarios',[])):
        policy=scenario.get('military_combat_conditions')
        if policy is None: continue
        path=('scenarios',number,'military_combat_conditions')
        if not isinstance(policy,dict) or policy!={'profile':'entry_fatigue_03_v1'}:
            issues.append(('MILITARY_COMBAT_CONDITIONS_POLICY',path,'须明确原03入战疲劳四档，不开放隐式倍率'))
        if (scenario.get('military_garrison_rest',{}).get('profile')!='supplied_garrison_rest_v1'
                or not {'military_combat_conditions_v1','military_garrison_rest_v1'}<=set(pack.get('required_capabilities',[]))):
            issues.append(('MILITARY_COMBAT_CONDITIONS_PREREQUISITE',path,'须原完整军事12与真实部队条件来源'))
    return issues
