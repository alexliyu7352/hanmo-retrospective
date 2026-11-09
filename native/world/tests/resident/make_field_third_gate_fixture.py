"""青川第三门固定作者桥：保留原母层，显式标注批准的补充初册。"""
from __future__ import annotations

import argparse
import ast
from copy import deepcopy
import hashlib
import json
from pathlib import Path

MOTHER_SHA256 = "f56813be90b990845ae6786eb77339493374bab1c9259c2089917f2069ca1a33"
ALPHA_SHA256 = "53bab3e4dd0e6cfa1c78bc399b8121e0c7bf871f228bb2a3de7cb080e248bc81"
HELPER_SHA256 = "b79e77e95dd1db79cef41213024dad2258845c5855bee07f0ba275dfc2d4b84c"
TERRAIN_SHA256 = {
    "terrain.json": "964351a7d2699416af0ce524ada7923a1f75ba752e3696673f6fa1472ca53544",
    "height.u16": "bcf62166aaac08a368563a8a727fe2a3cd8622a5c85dca0c7a962b7917bc26db",
    "rivers.u8x2": "dfc076c4e63f8cbfb5025a03f2e0d06584252d143815a1e24327497e71cee9b7",
    "land.u8x3": "e4203d6b49864b61f4de200a8d9e428a152cf9e6bacdef4888a4a691a33cf8f4",
}
ACTORS = tuple("actor." + side + "." + role
               for side in ("blackwater", "qingchuan")
               for role in ("bowman", "commander", "officer", "rider"))
ORGANIZATIONS = ("org.blackwater", "org.qingchuan")
OFFICES = ("office.blackwater.army", "office.blackwater.garrison",
           "office.qingchuan.army")


def require(ok: bool, code: str) -> None:
    """原输入不符即停；不采用模型默认、作者名猜测或部分成功。"""
    if not ok:
        raise ValueError(code)


def read_pinned(path: Path, expected: str) -> bytes:
    """读取具名固定原件并核整文件身份；不修改来源或运行时状态。"""
    require(path.is_file() and not path.is_symlink(), "OriginalSourceFileRequired:" + str(path))
    raw = path.read_bytes()
    require(hashlib.sha256(raw).hexdigest() == expected, "OriginalSourceHashMismatch:" + str(path))
    return raw


def field_parameters(helper: Path) -> dict:
    """仅借已冻 helper 的显式参数字典，不调用其旧场景或 admission 函数。"""
    tree = ast.parse(read_pinned(helper, HELPER_SHA256).decode("utf-8"))
    matches = []
    for node in ast.walk(tree):
        if not isinstance(node, ast.Assign) or not isinstance(node.value, ast.Dict):
            continue
        try:
            value = ast.literal_eval(node.value)
        except (ValueError, TypeError):
            continue
        if isinstance(value, dict) and value.get("version") == "field_battle/1":
            matches.append(value)
    require(len(matches) == 1, "UniqueOriginalFieldParameterDictionaryRequired")
    result = matches[0]
    require(len(result) == 38 and sum(type(v) is int for v in result.values()) == 35,
            "CompleteOriginalFieldParametersRequired")
    return result


def original_rows(alpha: dict, table: str, identifiers: tuple[str, ...]) -> dict:
    """原白名单逐 ID 唯一借行；不以同名异源或字典覆盖隐藏重复。"""
    rows = alpha.get(table)
    require(isinstance(rows, list), "OriginalAlphaTableRequired:" + table)
    result = {}
    for identifier in identifiers:
        found = [row for row in rows if row.get("id") == identifier]
        require(len(found) == 1, "UniqueOriginalAlphaRowRequired:" + identifier)
        result[identifier] = deepcopy(found[0])
    return result


