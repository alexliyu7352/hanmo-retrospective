"""只把固定作者源的战场地理引用闭包追加到唯一共用配置。"""
from __future__ import annotations

from copy import deepcopy


# 原作者记录白名单；不可混用 journey_life 的同名不同坐标记录。
FIELD_BATTLE_AUTHOR_IDS = {
    "world_maps": ("map.qingchuan",),
    "settlements": ("city.west", "village.east"),
    "routes": ("road.west_east", "road.east_west"),
    "places": (
        "west.street", "west.office", "west.grain", "west.tavern",
        "west.inn", "west.training", "west.estate", "west.camp",
        "east.entrance", "east.home", "east.camp",
    ),
}


def add_field_battle_author_data(data: dict, original_alpha: dict) -> None:
    """完整保留原记录及地图格；仅按合同追加细地形引用，冲突即拒绝。"""
    # 地图引用只取固定原作者场景，不改原源，也不切共用场景的旅行模式。
    author_scenario = next(row for row in original_alpha["scenarios"] if row["id"] == "scenario.alpha_life")
    author_map = author_scenario["world_map_id"]
    if author_map not in FIELD_BATTLE_AUTHOR_IDS["world_maps"]:
        raise ValueError("Original alpha world map is outside imported closure")
    enabled = data["scenarios"]["society::scenario.qingchuan_spring_reserve"]["enabled"]
    names: set[str] = set()
    flags: set[str] = set()
    for table, ids in FIELD_BATTLE_AUTHOR_IDS.items():
        source = {row["id"]: row for row in original_alpha[table]}
        for identifier in ids:
            if identifier in data[table] or identifier in enabled.get(table, []):
                raise ValueError("Field battle author import would replace " + identifier)
            row = deepcopy(source[identifier])
            if table in ("settlements", "routes"):
                # 新工程引用明确从该记录的原启用场景派生，不把单图 catalog 大小当来源。
                if "enabled" in author_scenario and identifier not in author_scenario["enabled"].get(table, []):
                    raise ValueError("Original alpha record is outside its author scope")
                row["world_map_id"] = author_map
            if "name_key" in row:
                names.add(row["name_key"])
            if table == "world_maps":
                row["fine_terrain"] = "content/world/terrain/map.qingchuan/"
                flags.update(cell["blocked_flag"] for cell in row["cells"] if "blocked_flag" in cell)
            data[table][identifier] = row
            enabled.setdefault(table, []).append(identifier)
    for key in sorted(names):
        value = original_alpha["texts"][key]
        if key in data["texts"] and data["texts"][key] != value:
            raise ValueError("Field battle text conflict " + key)
        data["texts"][key] = value
    # 原世界格引用的旗标也是同一作者输入，不增加任何事件或玩法结果。
    for key in sorted(flags):
        value = original_alpha["flags"][key]
        if key in data["flags"] and data["flags"][key] != value:
            raise ValueError("Field battle flag conflict " + key)
        data["flags"][key] = value
    dynamic = data["scenarios"]["society::scenario.qingchuan_spring_reserve"]["dynamic_world"]
    if "field_battle" in dynamic:
        raise ValueError("Field battle parameters would replace an existing selection")
    # 显式携带原合同 §2、§6、§7、§10 全部参数；不以模型默认补缺，亦不冒完整能力登记。
    dynamic["field_battle"] = {
        "version": "field_battle/1", "window_s": 21600, "step_s": 10,
        "length_m": 4000, "width_m": 3000, "cell_m": 25,
        "deployment_length_m": 600, "deployment_width_m": 1600,
        "deployment_inset_m": 300, "deployment_min_percent": 40,
        "exit_radius_m": 800, "anchor_shift_step_m": 500,
        "anchor_shift_limit_m": 4000, "courier_m_per_minute": 500,
        "minimum_order_delay_s": 60,
        "pulse_s": 60, "round_equiv_s": 3600,
        "morale_loss_bp": 20000, "commander_exposure_bp": 3000,
        "max_segments": 4,
        "infantry_m_per_minute": 50, "archer_m_per_minute": 50,
        "cavalry_m_per_minute": 150, "charge_m_per_minute": 300,
        "rout_speed_bp": 16000,
        "melee_contact_m": 5, "archer_min_range_m": 30,
        "archer_max_range_m": 180,
        "ai_retreat_fit_percent": 40, "ai_retreat_morale": 30,
        "ai_support_morale": 50, "ai_eval_s": 600, "ai_advance_hold_s": 600,
        "rejoin_distance_m": 200, "rejoin_stopped_s": 300,
        "max_formations_per_side": 32,
        # 原表列依次为步、弓、骑；零仅表示原不可通行地形。
        "terrain_speed_bp": {
            "Plain": [10000, 10000, 10000],
            "Field": [10000, 10000, 10000],
            "Forest": [6000, 6000, 4000],
            "Rough": [6000, 6000, 5000],
            "Steep": [0, 0, 0],
            "Stream": [4000, 4000, 4000],
            "Water": [0, 0, 0],
            "Road": [10000, 10000, 10000],
            "Bridge": [10000, 10000, 10000],
            "Ford": [4000, 4000, 4000],
            "Village": [6000, 6000, 5000],
            "Wall": [0, 0, 0],
            "Gate": [0, 0, 0],
        },
        "terrain_cover": {
            "Plain": 0, "Field": 0, "Forest": 20, "Rough": 10,
            "Steep": 0, "Stream": 0, "Water": 0, "Road": 0,
            "Bridge": 0, "Ford": 0, "Village": 25, "Wall": 0, "Gate": 0,
        },
    }


