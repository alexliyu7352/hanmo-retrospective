// 首次 facility_construction/1 的唯一字段表。旧设施根与旧 Estate 字节不变。
#pragma once
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "facility_construction_model.hpp"
#include "facility_construction_resource_fields.hpp"
#include "resident/domains/actor_foundation/condition_head_codec_fields.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/kernel/fixed_service_work_period_codec_fields.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/storage/common_projection.hpp"

namespace hanmo::resident::facility_construction_fields {
using namespace exact_fields;
// JSON 与 binary 都保此新选型的字段序；不在恢复时补缺字段或猜来源。
template <class C> Status begin(C &codec) {
  if constexpr (std::is_same_v<std::decay_t<C>, ActorFoundationJsonVisitor>)
    return codec.ordered_fields();
  return codec.status();
}
template <class C> Status rule(C &c, EconomicRuleRef &v) {
  begin(c);
  required_ref(c, "id", v.id);
  schema(c, "version", v.version);
  return required_ref(c, "catalog_digest", v.catalog_digest);
}
template <class C> Status original(C &c, MomentOwnerSource &v) {
  begin(c);
  c.object("source_ref", [&](auto &out) { return source(out, v.source_ref); });
  return schema(c, "source_schema", v.source_schema);
}
// Owner 仍走 Common 原 JSON/binary 实现，不能另订枚举或丢掉旧字段。
template <class C> Status owner(C &c, Owner &v) {
  if constexpr (std::is_same_v<std::decay_t<C>, CodecVisitor>)
    return common_checkpoint::owner(c, v);
  else
    return c.foreign_record(
        v,
        [](const BoundaryValue &value,
           ScopedCheckpointJsonDecode &) -> Outcome<Owner> {
          try {
            return {
                true, boundary::decode_owner(value, "construction_owner"), {}};
          } catch (const boundary::DecodeFailure &failure) {
            return {false, {}, failure.error};
          }
        },
        [](ProjectionWriter &out, const Owner &value) {
          return emit_common_owner_fields(out, value);
        });
}
template <class C>
Status requirement(C &c, ConstructionMaterialRequirement &v) {
  begin(c);
  required_ref(c, "item_id", v.item_id);
  c.field("quantity", v.quantity);
  if (c.status().ok && v.quantity <= 0)
    return c.reject("ConstructionPositiveMaterialRequired", v.item_id);
  return c.status();
}
template <class C> Status terms(C &c, ConstructionTerms &v) {
  begin(c);
  c.object("terms_ref", [&](auto &out) { return rule(out, v.terms_ref); });
  required_ref(c, "asset_ref", v.asset_ref);
  required_ref(c, "place_id", v.place_id);
  c.object("previous_definition_ref",
           [&](auto &out) { return rule(out, v.previous_definition_ref); });
  c.object("adopted_definition_ref",
           [&](auto &out) { return rule(out, v.adopted_definition_ref); });
  c.field("duration_halfdays", v.duration_halfdays);
  c.sequence("materials", v.materials,
             [](auto &out, auto &row) { return requirement(out, row); });
  c.field("money_cost", v.money_cost);
  c.field("payee_wallet_id", v.payee_wallet_id);
  c.field("capacity_units", v.capacity_units);
  required_ref(c, "capacity_unit_id", v.capacity_unit_id);
  if (c.status().ok &&
      (v.duration_halfdays < 1 || v.materials.empty() || v.money_cost < 0 ||
       v.capacity_units < 0))
    return c.reject("InvalidConstructionTerms", v.terms_ref.id);
  return c.status();
}
template <class C> Status consumed(C &c, ConstructionConsumedMaterial &v) {
  begin(c);
  c.object("actual", [&](auto &out) {
    begin(out);
    required_ref(out, "inventory_id", v.actual.inventory_id);
    required_ref(out, "lot_id", v.actual.lot_id);
    return out.field("quantity", v.actual.quantity);
  });
  required_ref(c, "item_id", v.item_id);
  c.object("original_owner",
           [&](auto &out) { return owner(out, v.original_owner); });
  c.optional("properties_ref", v.properties_ref, [](auto &out, auto &ref) {
    begin(out);
    return required_ref(out, "properties_id", ref.properties_id);
  });
  if (c.status().ok && v.actual.quantity <= 0)
    return c.reject("ConstructionPositiveConsumptionRequired", v.actual.lot_id);
  return c.status();
}
template <class C>
Status cost(C &c, ConstructionStartCost &v,
            const facility_construction_resource_fields::Layout &layout) {
  begin(c);
  required_ref(c, "receipt_id", v.receipt_id);
  required_ref(c, "construction_id", v.construction_id);
  c.object("occurred_at",
           [&](auto &out) { return moment(out, v.occurred_at); });
  c.sequence("consumed", v.consumed,
             [](auto &out, auto &row) { return consumed(out, row); });
  c.field("payer_wallet_id", v.payer_wallet_id);
  c.field("payee_wallet_id", v.payee_wallet_id);
  c.field("cash_paid", v.cash_paid);
  c.object("actual_resources", [&](auto &out) {
    return facility_construction_resource_fields::effects(
        out, v.actual_resources, layout);
  });
  if (c.status().ok && (v.cash_paid < 0 || v.consumed.empty()))
    return c.reject("ConstructionActualStartCostRequired", v.receipt_id);
  return c.status();
}
template <class C> Status construction(C &c, FacilityConstruction &v) {
  begin(c);
  required_ref(c, "construction_id", v.construction_id);
  required_ref(c, "builder_actor_id", v.builder_actor_id);
  c.object("terms_ref", [&](auto &out) { return rule(out, v.terms_ref); });
  c.object("execution_binding", [&](auto &out) {
    return work_fields::actor_action_binding(out, v.execution_binding);
  });
  c.object("started_at", [&](auto &out) { return moment(out, v.started_at); });
  required_ref(c, "actual_cost_ref", v.actual_cost_ref);
  c.sequence("authority_sources", v.authority_sources,
             [](auto &out, auto &ref) { return original(out, ref); });
  c.object("accepted_condition_head", [&](auto &out) {
    return actor_condition_codec::head(out, v.accepted_condition_head);
  });
  c.object("accepted_presence_source", [&](auto &out) {
    return original(out, v.accepted_presence_source);
  });
  if (c.status().ok &&
      (v.authority_sources.empty() ||
       v.execution_binding.action_id != v.construction_id ||
       v.execution_binding.action_kind != "facility_construction" ||
       !same_source_ref(
           v.execution_binding.source_owner_ref,
           facility_construction_started_source(v.construction_id)) ||
       v.accepted_condition_head.actor_id != v.builder_actor_id))
    return c.reject("ConstructionAdoptionPayloadMismatch", v.construction_id);
  return c.status();
}
template <class C> Status period(C &c, ConstructionPeriodResult &v) {
  begin(c);
  required_ref(c, "receipt_id", v.receipt_id);
  required_ref(c, "construction_id", v.construction_id);
  c.object("actual_period", [&](auto &out) {
    return fixed_period_fields::ref(out, v.actual_period);
  });
  c.object("occurred_at",
           [&](auto &out) { return moment(out, v.occurred_at); });
  if (c.status().ok) {
    const auto *binding =
        std::get_if<ActorActionWorkBinding>(&v.actual_period.binding);
    if (!binding || binding->action_id != v.construction_id ||
        binding->action_kind != "facility_construction" ||
        !same_source_ref(
            binding->source_owner_ref,
            facility_construction_started_source(v.construction_id)) ||
        take(compare_world_moments(v.occurred_at, v.actual_period.until)))
      return c.reject("ConstructionPeriodPayloadMismatch", v.receipt_id);
  }
  return c.status();
}
template <class C> Status stage(C &c, ConstructionStageResult &v) {
  begin(c);
  required_ref(c, "result_id", v.result_id);
  required_ref(c, "construction_id", v.construction_id);
  c.field("stage_ordinal", v.stage_ordinal);
  c.object("occurred_at",
           [&](auto &out) { return moment(out, v.occurred_at); });
  references(c, "actual_period_refs", v.actual_period_refs);
  required_ref(c, "actual_cost_ref", v.actual_cost_ref);
  if (c.status().ok &&
      (v.stage_ordinal != 0 || v.actual_period_refs.empty()))
    return c.reject("ConstructionActualStageRequired", v.result_id);
  return c.status();
}
// 唯一数组保原追加顺序；恢复同一 ID 不能覆盖前行，反向来源覆盖由原 validator
// 核。
template <class C, class T, class Allocator, class Visit, class Key>
Status rows(C &c, const char *name, std::vector<T, Allocator> &values,
            Visit visit, Key key) {
  std::size_t index = 0;
  return c.sequence(name, values, [&](auto &out, auto &row) {
    visit(out, row);
    if (out.status().ok)
      for (std::size_t prior = 0; prior < index; ++prior)
        if (key(values[prior]) == key(row))
          return out.reject("DuplicateConstructionSource", key(row));
    ++index;
    return out.status();
  });
}
template <class C>
Status state(C &c, FacilityConstructionState &v,
             const facility_construction_resource_fields::Layout &layout) {
  begin(c);
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedFacilityConstructionVersion");
  rows(
      c, "constructions", v.constructions,
      [](auto &out, auto &row) { return construction(out, row); },
      [](const auto &row) -> const StableId & { return row.construction_id; });
  rows(
      c, "start_costs", v.start_costs,
      [&](auto &out, auto &row) { return cost(out, row, layout); },
      [](const auto &row) -> const StableId & { return row.receipt_id; });
  rows(
      c, "periods", v.periods,
      [](auto &out, auto &row) { return period(out, row); },
      [](const auto &row) -> const StableId & { return row.receipt_id; });
  return rows(
      c, "stages", v.stages,
      [](auto &out, auto &row) { return stage(out, row); },
      [](const auto &row) -> const StableId & { return row.result_id; });
}
template <class C> Status catalog(C &c, FacilityConstructionCatalog &v) {
  begin(c);
  c.field("schema_version", v.schema_version);
  if (c.status().ok && v.schema_version != 1)
    return c.reject("UnsupportedFacilityConstructionCatalogVersion");
  c.object("identity",
           [&](auto &out) { return exact_fields::catalog(out, v.identity); });
  return rows(
      c, "terms", v.terms, [](auto &out, auto &row) { return terms(out, row); },
      [](const auto &row) -> const StableId & { return row.terms_ref.id; });
}
} // namespace hanmo::resident::facility_construction_fields