def read_original_inputs(mother: Path, alpha_path: Path, helper: Path,
                         terrain_root: Path, target_scenario: str) -> dict:
    """核现代 Complete 母层与青川原行；返回审阅材料，不是可运行目录。"""
    prepared = json.loads(read_pinned(mother, MOTHER_SHA256))
    alpha = json.loads(read_pinned(alpha_path, ALPHA_SHA256))
    require(prepared.get("ok") is True, "SuccessfulOriginalPreparedRequired")
    data = prepared["data"]
    require(target_scenario in data["scenarios"], "ExplicitTargetScenarioRequired")
    scenario = data["scenarios"][target_scenario]
    require(scenario["id"] == target_scenario, "TargetScenarioIdentityMismatch")
    require(scenario.get("world_time", {}).get("profile") == "rational_world_complete_v2",
            "OriginalCompleteMotherRequired")
    require(scenario.get("growth_permanent_contributions", {}).get("profile") ==
            "verified_work_growth_v1", "OriginalGrowthMotherRequired")
    require("field_battle" not in scenario["dynamic_world"], "FieldSelectionAlreadyPresent")
    for filename, digest in TERRAIN_SHA256.items():
        read_pinned(terrain_root / filename, digest)
    terrain = json.loads((terrain_root / "terrain.json").read_text())
    require(terrain["map_id"] == "map.qingchuan", "OriginalQingchuanTerrainRequired")
    for key, filename in (("height", "height.u16"), ("rivers", "rivers.u8x2"),
                          ("land", "land.u8x3")):
        declared = terrain["files"][key]
        require(declared["path"] == filename and declared["sha256"] == TERRAIN_SHA256[filename],
                "OriginalTerrainDeclarationMismatch:" + key)
    imports = {
        "actors": original_rows(alpha, "actors", ACTORS),
        "organizations": original_rows(alpha, "organizations", ORGANIZATIONS),
        "offices": original_rows(alpha, "offices", OFFICES),
        "world_maps": original_rows(alpha, "world_maps", ("map.qingchuan",)),
        "settlements": original_rows(alpha, "settlements", ("city.west", "village.east")),
        "routes": original_rows(alpha, "routes", ("road.west_east", "road.east_west")),
    }
    for table, rows in imports.items():
        for identifier in rows:
            require(identifier not in data.get(table, {}) and
                    identifier not in scenario["enabled"].get(table, []),
                    "OriginalMotherRowConflict:" + identifier)
    for actor in imports["actors"].values():
        require(type(actor.get("initial_age_years")) is int and
                isinstance(actor.get("attributes"), dict) and
                isinstance(actor.get("skills"), dict) and
                isinstance(actor.get("initial_place"), str),
                "OriginalActorFieldsRequired:" + actor["id"])
    for office in imports["offices"].values():
        require(office["organization_id"] in imports["organizations"],
                "OriginalOfficeOrganizationMismatch:" + office["id"])
    return {"target_scenario": target_scenario, "original_imports": imports,
            "field_parameters": field_parameters(helper)}


APPROVAL = "總設計師 2026-10-29 Slack 批准"
APPROVAL_MESSAGE = "1793331657.217219"
RELATIONS_APPROVAL_MESSAGE = "1793332834.456279"
SUPPLEMENT = "Field 守門補充資料，非 alpha 原有"
TEMPLATE_ACTOR = "complete_market::actor.seller"
TEMPLATE_ORG = "complete_market::organization.public_grain"
TEMPLATE_OFFICE = "complete_market::office.public_grain"
MILITARY_ROWS = {
    "places": ("west.street", "west.office", "west.grain", "west.tavern", "west.inn",
               "west.training", "west.estate", "west.camp", "east.entrance", "east.home", "east.camp"),
    "dialogues": tuple("dialog." + side + "." + str(i) for side in ("blackwater", "qingchuan") for i in range(4)),
    "roles": tuple("role." + side + "." + str(i) for side in ("blackwater", "qingchuan") for i in range(4)),
    "garrisons": ("garrison.blackwater", "garrison.qingchuan"),
    "wallets": ("wallet.blackwater.camp", "wallet.qingchuan.camp"),
    "inventories": ("inventory.blackwater.camp", "inventory.qingchuan.camp"),
    "items": ("item.bow", "item.grain", "item.spear", "item.timber"),
    "military_orders": ("order.blackwater.army", "order.blackwater.defense", "order.qingchuan.army"),
    "strongholds": ("stronghold.blackwater", "stronghold.qingchuan"),
    "siege_topologies": ("topology.blackwater", "topology.qingchuan"),
    "battlefields": ("field.camp", "field.valley"),
    "military_objectives": ("military_goal.camp", "military_goal.valley"),
    "troop_types": ("troop.archer", "troop.cavalry", "troop.infantry"),
}


