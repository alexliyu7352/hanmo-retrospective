"""由实际 Catalog.read_prepared 输出制作独立 S0 运行输入；不改原正式包或复制资产。"""
from __future__ import annotations

import copy
import hashlib
import json
import pathlib
import sys


def main() -> None:
    """保留既有规则和来源，显式修改测试政策、最小初态及新输入身份。"""
    source, destination = map(pathlib.Path, sys.argv[1:])
    original = source.read_bytes()
    prepared = json.loads(original)
    if prepared.get("ok") is not True:
        raise ValueError("A successful Catalog.read_prepared result is required")
    data = copy.deepcopy(prepared["data"])
    scenario = data["scenarios"]["society::scenario.qingchuan_spring_reserve"]
    dynamic = scenario["dynamic_world"]
    original_director = dynamic["director"]
    livelihood = copy.deepcopy(original_director["subjects"]["society::actor.study_swordswoman"])
    livelihood.pop("named_membership_policy_id", None)
    livelihood["templates"] = ["private_livelihood"]
    livelihood["report_source_keys"] = ["source.market"]
    observe = {
        "templates": ["maintain_commitment"],
        "request_arrear_after_ticks": 1,
        "decline_work_after_ticks": 2,
        "leave_after_ticks": 4,
    }
    policies = {"private_adult_life": livelihood, "observation_only": observe}
    admissions = {}
    for organization, source_key in [("city", "supply.city"), ("sect", "supply.sect")]:
        policy = copy.deepcopy(observe)
        policy["organization_id"] = "society::org." + organization
        policy["templates"] = ["maintain_commitment", "await_adjustment"]
        policy["report_source_keys"] = [source_key]
        policies[organization + "_member"] = policy
        admissions[organization + "_members"] = {
            "kind": "organization_members",
            "organization_id": "society::org." + organization,
            "policy_id": organization + "_member",
            "minimum_age_years": 18,
        }
    admissions["adult_life"] = {
        "kind": "all_actors",
        "policy_id": "private_adult_life",
        "minimum_age_years": 18,
        "excluded_actor_ids": [
            "society::actor.tutor", "society::actor.merchant", "society::actor.sect_steward"
        ],
    }
    admissions["remaining_observers"] = {"kind": "all_actors", "policy_id": "observation_only"}
    dynamic["director"] = {
        key: original_director[key]
        for key in ["candidates_per_subject", "decision_budget", "retry_delay_ticks", "review_interval_ticks"]
    }
    dynamic["director"].update(registry_version=1, policies=policies, admissions=admissions)
    dynamic["affairs"]["obligation_needs_version"] = 1
    dynamic["affairs"]["obligation_policies"] = {
        "salary." + name: {
            "id": "salary." + name,
            "obligation_id": "obligation." + name + "_salary",
            "organization_id": "society::org." + name,
            "semantic_key": "salary." + name,
        }
        for name in ["city", "sect"]
    }
    for actor in scenario["initial"]["actors"]:
        if actor["actor_id"] in ["society::actor.study_caravanner", "society::actor.study_swordswoman"]:
            actor["initial_goods"] = []
            actor["place_id"] = "society::place.sword_court"
        if actor["actor_id"] == "society::actor.home_keeper":
            actor["age_years"] = 12
            data["actors"][actor["actor_id"]]["initial_age_years"] = 12
        if actor["actor_id"] == "society::actor.tutor":
            actor["available"] = False
    for wallet in scenario["initial"]["wallets"]:
        if wallet["id"] in ["society::wallet.study_caravanner", "society::wallet.study_swordswoman"]:
            wallet["initial_balance"] = 0
    # 与原生effective_catalog相同，只保留本代表情景已启用的非被动记录。
    passive = {"actors", "items", "dialogues", "personal_skills", "personal_battlefields", "personal_objectives", "battlefields", "troop_types", "military_objectives", "siege_topologies", "etiquette_challenges", "bargain_challenges", "drill_challenges", "texts"}
    for table, enabled in scenario["enabled"].items():
        if table not in passive and table in data and isinstance(data[table], dict):
            data[table] = {key: value for key, value in data[table].items() if key in enabled}
    canonical = json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    fixture = {
        "ok": True,
        "data": data,
        "content_id": "test.s0.shared_world_foundation",
        "content_version": "1.0.0-test",
        "content_digest": hashlib.sha256(canonical.encode()).hexdigest(),
        "schema_sha256": prepared["schema_sha256"],
    }
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(fixture, ensure_ascii=False, separators=(",", ":")) + "\n")
    manifest = {
        "kind": "S0 native integration fixture, not an installed author package",
        "source_prepared_sha256": hashlib.sha256(original).hexdigest(),
        "source_content_id": prepared["content_id"],
        "fixture_sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "new_semantics": ["director_registry/1", "obligation_needs/1"],
        "unchanged_rules": "Existing native growth, travel, resources, task and organization algorithms",
        "scope": "Two working adults, two organizations, multiple places, one minor and one unavailable actor",
        "author_schema_admission": "Separate central schema check required; native decoder acceptance is not author-editor acceptance",
    }
    destination.with_suffix(".manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    print(manifest["fixture_sha256"])


if __name__ == "__main__":
    main()
