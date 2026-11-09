// 公共 decoder 与热 typed 路由使用同一名称；无任意 JSON 持久字段或命令别名。
#include "field_deployment_boundary.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
bool field_deployment_command_kind(std::string_view kind) noexcept {
  return kind == "field_battle_set_deployment" ||
         kind == "field_battle_start" || kind == "field_council_speak" ||
         kind == "field_council_adopt" || kind == "field_battle_read" ||
         kind == "field_battle_direct" || kind == "field_battle_order" ||
         kind == "field_battle_dispatch" || kind == "field_battle_rejoin" ||
         kind == "field_battle_delegate" || kind == "field_battle_advance" ||
         kind == "field_battle_tactic" || kind == "field_battle_stratagem";
}
const char *
field_deployment_command_kind(const FieldDeploymentCommand &command) noexcept {
  return std::visit(
      [](const auto &value) -> const char * {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, FieldBattleSetDeployment>)
          return "field_battle_set_deployment";
        else if constexpr (std::is_same_v<T, FieldBattleStart>)
          return "field_battle_start";
        else if constexpr (std::is_same_v<T, FieldCouncilSpeak>)
          return "field_council_speak";
        else if constexpr (std::is_same_v<T, FieldCouncilAdopt>)
          return "field_council_adopt";
        else if constexpr (std::is_same_v<T, FieldBattleRead>)
          return "field_battle_read";
        else if constexpr (std::is_same_v<T, FieldBattleDirect>)
          return "field_battle_direct";
        else if constexpr (std::is_same_v<T, FieldBattleOrder>)
          return "field_battle_order";
        else if constexpr (std::is_same_v<T, FieldBattleDispatch>)
          return "field_battle_dispatch";
        else if constexpr (std::is_same_v<T, FieldBattleRejoin>)
          return "field_battle_rejoin";
        else if constexpr (std::is_same_v<T, FieldBattleDelegate>)
          return "field_battle_delegate";
        else if constexpr (std::is_same_v<T, FieldBattleAdvance>)
          return "field_battle_advance";
        else if constexpr (std::is_same_v<T, FieldBattleTactic>)
          return "field_battle_tactic";
        else
          return "field_battle_stratagem";
      },
      command);
}
namespace {
// 连续坐标保原整数，不在冷入口转为格号或替换成格中心。
PointI point(const BoundaryValue &value, const StableId &path) {
  boundary::fields(value, {"x", "y"}, {}, path);
  return {boundary::get_int(value, "x", path),
          boundary::get_int(value, "y", path)};
}
// 五种任务仅携带合同明确目标；合法性由同 Field validator 在真实上下文验证。
FieldMission mission(const BoundaryValue &value) {
  const StableId path = "params/mission";
  const auto kind = boundary::get_string(value, "kind", path);
  if (kind == "move_to" || kind == "hold") {
    boundary::fields(value, {"kind", "point_dm", "facing_mdeg"}, {}, path);
    const auto target =
        point(boundary::field(value, "point_dm", path), path + "/point_dm");
    const auto facing = boundary::get_int(value, "facing_mdeg", path);
    return kind == "move_to" ? FieldMission{FieldMoveTo{target, facing}}
                             : FieldMission{FieldHold{target, facing}};
  }
  if (kind == "engage") {
    boundary::fields(value, {"kind", "target_formation_id"}, {}, path);
    return FieldEngage{
        boundary::get_string(value, "target_formation_id", path)};
  }
  if (kind == "support") {
    boundary::fields(value, {"kind", "ally_formation_id"}, {}, path);
    return FieldSupport{boundary::get_string(value, "ally_formation_id", path)};
  }
  if (kind == "withdraw") {
    boundary::fields(value, {"kind", "exit_side"}, {}, path);
    return FieldWithdraw{boundary::get_int(value, "exit_side", path)};
  }
  boundary::fail("InvalidFieldMission", path);
}
// 输入波次与角色使用合同具名值；非法枚举不会落入默认分支。
FieldCouncilRequestedRole role(const StableId &name) {
  if (name == "vanguard")
    return FieldCouncilRequestedRole::Vanguard;
  if (name == "flank")
    return FieldCouncilRequestedRole::Flank;
  if (name == "reserve")
    return FieldCouncilRequestedRole::Reserve;
  if (name == "guard")
    return FieldCouncilRequestedRole::Guard;
  boundary::fail("InvalidFieldCouncilRequest", "params/role");
}
std::vector<FieldCouncilAssignment> assignments(const BoundaryValue &value) {
  const auto &rows = boundary::array(value, "params/assignments");
  if (rows.size() > 32)
    boundary::fail("InvalidFieldFormationCount", "params/assignments");
  std::vector<FieldCouncilAssignment> out;
  for (const auto &row : rows) {
    boundary::fields(row, {"formation_id", "mission", "wave"}, {},
                     "params/assignments");
    FieldCouncilAssignment a;
    a.formation_id =
        boundary::get_string(row, "formation_id", "params/assignments");
    a.mission = mission(boundary::field(row, "mission", "params/assignments"));
    const auto &wave = boundary::field(row, "wave", "params/assignments");
    if (const auto *number = wave.as<Integer>()) {
      if (*number == 1)
        a.wave = FieldCouncilWave::First;
      else if (*number == 2)
        a.wave = FieldCouncilWave::Second;
      else
        boundary::fail("InvalidFieldCouncilPlan", "params/assignments/wave");
    } else if (const auto *name = wave.as<StableId>();
               name && *name == "reserve")
      a.wave = FieldCouncilWave::Reserve;
    else
      boundary::fail("InvalidFieldCouncilPlan", "params/assignments/wave");
    out.push_back(std::move(a));
  }
  return out;
}
// 主将提案只能给本合同明确计划字段，不能传入已采纳状态或预种
// MilitaryCouncilFact。
FieldCouncilPlan plan(const BoundaryValue &v) {
  boundary::fields(v, {"layout", "assignments", "stratagems"}, {},
                   "params/plan");
  const auto layout = boundary::get_string(v, "layout", "params/plan");
  FieldCouncilPlan p;
  if (layout == "hold")
    p.layout = FieldCouncilLayout::Hold;
  else if (layout == "line")
    p.layout = FieldCouncilLayout::Line;
  else if (layout == "flank")
    p.layout = FieldCouncilLayout::Flank;
  else if (layout == "ambush")
    p.layout = FieldCouncilLayout::Ambush;
  else
    boundary::fail("InvalidFieldCouncilPlan", "params/plan/layout");
  p.assignments = assignments(boundary::field(v, "assignments", "params/plan"));
  const auto &rows =
      boundary::array(boundary::field(v, "stratagems", "params/plan"),
                      "params/plan/stratagems");
  if (rows.size() > 32)
    boundary::fail("InvalidFieldCouncilPlan", "params/plan/stratagems");
  for (const auto &row : rows) {
    const auto kind =
        boundary::get_string(row, "kind", "params/plan/stratagems");
    if (kind == "ambush") {
      boundary::fields(row, {"kind", "formation_id", "area_dm"}, {},
                       "params/plan/stratagems");
      p.stratagems.push_back(
          {FieldCouncilStratagemKind::Ambush,
           boundary::get_string(row, "formation_id", "params/plan/stratagems"),
           point(boundary::field(row, "area_dm", "params/plan/stratagems"),
                 "params/plan/stratagems/area_dm")});
    } else if (kind == "feint_strength") {
      boundary::fields(row, {"kind"}, {}, "params/plan/stratagems");
      p.stratagems.push_back(
          {FieldCouncilStratagemKind::FeintStrength, {}, {}});
    } else
      boundary::fail("InvalidFieldCouncilPlan", "params/plan/stratagems/kind");
  }
  return p;
}
} // namespace
Outcome<FieldDeploymentCommand>
decode_field_deployment_command(const StableId &kind,
                                const BoundaryValue &params) {
  try {
    if (kind == "field_battle_read") {
      boundary::fields(params, {"battle_id"},
                       {"known_field_hash", "after_event_sequence"}, "params");
      FieldBattleRead v;
      v.battle_id = boundary::get_string(params, "battle_id", "params");
      if (params.find("known_field_hash"))
        v.known_field_hash =
            boundary::get_string(params, "known_field_hash", "params");
      if (params.find("after_event_sequence"))
        v.after_event_sequence =
            boundary::get_int(params, "after_event_sequence", "params");
      if (v.after_event_sequence < 0)
        boundary::fail("InvalidFieldReadCursor", "params/after_event_sequence");
      return {true, std::move(v), {}};
    }
    if (kind == "field_battle_direct" || kind == "field_battle_order" ||
        kind == "field_battle_dispatch") {
      if (kind == "field_battle_direct")
        boundary::fields(params,
                         {"battle_id", "formation_id", "mission",
                          "expected_mission_revision"},
                         {}, "params");
      else
        boundary::fields(params, {"battle_id", "formation_id", "mission"}, {},
                         "params");
      const auto battle = boundary::get_string(params, "battle_id", "params");
      const auto formation =
          boundary::get_string(params, "formation_id", "params");
      auto task = mission(boundary::field(params, "mission", "params"));
      if (kind == "field_battle_direct")
        return {true,
                FieldBattleDirect{battle, formation,
                                  boundary::get_int(params,
                                                    "expected_mission_revision",
                                                    "params"),
                                  std::move(task)},
                {}};
      if (kind == "field_battle_order")
        return {true, FieldBattleOrder{battle, formation, std::move(task)}, {}};
      return {
          true, FieldBattleDispatch{battle, formation, std::move(task)}, {}};
    }
    if (kind == "field_battle_rejoin") {
      boundary::fields(params, {"battle_id", "formation_id"}, {}, "params");
      return {true,
              FieldBattleRejoin{
                  boundary::get_string(params, "battle_id", "params"),
                  boundary::get_string(params, "formation_id", "params")},
              {}};
    }
    if (kind == "field_battle_delegate") {
      boundary::fields(params, {"battle_id"}, {}, "params");
      return {true,
              FieldBattleDelegate{
                  boundary::get_string(params, "battle_id", "params")},
              {}};
    }
    if (kind == "field_battle_advance") {
      boundary::fields(params, {"battle_id", "to"}, {}, "params");
      const auto &to = boundary::field(params, "to", "params");
      boundary::fields(to, {"tick", "t"}, {}, "params/to");
      return {true,
              FieldBattleAdvance{
                  boundary::get_string(params, "battle_id", "params"),
                  {boundary::get_int(to, "tick", "params/to"),
                   boundary::get_int(to, "t", "params/to")}},
              {}};
    }
    if (kind == "field_battle_tactic") {
      boundary::fields(params, {"battle_id", "formation_id", "tactic_id"}, {},
                       "params");
      return {true,
              FieldBattleTactic{
                  boundary::get_string(params, "battle_id", "params"),
                  boundary::get_string(params, "formation_id", "params"),
                  boundary::get_string(params, "tactic_id", "params")},
              {}};
    }
    if (kind == "field_battle_stratagem") {
      FieldBattleStratagem v;
      v.battle_id = boundary::get_string(params, "battle_id", "params");
      const auto kind = boundary::get_string(params, "kind", "params");
      if (kind == "ambush") {
        boundary::fields(params,
                         {"battle_id", "kind", "formation_id", "area_dm"},
                         {"proposal_id"}, "params");
        v.kind = FieldStratagemKind::Ambush;
        v.formation_id = boundary::get_string(params, "formation_id", "params");
        v.area_dm = point(boundary::field(params, "area_dm", "params"),
                          "params/area_dm");
      } else if (kind == "feint_strength") {
        boundary::fields(params, {"battle_id", "kind"}, {"proposal_id"},
                         "params");
        v.kind = FieldStratagemKind::FeintStrength;
      } else if (kind == "confuse") {
        boundary::fields(params, {"battle_id", "kind", "target_formation_id"},
                         {}, "params");
        v.kind = FieldStratagemKind::Confuse;
        v.target_formation_id =
            boundary::get_string(params, "target_formation_id", "params");
      } else
        boundary::fail("InvalidFieldStratagem", "params/kind");
      if (params.find("proposal_id"))
        v.proposal_id = boundary::get_string(params, "proposal_id", "params");
      return {true, std::move(v), {}};
    }
    if (kind == "field_council_adopt") {
      boundary::fields(params, {"battle_id"}, {"proposal_id", "overrides"},
                       "params");
      FieldCouncilAdopt v;
      v.battle_id = boundary::get_string(params, "battle_id", "params");
      if (params.find("proposal_id"))
        v.proposal_id = boundary::get_string(params, "proposal_id", "params");
      if (params.find("overrides"))
        v.overrides =
            assignments(boundary::field(params, "overrides", "params"));
      return {true, std::move(v), {}};
    }
    if (kind == "field_council_speak") {
      FieldCouncilSpeak v;
      v.battle_id = boundary::get_string(params, "battle_id", "params");
      const auto speech = boundary::get_string(params, "kind", "params");
      if (speech == "role") {
        boundary::fields(params, {"battle_id", "kind", "role"}, {}, "params");
        v.speech = FieldCouncilRoleRequest{
            role(boundary::get_string(params, "role", "params"))};
      } else if (speech == "target") {
        boundary::fields(params, {"battle_id", "kind", "target_formation_id"},
                         {}, "params");
        v.speech = FieldCouncilTargetRequest{
            boundary::get_string(params, "target_formation_id", "params")};
      } else if (speech == "proposal") {
        boundary::fields(params, {"battle_id", "kind", "plan"}, {}, "params");
        v.speech = plan(boundary::field(params, "plan", "params"));
      } else
        boundary::fail("InvalidFieldCouncilRequest", "params/kind");
      return {true, std::move(v), {}};
    }
    if (kind == "field_battle_start") {
      boundary::fields(params, {"battle_id"}, {}, "params");
      return {
          true,
          FieldBattleStart{boundary::get_string(params, "battle_id", "params")},
          {}};
    }
    if (kind != "field_battle_set_deployment")
      return {false,
              {},
              common_failure("InvalidFieldDeploymentCommand", kind).error};
    boundary::fields(params,
                     {"battle_id", "formation_id", "position_dm", "facing_mdeg",
                      "mission", "control"},
                     {"deputy_actor_id"}, "params");
    FieldBattleSetDeployment out;
    out.battle_id = boundary::get_string(params, "battle_id", "params");
    out.formation_id = boundary::get_string(params, "formation_id", "params");
    out.position_dm = point(boundary::field(params, "position_dm", "params"),
                            "params/position_dm");
    out.facing_mdeg = boundary::get_int(params, "facing_mdeg", "params");
    out.mission = mission(boundary::field(params, "mission", "params"));
    const auto control = boundary::get_string(params, "control", "params");
    if (control != "direct" && control != "autonomous")
      return {false,
              {},
              common_failure("InvalidFieldDeployment", "params/control").error};
    out.control = control == "direct" ? FieldDeploymentControl::Direct
                                      : FieldDeploymentControl::Autonomous;
    if (params.find("deputy_actor_id"))
      out.deputy_actor_id =
          boundary::get_string(params, "deputy_actor_id", "params");
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
