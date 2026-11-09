// 具名看管所有实字段的共同冷访客；来源校验仍由领域 validator 完整负责。
#pragma once
#include "named_military_custody_history_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/military/named_military_custody_history_model.hpp"
#include "resident/domains/travel/movement_record_codec_fields.hpp"
namespace hanmo::resident::named_custody_fields {
template<class C>Status value(C&, NamedMilitaryCustodyPolicy&, const MovementRecordCodecSelection&);
template<class C>Status value(C&, NamedMilitaryCustodyTerm&, const MovementRecordCodecSelection&);
template<class C>Status value(C&, NamedMilitaryCustodyRelease&, const MovementRecordCodecSelection&);
template<class C>Status value(C&, NamedMilitaryCustodyHistory&, const MovementRecordCodecSelection&);
template<class C>Status value(C& c, StableId& v, const MovementRecordCodecSelection&) { return c.field("value", v); }
template<class C, class T>Status field(C& c, const char* name, T& v, const MovementRecordCodecSelection& selection) {
  if constexpr(std::is_same_v<T, StableId> || std::is_same_v<T, Integer> || std::is_same_v<T, bool>) return c.field(name, v);
  else return c.object(name, [&](auto& child) { return movement_record_fields::value(child, v, selection); });
}
template<class C, class T, class A>Status field(C& c, const char* name, std::vector<T,A>& v, const MovementRecordCodecSelection& selection) {
  return c.sequence(name, v, [&](auto& child, auto& row) {
    if constexpr(std::is_same_v<T, StableId> || std::is_same_v<T, NamedMilitaryCustodyTerm> || std::is_same_v<T, NamedMilitaryCustodyRelease>) return value(child, row, selection);
    else return movement_record_fields::value(child, row, selection);
  });
}
template<class C>Status value(C& c, NamedMilitaryCustodyPolicy& v, const MovementRecordCodecSelection& selection) {
  field(c, "rule_id", v.rule_id, selection);
  field(c, "duration_half_days", v.duration_half_days, selection);
  return c.status();
}
template<class C>Status value(C& c, NamedMilitaryCustodyTerm& v, const MovementRecordCodecSelection& selection) {
  field(c, "id", v.id, selection);
  field(c, "actor_id", v.actor_id, selection);
  field(c, "captured_army_id", v.captured_army_id, selection);
  field(c, "captured_organization_id", v.captured_organization_id, selection);
  field(c, "capture_source", v.capture_source, selection);
  field(c, "capture_application_source", v.capture_application_source, selection);
  field(c, "capture_actor_ordinal", v.capture_actor_ordinal, selection);
  field(c, "capture_application_schema", v.capture_application_schema, selection);
  field(c, "army_formation_source", v.army_formation_source, selection);
  field(c, "army_formation_schema", v.army_formation_schema, selection);
  field(c, "policy_source", v.policy_source, selection);
  field(c, "capture_tick", v.capture_tick, selection);
  field(c, "recorded_tick", v.recorded_tick, selection);
  field(c, "duration_half_days", v.duration_half_days, selection);
  field(c, "due_tick", v.due_tick, selection);
  field(c, "adopted_at", v.adopted_at, selection);
  field(c, "due_at", v.due_at, selection);
  field(c, "field_order", v.field_order, selection);
  return c.status();
}
template<class C>Status value(C& c, NamedMilitaryCustodyRelease& v, const MovementRecordCodecSelection& selection) {
  field(c, "id", v.id, selection);
  field(c, "term_id", v.term_id, selection);
  field(c, "actor_id", v.actor_id, selection);
  field(c, "keeper_army_id", v.keeper_army_id, selection);
  field(c, "keeper_organization_id", v.keeper_organization_id, selection);
  field(c, "prior_custody_source", v.prior_custody_source, selection);
  field(c, "released_at", v.released_at, selection);
  field(c, "recorded_tick", v.recorded_tick, selection);
  field(c, "origin_anchor", v.origin_anchor, selection);
  field(c, "origin_sources", v.origin_sources, selection);
  field(c, "destination_priority", v.destination_priority, selection);
  field(c, "destination_settlement_id", v.destination_settlement_id, selection);
  field(c, "jurisdiction_id", v.jurisdiction_id, selection);
  field(c, "road_distance", v.road_distance, selection);
  field(c, "road_ids", v.road_ids, selection);
  field(c, "initial_returning", v.initial_returning, selection);
  field(c, "released_anchor", v.released_anchor, selection);
  field(c, "field_order", v.field_order, selection);
  return c.status();
}
template<class C>Status value(C& c, NamedMilitaryCustodyHistory& v, const MovementRecordCodecSelection& selection) {
  field(c, "terms", v.terms, selection);
  field(c, "closures", v.closures, selection);
  field(c, "field_order", v.field_order, selection);
  return c.status();
}
} // namespace hanmo::resident::named_custody_fields
