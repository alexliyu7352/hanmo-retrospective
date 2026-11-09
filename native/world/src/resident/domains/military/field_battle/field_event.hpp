// 事件公共壳只含固定共通字段与 opaque owning 载荷；编号不是来源证明。
#pragma once
#include "field_mission_model.hpp"
#include "field_event_payloads.hpp"
#include <compare>
namespace hanmo::resident {
// 数值由唯一显式登记 CPP 接受；旧 0..21 身份不重排。
struct FieldBattleEventKind {
  Integer value = 0;
  constexpr operator Integer() const noexcept { return value; }
  constexpr auto operator<=>(const FieldBattleEventKind &) const = default;
};
struct FieldBattleEvent {
  Integer sequence = 0;
  FieldBattleTime at;
  FieldBattleEventKind kind{};
  std::optional<StableId> formation_id, target_formation_id, source_id;
  std::optional<Integer> count, side;
  std::optional<FieldTacticKind> tactic;
  std::optional<FieldStratagemKind> stratagem;
  std::optional<bool> success;
  FieldEventPayloads payloads;
};
} // namespace hanmo::resident