# 原军政引用闭包。旧 topology/field 仅供原军令引用校验，不用于新会战执行。
FIELD_BATTLE_MILITARY_IDS = {
    "actors": tuple("actor." + faction + "." + role for faction in ("blackwater", "qingchuan")
                    for role in ("bowman", "commander", "officer", "rider")),
    "dialogues": tuple("dialog." + faction + "." + str(i)
                       for faction in ("blackwater", "qingchuan") for i in range(4)),
    "roles": tuple("role." + faction + "." + str(i)
                   for faction in ("blackwater", "qingchuan") for i in range(4)),
    "garrisons": ("garrison.blackwater", "garrison.qingchuan"),
    "organizations": ("org.blackwater", "org.qingchuan"),
    "wallets": ("wallet.blackwater.camp", "wallet.qingchuan.camp"),
    "inventories": ("inventory.blackwater.camp", "inventory.qingchuan.camp"),
    "items": ("item.bow", "item.grain", "item.spear", "item.timber"),
    "offices": ("office.blackwater.army", "office.blackwater.garrison", "office.qingchuan.army"),
    "military_orders": ("order.blackwater.army", "order.blackwater.defense", "order.qingchuan.army"),
    "strongholds": ("stronghold.blackwater", "stronghold.qingchuan"),
    "siege_topologies": ("topology.blackwater", "topology.qingchuan"),
    "battlefields": ("field.camp", "field.valley"),
    "military_objectives": ("military_goal.camp", "military_goal.valley"),
    "troop_types": ("troop.archer", "troop.cavalry", "troop.infantry"),
}