def unique(rows: list, key: str, value: str) -> dict:
    found = [r for r in rows if r.get(key) == value]
    require(len(found) == 1, "UniqueTemplateRequired:" + value)
    return found[0]


def put(rows: dict, key: str, value: object) -> None:
    require(key not in rows, "AuthorRowConflict:" + key)
    rows[key] = value


def import_originals(data: dict, scenario: dict, alpha: dict, imports: dict) -> None:
    """固定原行和其文本/旗标闭包；绝不借另一个青川版本或覆盖母层。"""
    text_keys = set()

    def texts(value: object) -> None:
        if isinstance(value, str) and value in alpha["texts"]:
            text_keys.add(value)
        elif isinstance(value, list):
            for child in value:
                texts(child)
        elif isinstance(value, dict):
            for child in value.values():
                texts(child)

    for table, rows in imports.items():
        require(table in data and isinstance(data[table], dict), "OriginalMotherTableRequired:" + table)
        for identifier, original in rows.items():
            row = deepcopy(original)
            if table in ("routes", "settlements"):
                row["world_map_id"] = "map.qingchuan"
            if table == "world_maps":
                row["fine_terrain"] = "content/world/terrain/map.qingchuan/"
                for cell in row["cells"]:
                    if "blocked_flag" in cell:
                        flag = cell["blocked_flag"]
                        require(flag in alpha["flags"], "OriginalMapFlagRequired:" + flag)
                        require(flag not in data["flags"] or data["flags"][flag] == alpha["flags"][flag], "MapFlagConflict")
                        data["flags"][flag] = deepcopy(alpha["flags"][flag])
                        enabled_flags = scenario["enabled"].setdefault("flags", [])
                        if flag not in enabled_flags:
                            enabled_flags.append(flag)
            texts(row)
            put(data[table], identifier, row)
            enabled = scenario["enabled"].setdefault(table, [])
            require(identifier not in enabled, "EnabledRowConflict:" + identifier)
            enabled.append(identifier)
    for key in sorted(text_keys):
        require(key not in data["texts"] or data["texts"][key] == alpha["texts"][key], "OriginalTextConflict:" + key)
        data["texts"][key] = alpha["texts"][key]


def add_actor_initials(data: dict, scenario: dict, imports: dict) -> None:
    """仅外观/健康借批准模板；年龄、属性、技能、地点逐值保原 alpha。"""
    initial = scenario["initial"]
    foundation = scenario["actor_foundation"]
    template = deepcopy(unique(initial["actors"], "actor_id", TEMPLATE_ACTOR))
    actor_template = deepcopy(unique(foundation["actor_states"], "actor_id", TEMPLATE_ACTOR))
    condition_template = deepcopy(unique(foundation["initial_condition_seeds"], "actor_id", TEMPLATE_ACTOR))
    require(template["appearance_definition"]["kind"] == "fixed" and template["alive"] is True and
            template["available"] is True and actor_template["hp"] == condition_template["hp"] and
            actor_template["age_basis"]["kind"] == "DeclaredAgeAtStart", "ApprovedActorTemplateShapeRequired")
    for actor_id in ACTORS:
        original = imports["actors"][actor_id]
        require(not any(r["actor_id"] == actor_id for r in initial["actors"]), "InitialActorConflict")
        row = deepcopy(template)
        row.update(actor_id=actor_id, age_years=original["initial_age_years"],
                   place_id=original["initial_place"], attributes=deepcopy(original["attributes"]),
                   skills=deepcopy(original["skills"]), initial_goods=deepcopy(original["initial_goods"]))
        if "combat" in original:
            row["combat"] = deepcopy(original["combat"])
        initial["actors"].append(row)
        actor = data["actors"][actor_id]
        actor["appearance_definition"] = deepcopy(row["appearance_definition"])
        actor["appearance_note"] = SUPPLEMENT + "；" + APPROVAL + "；" + APPROVAL_MESSAGE
        state = deepcopy(actor_template)
        state["actor_id"] = actor_id
        state["age_basis"]["years"] = original["initial_age_years"]
        state["age_basis"]["source"].update(row_key=actor_id, scenario_id=scenario["id"])
        state["appearance_source_ref"]["id"] = actor_id
        state["identity_definition_ref"]["id"] = actor_id
        state["placement"]["place_ref"] = original["initial_place"]
        seed_key = "initial-condition:" + actor_id
        state["condition_seed_refs"] = [{"seed_key": seed_key}]
        foundation["actor_states"].append(state)
        condition = deepcopy(condition_template)
        condition.update(actor_id=actor_id, seed_key=seed_key)
        condition["source"].update(row_key=actor_id, scenario_id=scenario["id"])
        foundation["initial_condition_seeds"].append(condition)
    for table in ("wallets", "inventories", "garrisons", "offices", "strongholds", "roles"):
        target = initial.setdefault(table, [])
        for identifier, row in imports[table].items():
            if table == "roles":
                out = {"role_id": identifier, "actor_id": row["initial_actor"], "enabled": True}
            elif table in ("offices", "strongholds"):
                out = {"id": identifier}
            else:
                fields = {"wallets": ("id", "owner", "initial_balance"),
                          "inventories": ("id", "owner", "capacity", "goods"),
                          "garrisons": ("id", "organization_id", "units")}[table]
                out = {key: deepcopy(row[key]) for key in fields}
            require(not any(r.get("id", r.get("role_id")) == identifier for r in target), "InitialRowConflict:" + identifier)
            target.append(out)


