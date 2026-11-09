"""原入战统率/兵种技能规则准入，不以作者整数替代永久人物资格。"""
def check_military_commanded_damage(pack: dict, index: dict) -> list:
    """只明确完整13和固定公式；所有启用营地的原单位必须真实配置训练/疲劳。"""
    issues=[]
    for number, scenario in enumerate(pack.get("scenarios", [])):
        policy=scenario.get("military_commanded_damage")
        if policy is None:
            continue
        path=("scenarios",number,"military_commanded_damage")
        if policy!={"profile":"entry_commanded_damage_05_v1"}:
            issues.append(("MILITARY_COMMANDED_POLICY",path,"须明确原05统率与兵种技能伤害公式"))
        if (scenario.get("military_combat_conditions")!={"profile":"entry_fatigue_03_v1"}
                or not {"military_commanded_damage_v1","military_combat_conditions_v1"}<=set(pack.get("required_capabilities",[]))):
            issues.append(("MILITARY_COMMANDED_PREREQUISITE",path,"须完整13及真实原入战条件"))
        for garrison_id in scenario.get("enabled",{}).get("garrisons",[]):
            garrison=index.get("garrisons",{}).get(garrison_id)
            if garrison is None:
                continue  # 原通用引用门报告缺目录；本门不伪造默认训练值。
            for unit in garrison.get("units",[]):
                if any(type(unit.get(k)) is not int or not 0<=unit[k]<=100 for k in ("initial_training","initial_fatigue")):
                    issues.append(("MILITARY_COMMANDED_TRAINING_SOURCE",path,"所选原单位须显式成对训练/疲劳，不能到战场才补默认值"))
    return issues