def add_field_battle_military_data(data: dict, original_alpha: dict,
                                   appearance_definition: dict) -> None:
    """待共享审定后由唯一生成器调用；真兵营初态不预写成军、接触或会话。"""
    scenario = data["scenarios"]["society::scenario.qingchuan_spring_reserve"]
    enabled, initial = scenario["enabled"], scenario["initial"]
    names: set[str] = set()

    def collect_texts(value: object) -> None:
        """对白行和显示名也沿原文本引用，不复制另一套翻译。"""
        if isinstance(value, str) and value in original_alpha["texts"]:
            names.add(value)
        elif isinstance(value, list):
            for child in value:
                collect_texts(child)
        elif isinstance(value, dict):
            for child in value.values():
                collect_texts(child)

    for table, ids in FIELD_BATTLE_MILITARY_IDS.items():
        source = {row["id"]: row for row in original_alpha[table]}
        for identifier in ids:
            if identifier in data[table] or identifier in enabled.get(table, []):
                raise ValueError("Field military import would replace " + identifier)
            row = deepcopy(source[identifier])
            collect_texts(row)
            data[table][identifier] = row
            enabled.setdefault(table, []).append(identifier)
            if table == "actors":
                # alpha 旧剧本的原 _legacy 默认 combat={}；外观是明确测试输入，不冒原 alpha 字段。
                initial["actors"].append({
                    "actor_id": identifier, "place_id": row["initial_place"],
                    "alive": True, "available": True, "age_years": row["initial_age_years"],
                    "attributes": deepcopy(row["attributes"]), "skills": deepcopy(row["skills"]),
                    "combat": deepcopy(row.get("combat", {})),
                    "appearance_definition": deepcopy(appearance_definition),
                    "initial_goods": deepcopy(row.get("initial_goods", [])),
                })
            elif table in ("wallets", "inventories"):
                keys = ("id", "owner", "initial_balance") if table == "wallets" else ("id", "owner", "capacity", "goods")
                initial.setdefault(table, []).append({key: deepcopy(row[key]) for key in keys})
            elif table == "garrisons":
                initial.setdefault(table, []).append({key: deepcopy(row[key]) for key in ("id", "organization_id", "units")})
            elif table in ("offices", "strongholds"):
                initial.setdefault(table, []).append({"id": identifier})
            elif table == "roles":
                initial["roles"].append({"role_id": identifier, "actor_id": row["initial_actor"], "enabled": True})
    for key in sorted(names):
        value = original_alpha["texts"][key]
        if key in data["texts"] and data["texts"][key] != value:
            raise ValueError("Field military text conflict " + key)
        data["texts"][key] = value
    # 只为青川补对称 NPC 意图；仍须原任职、接令、成军和出发，不给已完成军团。
    commander = data["actors"]["actor.qingchuan.commander"]
    if "military_plan" in commander:
        raise ValueError("Would replace original qingchuan military plan")
    commander["military_plan"] = {
        "office_id": "office.qingchuan.army", "order_id": "order.qingchuan.army",
        "unit_ids": ["unit.qingchuan.infantry", "unit.qingchuan.archer"],
        "food_quantity": 800, "money": 300,
    }
    # 新军官沿原军政调度；只排除生计还会落入 all_actors 观察者，从而被
    # director_routes_actor 接管并跳过原军事提案。两处仅排除本次新增八人。
    admissions = scenario["dynamic_world"]["director"]["admissions"]
    for admission in ("adult_life", "remaining_observers"):
        excluded = admissions[admission].setdefault("excluded_actor_ids", [])
        for identifier in FIELD_BATTLE_MILITARY_IDS["actors"]:
            if identifier in excluded:
                raise ValueError("Duplicate military Director exclusion " + identifier)
            excluded.append(identifier)


def add_field_battle_playable_data(data: dict) -> None:
    """待 root 审定接入；只加原统帅的公开选人入口，不换默认人物或运行身份。"""
    scenario = data["scenarios"]["society::scenario.qingchuan_spring_reserve"]
    identifier = "actor.qingchuan.commander"
    if scenario["player_actor"] != "society::actor.apprentice":
        raise ValueError("Field playable addition cannot replace the original default")
    if identifier in scenario["playable_actor_ids"]:
        raise ValueError("Field commander is already selectable")
    if identifier not in scenario["enabled"]["actors"]:
        raise ValueError("Original field commander must be enabled first")
    rows = [row for row in scenario["initial"]["actors"]
            if row["actor_id"] == identifier]
    if len(rows) != 1:
        raise ValueError("Exactly one original field commander initial row is required")
    appearance = rows[0]["appearance_definition"]
    if appearance.get("kind") != "fixed" or appearance.get("generator_version") != "appearance-v1":
        raise ValueError("Selectable original commander must preserve a real fixed appearance")
    actor = data["actors"][identifier]
    if "appearance_definition" in actor:
        if actor["appearance_definition"] != appearance:
            raise ValueError("Field playable addition cannot replace author appearance")
    else:
        # 与已审军官初态的明确测试外观同值，不能仅为准入放一个空外观。
        actor["appearance_definition"] = deepcopy(appearance)
    dynamic = scenario["dynamic_world"]
    if dynamic["founder_policy"] not in ("selected_existing_actor_v1",
                                          "selected_existing_actor_roster_v1"):
        raise ValueError("Original existing-actor selection policy is required")
    # 原 v1 限恰好两人，增加第三人必须选原已实现的有界名单政策，不能绕过校验。
    dynamic["founder_policy"] = "selected_existing_actor_roster_v1"
    scenario["playable_actor_ids"].append(identifier)
    scenario["playable_actor_appearance_policies"].append({
        "actor_id": identifier, "policy": "preserve_existing_v1",
    })
