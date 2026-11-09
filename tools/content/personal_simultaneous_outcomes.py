"""显式双方倒地只选择原普通个人战目标，不替作者改护送或补人物来源。"""

def check_personal_simultaneous_outcomes(pack: dict, index: dict) -> list:
    """按情景实际启用目标核有限唯一集合；能力存在不隐式启用其他场景。"""
    issues = []
    for number, scenario in enumerate(pack.get("scenarios", [])):
        policy = scenario.get("personal_simultaneous_outcomes")
        if policy is None:
            continue
        path = ("scenarios", number, "personal_simultaneous_outcomes")
        if (not isinstance(policy, dict) or set(policy) != {"version", "objective_ids"}
                or type(policy.get("version")) is not int or policy["version"] != 1
                or not isinstance(policy.get("objective_ids"), list)
                or not 1 <= len(policy["objective_ids"]) <= 32
                or any(not isinstance(x, str) or not x for x in policy["objective_ids"])
                or len(set(policy["objective_ids"])) != len(policy["objective_ids"])):
            issues.append(("PERSONAL_SIMULTANEOUS_OUTCOME_POLICY", path, "须明确版本1与有限唯一原目标"))
            continue
        if (not {"personal_combat", "native_dynamic_world", "personal_simultaneous_outcomes_v1"} <= set(pack.get("required_capabilities", []))
                or scenario.get("dynamic_world", {}).get("profile") != "local_society_v1"):
            issues.append(("PERSONAL_SIMULTANEOUS_OUTCOME_CAPABILITY", path, "须显式个人战与双方倒地能力"))
        enabled = set(scenario.get("enabled", {}).get("personal_objectives", []))
        for i, objective_id in enumerate(policy["objective_ids"]):
            objective = index.get("personal_objectives", {}).get(objective_id)
            if (objective_id not in enabled or not objective
                    or objective.get("kind") not in ("sparring", "skirmish")
                    or "escort_model" in objective):
                issues.append(("PERSONAL_SIMULTANEOUS_OUTCOME_OBJECTIVE", path + ("objective_ids", i), "须选择实际启用的切磋或遭遇目标，不能覆盖护送关键失败"))
    return issues