def add_organization_initials(scenario: dict, imports: dict) -> None:
    """原 runtime6 两阶段设施初册：借固定母模板逐字段重绑，不预造任职。"""
    dynamic = scenario["dynamic_world"]
    orgs = dynamic["organization"]
    lifecycle = scenario["organization_runtime"]["lifecycle_catalog"]
    facilities = scenario["economic_facility_capacity_sources"]["economic_facility_catalog"]
    conditions = scenario["production_facility_conditions"]["production_facility_conditions_catalog"]["seeds"]
    assets = dynamic["local_economy"]["production_management"]["assets"]
    base = "complete_market::"
    for org_id in ORGANIZATIONS:
        side = org_id[len("org."):] if org_id.startswith("org.") else org_id
        garrison = imports["garrisons"]["garrison." + side]
        camp = garrison["place_id"]
        require(garrison["organization_id"] == org_id and camp in imports["places"], "OriginalCampBindingRequired")
        prefix = "field_gate." + side
        asset_id, seed_id = prefix + ".asset", prefix + ".seed"
        facility_id, condition_id = prefix + ".facility", prefix + ".condition"
        definition_id, opening_id = prefix + ".definition", prefix + ".opening"
        usage_id, capacity_id = prefix + ".own_use", prefix + ".workbench"
        policy_id = prefix + ".lifecycle"
        org = deepcopy(orgs["organizations"][TEMPLATE_ORG])
        org.update(id=org_id, headquarters_site=camp,
                   office_ids=[i for i in OFFICES if imports["offices"][i]["organization_id"] == org_id])
        put(orgs["organizations"], org_id, org)
        for office_id in org["office_ids"]:
            office = deepcopy(orgs["offices"][TEMPLATE_OFFICE])
            require(office["status"] == "vacant" and office["holder_actor_id"] is None and
                    office["epoch"] == 0 and office["grant_ids"] == [], "VacantOfficeTemplateRequired")
            office.update(id=office_id, organization_id=org_id)
            put(orgs["offices"], office_id, office)
        policy = deepcopy(lifecycle["seed_policies"][base + "lifecycle.public_grain"])
        policy.update(id=policy_id, required_office_ids=[],
                      initial_facility_seed_ref={"seed_row_key": seed_id, "original_asset_ref": asset_id})
        put(lifecycle["seed_policies"], policy_id, policy)
        lifecycle["authored_seeds"].append({"organization_id": org_id, "lifecycle_seed_policy_ref": {"id": policy_id, "version": 1}})
        asset = deepcopy(assets[base + "asset.public_grain_hq"])
        asset.update(id=asset_id, site_id=camp, source_key=prefix + ".asset_source")
        asset["owner"]["id"] = org_id
        put(assets, asset_id, asset)
        definition = deepcopy(next(r for r in facilities["definitions"] if r["definition_ref"]["id"] == base + "facility.public_grain_hq.definition"))
        definition["definition_ref"]["id"] = definition_id
        definition["availability_policy_ref"]["id"] = opening_id
        definition["base_binding"]["condition_ref"] = condition_id
        definition["base_binding"]["original_slot_definition_ref"]["id"] = definition_id
        definition["capacity_unit_id"] = capacity_id
        definition["physical_capacity_identity"] = prefix + ".physical_workbench"
        # 版型/损修规则保母模板原身份；只新资产、设施和本地用途行重绑。
        definition["usage_policy_ref"]["id"] = usage_id
        facilities["definitions"].append(definition)
        installed = deepcopy(unique(facilities["initial_rows"], "facility_id", base + "facility.public_grain_hq"))
        installed.update(facility_id=facility_id, place_ref=camp)
        installed["definition_ref"]["id"] = definition_id
        installed["initial_title_owner"]["id"] = org_id
        installed["original_asset_row"].update(id=asset_id, site_id=camp, source_key=asset["source_key"])
        installed["original_asset_row"]["owner"]["id"] = org_id
        installed["row_ref"]["seed_row_key"] = seed_id
        facilities["initial_rows"].append(installed)
        for table, old, new in (("opening_rules", "opening", opening_id), ("usage_policies", "own_use", usage_id)):
            row = deepcopy(next(r for r in facilities[table] if r["rule_ref"]["id"] == base + "facility.public_grain_hq." + old))
            row["rule_ref"]["id"] = new
            facilities[table].append(row)
        condition = deepcopy(conditions[base + "condition.public_grain_hq"])
        condition.update(condition_ref=condition_id, original_asset_ref=asset_id)
        condition["base_capacity_by_slot"][0]["capacity_unit_id"] = capacity_id
        condition["original_installation_seed_ref"]["seed_row_key"] = seed_id
        put(conditions, condition_id, condition)


