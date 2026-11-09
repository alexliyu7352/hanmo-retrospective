// 原 Military 根的唯一成员/行型/回调绑定。运行 codec 与静态布局检查
// 共用下列遍历；检查器不构造世界、不读取当前值，也不补未实现的根。
#pragma once
#include "military_exact_sources.hpp"
#include "military_tactical_ownership_codec_fields.hpp"
#include "dynamic_action_codec_fields.hpp"
#include "dynamic_session_codec_fields.hpp"
#include "military_local_boundary_codec_fields.hpp"
#include "military_spatial_codec_fields.hpp"
#include "dynamic_effect_codec_fields.hpp"
#include "military_exact_source_codec_fields.hpp"
#include "military_supply_exact_codec_fields.hpp"
#include "dynamic_objective_codec_fields.hpp"
#include "duty/military_duty_codec_fields.hpp"
#include <type_traits>
#include <utility>

namespace hanmo::resident::military_exact_root_bindings {
// 职责：编译期验证绑定的真实成员确实持有声明行型，不能只靠根名
// 给不相关表贴类型标签；Getter 在布局检查时只用于 unevaluated 类型推导。
template <class Row, class Getter> constexpr void require_table_member() {
  using Actual = std::remove_reference_t<decltype(std::declval<Getter>()(
      std::declval<MilitaryExactState &>()))>;
  static_assert(std::is_same_v<Actual, OrderedTable<Row>>,
                "military root must bind its actual owned row type");
}

// 职责：按现有明确 /2、/3、/4 分支访问唯一根回调；不把 /3 成员
// 偷写进 /2，不改变任何行的原字段、标签或错误优先级。
template <class BindingArchive>
Status visit(BindingArchive &c, Integer selected) {
  if (selected != 2 && selected != 3 && selected != 4)
    return c.reject("MilitaryExactSelectedSchemaRequired");
  c.schema(selected);
  if (!c.status().ok)
    return c.status();
  const Integer original_selected = selected == 4 ? 3 : selected;
  if (original_selected == 3) {
    c.template table<MilitaryTacticalFormation3>(
        "formations",
        [](MilitaryExactState &s) -> auto & {
          return s.formations;
        },
        [](auto &x, auto &row) {
          return military_tactical_ownership_fields::value(x, row);
        },
        0);
    c.template table<MilitaryTacticalMissionRecord3>(
        "missions",
        [](MilitaryExactState &s) -> auto & {
          return s.missions;
        },
        [](auto &x, auto &row) {
          return military_tactical_ownership_fields::value(x, row);
        },
        0);
  }
  c.template table<DynamicMilitaryAction2>(
      "actions",
      [](MilitaryExactState &s) -> auto & {
        return s.actions;
      },
      [](auto &x, auto &row) {
        return dynamic_action_codec::value(x, row);
      },
      0);
  c.template table<DynamicMilitarySessionCurrent2>(
      "session_current_states",
      [](MilitaryExactState &s) -> auto & {
        return s.session_current_states;
      },
      [](auto &x, auto &row) {
        return dynamic_session_codec::value(x, row);
      },
      0);
  c.template table<DynamicSessionStateSource2>(
      "session_state_sources",
      [](MilitaryExactState &s) -> auto & {
        return s.session_state_sources;
      },
      [](auto &x, auto &row) {
        return dynamic_session_codec::value(x, row);
      },
      0);
  c.template table<LocalHexMotionBasis2>(
      "local_motion_bases",
      [](MilitaryExactState &s) -> auto & {
        return s.local_motion.bases;
      },
      [](auto &x, auto &row) {
        return military_local_motion_fields::value(x, row);
      },
      0);
  c.template table<LocalMotionCursor2>(
      "local_motion_cursors",
      [](MilitaryExactState &s) -> auto & {
        return s.local_motion.cursors;
      },
      [](auto &x, auto &row) {
        return military_local_motion_fields::value(x, row);
      },
      0);
  c.template table<LocalMotionBoundarySource>(
      "local_motion_prefix_closes",
      [](MilitaryExactState &s) -> auto & {
        return s.local_motion.prefix_closes;
      },
      [original_selected](auto &x, auto &row) {
        return military_local_boundary_fields::selected(x, row,
                                                        original_selected);
      },
      original_selected);
  c.template table<MilitarySpatialChart2>(
      "spatial_charts",
      [](MilitaryExactState &s) -> auto & {
        return s.spatial_handoffs.charts;
      },
      [](auto &x, auto &row) {
        return military_spatial_fields::value(x, row);
      },
      0);
  c.template table<MilitarySpatialHandoff2>(
      "spatial_handoffs",
      [](MilitaryExactState &s) -> auto & {
        return s.spatial_handoffs.handoffs;
      },
      [](auto &x, auto &row) {
        return military_spatial_fields::value(x, row);
      },
      0);
  c.template table<DynamicBattleEffectGroup2>(
      "effect_groups",
      [](MilitaryExactState &s) -> auto & {
        return s.effect_groups;
      },
      [](auto &x, auto &row) {
        return dynamic_effect_codec::value(x, row);
      },
      0);
  c.template table<DynamicMilitaryBattleFact2>(
      "dynamic_battle_facts",
      [](MilitaryExactState &s) -> auto & {
        return s.dynamic_battle_facts;
      },
      [](auto &x, auto &row) {
        return military_exact_source_fields::value(x, row);
      },
      0);
  c.template table<DynamicMilitaryCasualtyFact2>(
      "dynamic_casualty_facts",
      [](MilitaryExactState &s) -> auto & {
        return s.dynamic_casualty_facts;
      },
      [](auto &x, auto &row) {
        return military_exact_source_fields::value(x, row);
      },
      0);
  c.template table<MilitarySupplyEffectSource2>(
      "supply_effect_sources",
      [](MilitaryExactState &s) -> auto & {
        return s.supply_effect_sources;
      },
      [](auto &x, auto &row) {
        return military_supply_exact_fields::value(x, row);
      },
      0);
  c.template object<MilitaryDutyState>(
      "duty_sources",
      [](MilitaryExactState &s) -> auto & {
        return s.duty_sources;
      },
      [selected](auto &x, auto &owner) {
        return military_duty_codec::state(x, owner, selected);
      },
      selected);
  c.template table<DynamicMilitaryObjectiveEvidence2>(
      "objective_evidence",
      [](MilitaryExactState &s) -> auto & {
        return s.objective_evidence;
      },
      [](auto &x, auto &row) {
        return dynamic_objective_codec::value(x, row);
      },
      0);
  return c.status();
}

// 职责：将上述唯一绑定转给原实际 archive。只借本次候选，不另建
// 持值、资源预算或 parser；错误仍由原字段 callback 原样返回。
template <class Archive> struct StateArchive {
  Archive &original;
  MilitaryExactState &state;
  // 职责：转回原 archive 的首错状态，不另造成功。
  Status status() const {
    return original.status();
  }
  // 职责：沿原 archive 记录同一字段错误。
  Status reject(std::string_view code) {
    return original.reject(code);
  }
  // 职责：保持原 schema 首字段及选版不符的原失败。
  Status schema(Integer selected) {
    Integer schema = selected;
    original.field("schema", schema);
    if (!original.status().ok)
      return original.status();
    return schema == selected
               ? original.status()
               : original.reject("MilitaryExactSelectedSchemaMismatch");
  }
  // 职责：只转发实际已核行型的表成员与原 callback。
  template <class Row, class Getter, class Fields>
  Status table(std::string_view name, Getter getter, Fields fields, Integer) {
    require_table_member<Row, Getter>();
    return original.table(name, getter(state), fields);
  }
  // 职责：只转发真实嵌入 owner，保持原对象边界。
  template <class Value, class Getter, class Fields>
  Status object(std::string_view name, Getter getter, Fields fields, Integer) {
    using Actual = std::remove_reference_t<decltype(getter(
        std::declval<MilitaryExactState &>()))>;
    static_assert(std::is_same_v<Actual, Value>);
    return original.object(name, getter(state), fields);
  }
};
}
