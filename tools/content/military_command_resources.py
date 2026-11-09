"""原指挥资源固定规则和真实100点军种准入，不能借此发放人物战法卡。"""
def check_military_command_resources(pack: dict, index: dict) -> list:
    """完整顺序轮为前置，所选营地引用的军种须明确100上限。"""
    issues=[]
    for i,scenario in enumerate(pack.get("scenarios",[])):
        if "military_command_resources" not in scenario:
            continue
        path=("scenarios",i,"military_command_resources")
        if scenario["military_command_resources"]!={"profile":"entry_tactics_recovery_05_v1"}:
            issues.append(("MILITARY_COMMAND_RESOURCES_POLICY",path,"须原05固定资源规则"))
        if (scenario.get("military_sequential_rounds")!={"profile":"entry_initiative_05_v1"}
                or not {"military_command_resources_v1","military_sequential_rounds_v1"}<=set(pack.get("required_capabilities",[]))):
            issues.append(("MILITARY_COMMAND_RESOURCES_PREREQUISITE",path,"须完整原顺序轮及兵法来源能力"))
        for id in scenario.get("enabled",{}).get("garrisons",[]):
            for unit in index.get("garrisons",{}).get(id,{}).get("units",[]):
                troop=index.get("troop_types",{}).get(unit.get("troop_type_id"))
                if troop is not None and troop.get("tactic_capacity")!=100:
                    issues.append(("MILITARY_COMMAND_RESOURCES_CAPACITY",path,"所选军种必须明确100点上限"))
    return issues
