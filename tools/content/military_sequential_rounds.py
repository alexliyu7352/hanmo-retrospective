"""只显式选择完整M14之上的原入战顺序轮，不修改旧轮次或补造资格。"""
def check_military_sequential_rounds(pack: dict, index: dict) -> list:
    """固定策略、完整资格能力和原场景M14三者必须同启。"""
    issues=[]
    for i, scenario in enumerate(pack.get("scenarios",[])):
        if "military_sequential_rounds" not in scenario:
            continue
        path=("scenarios",i,"military_sequential_rounds")
        if scenario["military_sequential_rounds"]!={"profile":"entry_initiative_05_v1"}:
            issues.append(("MILITARY_SEQUENTIAL_POLICY",path,"须原入战统率先手固定策略"))
        if (scenario.get("military_commanded_damage")!={"profile":"entry_commanded_damage_05_v1"}
                or not {"military_commanded_damage_v1","military_sequential_rounds_v1"}<=set(pack.get("required_capabilities",[]))):
            issues.append(("MILITARY_SEQUENTIAL_PREREQUISITE",path,"须完整M14真实原入战资格及两能力"))
    return issues