def build_fixture(mother: Path, alpha_path: Path, helper: Path,
                  terrain_root: Path, target_scenario: str) -> tuple[dict, dict]:
    """只组合已批准作者输入；任何运行状态必须随后由原真实命令产生。"""
    checked = read_original_inputs(mother, alpha_path, helper, terrain_root, target_scenario)
    envelope = json.loads(read_pinned(mother, MOTHER_SHA256))
    alpha = json.loads(read_pinned(alpha_path, ALPHA_SHA256))
    data = envelope["data"]
    original_data = deepcopy(data)
    scenario = data["scenarios"][target_scenario]
    imports = checked["original_imports"]
    for table, identifiers in MILITARY_ROWS.items():
        imports[table] = original_rows(alpha, table, identifiers)
    import_originals(data, scenario, alpha, imports)
    add_actor_initials(data, scenario, imports)
    add_organization_initials(scenario, imports)
    dynamic = scenario["dynamic_world"]
    dynamic["field_battle"] = checked["field_parameters"]
    # 两个原 fixed-duration owner；registration 的 source_schema2 不是作者版号。
    action_profiles = scenario["world_time"]["action_time_profiles"]
    for action in ("office_accept", "army_form"):
        require(not any(row["action_kind"] == action for row in action_profiles),
                "ExistingMilitaryActionProfileConflict:" + action)
        action_profiles.append({"action_kind": action, "profile_id": "military_action_time", "version": 1})
    # 原 Office/Form concrete 的完整选型；是 scenario 顶层 profile，不是 dynamic version。
    for module, profile in (
        ("military_action_time", "original_military_action_time_v1"),
        ("military_authority_time", "original_military_authority_time_v1"),
        ("military_action_resources", "original_military_action_resources_v1"),
        ("military_action_conditions", "original_military_action_conditions_v2"),
        ("military_formation_reservations", "original_military_formation_reservations_v2"),
        ("military_battle_lifecycle", "original_military_battle_lifecycle_v1"),
        ("organization_service_effects", "original_organization_service_effects_v1"),
    ):
        put(scenario, module, {"profile": profile})
    require(dynamic["travel"] == {"version": 4}, "OriginalTravel4Required")
    for module, version in (("group_transport", 6), ("transport_manifest_bindings", 2),
                            ("carrier_location", 1), ("movement_group_member_releases", 1),
                            ("movement_group_member_losses", 2), ("movement_boundary_contacts", 1)):
        put(dynamic, module, {"version": version})
    for capability in ("military", "world_terrain", "personal_combat", "dialogue"):
        require(capability in alpha["required_capabilities"], "OriginalCapabilityRequired")
        if capability not in data["required_capabilities"]:
            data["required_capabilities"].append(capability)
    require("character_injury_sources" not in scenario and "personal_battle_lifecycle" not in scenario,
            "ExistingInjurySelectionConflict")
    scenario["character_injury_sources"] = {"version": 1}
    scenario["personal_battle_lifecycle"] = {"profile": "original_personal_battle_lifecycle_v1"}
    foundation = scenario["actor_foundation"]
    require(not any(k in foundation for k in ("injury_capability_map", "original_battle_injury_rules", "original_military_wound_rule")), "ExistingInjuryBindingsConflict")
    foundation["injury_capability_map"] = {"version": 3, "profile": "original_injury_grade_effects_v1"}
    admissions = dynamic["director"]["admissions"]
    require(set(admissions) == {"complete_market::admission.buyer"}, "OriginalMotherAdmissionRequired")
    admission = admissions["complete_market::admission.buyer"]
    require(admission["kind"] == "all_actors", "OriginalMotherAdmissionKindRequired")
    for actor in ACTORS:
        require(actor not in admission["excluded_actor_ids"], "AdmissionExclusionConflict")
        admission["excluded_actor_ids"].append(actor)
    # 沿原 helper 已有明确青川 NPC 意图；不调用它的旧 admission 或选人代码。
    commander = data["actors"]["actor.qingchuan.commander"]
    require("military_plan" not in commander, "OriginalCommanderPlanConflict")
    commander["military_plan"] = {"office_id": "office.qingchuan.army", "order_id": "order.qingchuan.army",
                                 "unit_ids": ["unit.qingchuan.infantry", "unit.qingchuan.archer"],
                                 "food_quantity": 800, "money": 300}
    policies = []
    for road_id, road in imports["routes"].items():
        for org_id in ORGANIZATIONS:
            policies.append({"id": "field_gate.passage." + road_id + "." + org_id,
                             "version": 1, "boundary": {"kind": "road_edge", "id": road_id,
                             "map_id": "map.qingchuan", "from_site_id": road["origin"], "to_site_id": road["destination"]},
                             "organization_id": org_id, "mode": "army", "admission": "allowed", "tariff": "none"})
    put(scenario, "field_group_passage", {"schema_version": 1, "provenance": APPROVAL, "policies": policies})
    # 独立明确批准的唯一 P3 初值；通行许可和既有征讨军令均不作此来源。
    first, second = "org.blackwater", "org.qingchuan"
    require(first < second and all(org in dynamic["organization"]["organizations"]
                                   for org in (first, second)), "OriginalPoliticalPairRequired")
    put(scenario, "political_relations", {"schema_version": 1, "initial_relations": [
        {"factions": {"first": first, "second": second}, "relation": "hostile"}]})
    put(data["texts"], "field_gate.initial_relations.provenance",
        SUPPLEMENT + "；" + APPROVAL + "；" + RELATIONS_APPROVAL_MESSAGE)
    put(data["texts"], "field_gate.supplement.provenance", SUPPLEMENT + "；" + APPROVAL + "；" + APPROVAL_MESSAGE)
    # 母层目录逐行全保留；唯一原场景明确扩展，不删市场来源或换玩家。
    for table, rows in original_data.items():
        if isinstance(rows, dict) and table != "scenarios":
            require(all(data[table][key] == value for key, value in rows.items()), "OriginalMotherRowsChanged:" + table)
    require(scenario["player_actor"] == original_data["scenarios"][target_scenario]["player_actor"], "OriginalPlayerChanged")
    for actor_id in ACTORS:
        original = imports["actors"][actor_id]
        current = unique(scenario["initial"]["actors"], "actor_id", actor_id)
        require(current["age_years"] == original["initial_age_years"] and current["attributes"] == original["attributes"] and
                current["skills"] == original["skills"] and current["place_id"] == original["initial_place"], "OriginalActorValuesChanged")
    canonical = (json.dumps(data, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()
    envelope.update(content_id="test.field.third_gate", content_version="1.0.0-test",
                    content_digest=hashlib.sha256(canonical).hexdigest())
    evidence = {"kind": "native-test-fixture-author-input-not-runtime-validation", "label": SUPPLEMENT,
                "authority": APPROVAL, "slack_message": APPROVAL_MESSAGE,
                "mother_sha256": MOTHER_SHA256, "alpha_sha256": ALPHA_SHA256, "helper_sha256": HELPER_SHA256,
                "terrain_sha256": TERRAIN_SHA256, "target_scenario": target_scenario,
                "military_action_profiles_source": [
                    "resident/domains/military/office_exact_action_time.cpp::exact_office_action_registration",
                    "resident/domains/military/formation_exact_action_time.cpp::exact_formation_action_registration"],
                "military_original_selection_sources": [
                    "kernel/native_military_state.cpp",
                    "kernel/native_military_battle_lifecycle.cpp",
                    "kernel/native_organization_runtime.cpp",
                    "domains/military/native_formation_original.cpp:342",
                    "domains/military/native_office_original.cpp:23/38"],
                "actor_template": TEMPLATE_ACTOR, "organization_template": TEMPLATE_ORG,
                "template_source_paths": ["scenario.initial.actors[complete_market::actor.seller]",
                    "scenario.actor_foundation.actor_states[complete_market::actor.seller]",
                    "scenario.actor_foundation.initial_condition_seeds[complete_market::actor.seller]",
                    "scenario.organization_runtime.lifecycle_catalog[complete_market::lifecycle.public_grain]",
                    "scenario.dynamic_world.local_economy.production_management.assets[complete_market::asset.public_grain_hq]",
                    "scenario.economic_facility_capacity_sources.economic_facility_catalog[complete_market::facility.public_grain_hq]",
                    "scenario.production_facility_conditions.production_facility_conditions_catalog.seeds[complete_market::condition.public_grain_hq]"],
                "author_schema_admission": "Separate current author schema/link validation required; this is not an installed author package",
                "native_required_modules": {"travel": 4, "group_transport": 6,
                    "transport_manifest_bindings": 2, "carrier_location": 1,
                    "movement_group_member_releases": 1, "movement_group_member_losses": 2,
                    "movement_boundary_contacts": 1},
                "movement_boundary_flags": "Original native prepare captures actual data.flags with final catalog identity; no authored owner; author schema synchronization remains central",
                "office_template": TEMPLATE_OFFICE, "required_office_ids": [],
                "actor_preserved_fields": ["initial_age_years", "attributes", "skills", "initial_place"],
                "supplemental_fields": ["appearance", "health", "runtime6 camp facility initial sources"],
                "content_digest": envelope["content_digest"],
                "runtime_state_preseeded": False, "native_validation_performed": False,
                "political_relations_authority": {"provenance": APPROVAL,
                    "slack_message": RELATIONS_APPROVAL_MESSAGE,
                    "scope": "Only org.blackwater/org.qingchuan in this Field fixture; all other defaults unchanged"},
                "unresolved_original_inputs": []}
    return envelope, evidence


def main() -> None:
    """显式固定原件输入；不覆盖输出，不运行原生程序或声称守门已通过。"""
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("mother", "alpha", "helper", "terrain-root", "output", "evidence"):
        parser.add_argument("--" + name, required=True, type=Path)
    parser.add_argument("--target-scenario", required=True)
    args = parser.parse_args()
    require(args.output != args.evidence and not args.output.exists() and not args.evidence.exists(), "FreshOutputPathsRequired")
    fixture, evidence = build_fixture(args.mother, args.alpha, args.helper, args.terrain_root, args.target_scenario)
    # 原 P3 双模块由此真实作者块派生；缺源或异形仍拒，不借道路许可补关系。
    relations = fixture["data"]["scenarios"][args.target_scenario].get("political_relations")
    require(relations == {"schema_version": 1, "initial_relations": [{
        "factions": {"first": "org.blackwater", "second": "org.qingchuan"},
        "relation": "hostile"}]}, "FieldThirdGateApprovedPoliticalRelationsRequired")
    for path, value in ((args.output, fixture), (args.evidence, evidence)):
        with path.open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, sort_keys=True, indent=2)
            stream.write("\n")


if __name__ == "__main__":
    main()
