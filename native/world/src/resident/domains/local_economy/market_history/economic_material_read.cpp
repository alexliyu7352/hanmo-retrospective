// 已批准首批市场关账：本文件具名 plan 仅停内存计量，原数量、来源、复制及租约流程不变。
#include "economic_material_read.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/native_item_craft_state.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> v) {
  if (!v.ok)
    throw Failure{std::move(v.error)};
  return std::move(v.value);
}
template <class T, class F> Outcome<T> guard(F &&f) {
  try {
    return {true, f(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
EconomicMaterialNeedRead need(const EconomicMaterialSources &x,
                              const EconomicMaterialNeedKey &key) {
  if (key.kind == EconomicMaterialKind::production_input) {
    const auto *u = x.economy.production_units.find(key.original_business_key);
    const auto *r = u ? x.economy.recipes.find(u->recipe_id) : nullptr;
    if (!u || !r || u->id != key.original_business_key ||
        r->id != u->recipe_id || key.need_slot_ordinal >= r->inputs.size())
      fail("EconomicMaterialOriginalRecipeRequired", key.original_business_key);
    const auto &q = r->inputs[key.need_slot_ordinal];
    if (!x.shared.items.find(q.item_id) || q.numerator < 0 ||
        q.denominator <= 0)
      fail("EconomicMaterialOriginalInputRequired", r->id);
    return {&q.item_id, &r->id, &x.identity.content_digest, 1,
            key.need_slot_ordinal};
  }
  if (key.kind != EconomicMaterialKind::ration || key.need_slot_ordinal != 0)
    fail("EconomicMaterialNeedKindMismatch", key.original_business_key);
  const auto *o = x.economy.obligations.find(key.original_business_key);
  const auto *r = o ? x.economy.rates.find(o->rate_profile) : nullptr;
  if (!o || o->id != key.original_business_key ||
      o->kind != ObligationKind::ration || !r || r->id != o->rate_profile ||
      r->numerator < 0 || r->denominator <= 0 ||
      !x.shared.items.find(o->item_id))
    fail("EconomicMaterialOriginalRationRequired", key.original_business_key);
  return {&o->item_id, &o->id, &x.identity.content_digest, 1, 0};
}
void original_owner(const EconomicMaterialSources &x,
                    const EconomicMaterialNeedKey &key,
                    const InventoryView &inventory, const Owner &owner) {
  if (key.kind == EconomicMaterialKind::production_input) {
    const auto &u = *x.economy.production_units.find(key.original_business_key);
    if (std::find(u.input_inventory_ids.begin(), u.input_inventory_ids.end(),
                  inventory.id) == u.input_inventory_ids.end() ||
        !owner_equal(owner, u.owner))
      fail("EconomicMaterialOriginalProductionOwnerRequired", inventory.id);
  } else {
    const auto &o = *x.economy.obligations.find(key.original_business_key);
    if (inventory.id != o.source_wallet_or_inventory ||
        !owner_equal(inventory.owner, owner))
      fail("EconomicMaterialOriginalRationInventoryRequired", inventory.id);
    if (o.principal.kind == SubjectKind::organization) {
      if (owner.kind != Owner::Kind::organization ||
          owner.id != o.principal.id || owner.extra)
        fail("EconomicMaterialOriginalRationOwnerRequired", inventory.id);
    } else if (o.principal.kind == SubjectKind::cohort) {
      const auto *c = x.economy.cohorts.find(o.principal.id);
      const auto *h =
          c ? x.economy.households.find(c->household_group_id) : nullptr;
      if (!h || h->inventory_id != inventory.id ||
          owner.kind != Owner::Kind::household || owner.id != h->id ||
          owner.extra)
        fail("EconomicMaterialOriginalRationOwnerRequired", inventory.id);
    } else
      fail("EconomicMaterialOriginalRationOwnerRequired", inventory.id);
  }
}
void validate_material_boundary(const EconomicMaterialSources &x,
                                const EconomicMaterialNeedKey &key,
                                const InventoryView &inventory, Integer tick,
                                const SourceRef *before) {
  const auto now = take(x.clock.read_current_moment());
  take(encode_world_moment(now));
  if (tick < 0 || tick > now.half_day_tick)
    fail("EconomicMaterialActualTickRequired", inventory.id);
  if (!before) {
    if (x.common.inventory(inventory.id) != &inventory ||
        tick != now.half_day_tick)
      fail("EconomicMaterialActualInventoryBorrowRequired", inventory.id);
    return;
  }
  if (before->domain != "local_economy" ||
      before->kind != (key.kind == EconomicMaterialKind::production_input
                           ? "local_production"
                           : "local_ration"))
    fail("EconomicMaterialHistoricalBusinessRequired", before->id);
  take(read_actual_common_fact_position(x.common, *before));
}
// 所有候选使用同一 S9 纯核；过期是可复算的不合格，坏来源不是可以跳过的批。
bool eligible(const EconomicMaterialSources &x,
              const EconomicMaterialNeedRead &rule, const Owner &owner,
              const LotView &lot, Integer tick, const SourceRef *before) {
  if (lot.item_id != *rule.item_id || !owner_equal(lot.owner, owner) ||
      !lot.reserved_by.empty())
    return false;
  if (lot.id.empty() || lot.quantity <= 0 || lot.extra || lot.owner.extra ||
      (lot.item_instance_id && lot.lot_properties_ref))
    fail("EconomicMaterialInvalidCandidateLot", lot.id);
  if (lot.item_instance_id) {
    if (!x.instances || !x.item_catalog || !x.outputs || lot.quantity != 1)
      fail("EconomicMaterialOriginalInstanceRequired", lot.id);
    const auto *adoption = take(read_item_instance_original_output(
        x.common, *x.instances, *x.item_catalog, *x.outputs,
        *lot.item_instance_id, &x.clock));
    const auto original =
        take(x.outputs->read_item_output(adoption->key, adoption->source_kind));
    if (original.produced_tick > tick ||
        (before &&
         take(read_actual_common_fact_position(x.common,
                                               adoption->key.source_ref)) >=
             take(read_actual_common_fact_position(x.common, *before))))
      fail("EconomicMaterialPropertyAfterBusiness", lot.id);
    // 原普通生产和口粮没有独特件终结权，不把它拆成可消耗普通数量。
    return false;
  }
  if (!lot.lot_properties_ref)
    return true;
  if (!x.instances || !x.properties || !x.item_catalog || !x.outputs)
    fail("EconomicMaterialOriginalPropertiesRequired", lot.id);
  const auto *adoption = take(read_lot_properties_original_output(
      x.common, *x.properties, *x.item_catalog, *x.outputs,
      *lot.lot_properties_ref, &x.clock));
  const auto &property =
      *x.properties->properties.find(lot.lot_properties_ref->properties_id);
  if (adoption->definition_id != lot.item_id || property.produced_tick > tick)
    fail("EconomicMaterialPropertyAfterUse", lot.id);
  if (before && take(read_actual_common_fact_position(
                    x.common, adoption->key.source_ref)) >=
                    take(read_actual_common_fact_position(x.common, *before)))
    fail("EconomicMaterialPropertyAfterBusiness", lot.id);


  auto fields = CheckpointMemoryCharge(0);
  ItemMaterialUse use;
  use.definition_id = checkpoint_fresh_string_copy(*rule.item_id);
  const auto allowed = check_item_material_use(*x.instances, *x.properties,
                                               *x.item_catalog, lot, use, tick);
  if (!allowed.ok) {
    if (allowed.error.original.code == "MaterialExpired")
      return false;
    throw Failure{allowed.error};
  }
  return true;
}
} // namespace
Status NativeEconomicMaterialRead::validate_bindings() const {
  return guard<Done>([&] {
    const auto v = take(selected_native_item_craft_versions(x_.identity));
    if (!x_.economy.enabled ||
        x_.common.state().metadata.content_id != x_.identity.content_id ||
        x_.common.state().metadata.content_version !=
            x_.identity.content_version ||
        x_.common.state().metadata.content_digest !=
            x_.identity.content_digest ||
        x_.common.state().metadata.ruleset_id != x_.identity.ruleset_id ||
        !v.selected_resource_operations ||
        !x_.shared.selected_resource_operations_enabled)
      fail("EconomicMaterialLockedSelectionRequired");
    if (bool(v.item_instances) != bool(x_.instances) ||
        bool(v.lot_properties) != bool(x_.properties) ||
        bool(v.item_instances) != bool(x_.item_catalog) ||
        bool(v.item_instances) != bool(x_.outputs) ||
        x_.shared.item_instances_enabled != bool(v.item_instances) ||
        x_.shared.lot_properties_enabled != bool(v.lot_properties))
      fail("EconomicMaterialItemSelectionMismatch");
    if (!v.item_instances)
      for (const auto &row : x_.common.state().inventories.rows())
        for (const auto &lot : row.second.lots)
          if (lot.item_instance_id || lot.lot_properties_ref)
            fail("EconomicMaterialUnselectedTypedLot", lot.id);
    return Done{};
  });
}
bool NativeEconomicMaterialRead::bound_to(
    const CommonStore &c, const CommonCatalog &s,
    const LocalEconomyCatalog &e) const noexcept {
  return &c == &x_.common && &s == &x_.shared && &e == &x_.economy;
}
Outcome<EconomicMaterialNeedRead> NativeEconomicMaterialRead::read_need(
    const EconomicMaterialNeedKey &key) const {
  return guard<EconomicMaterialNeedRead>([&] {
    take(validate_bindings());
    return need(x_, key);
  });
}
Outcome<Integer> NativeEconomicMaterialRead::count(
    const EconomicMaterialNeedKey &key, const InventoryView &inventory,
    const Owner &owner, Integer tick, const SourceRef *before) const {
  return guard<Integer>([&] {
    const auto rule = take(read_need(key));
    original_owner(x_, key, inventory, owner);
    validate_material_boundary(x_, key, inventory, tick, before);
    Integer total = 0;
    for (const auto &lot : inventory.lots)
      if (eligible(x_, rule, owner, lot, tick, before))
        if (__builtin_add_overflow(total, lot.quantity, &total))
          fail("EconomyQuantityOverflow", inventory.id);
    return total;
  });
}
Outcome<EconomicMaterialFifoRead> NativeEconomicMaterialRead::select(
    const EconomicMaterialNeedKey &key, const InventoryView &inventory,
    const Owner &owner, Integer quantity, Integer tick,
    const SourceRef *before) const {
  return guard<EconomicMaterialFifoRead>([&] {
    const auto rule = take(read_need(key));
    original_owner(x_, key, inventory, owner);
    validate_material_boundary(x_, key, inventory, tick, before);
    if (quantity < 0)
      fail("InvalidEconomicMaterialQuantity", inventory.id);
    EconomicMaterialFifoRead out;
    Integer left = quantity;
    for (const auto &lot : inventory.lots) {
      if (!left)
        break;
      if (!eligible(x_, rule, owner, lot, tick, before))
        continue;
      const auto amount = std::min(left, lot.quantity);
      if (lot.lot_properties_ref && amount < lot.quantity) {
        const auto &p = *x_.properties->properties.find(
            lot.lot_properties_ref->properties_id);
        const auto *policy =
            x_.item_catalog->property_policies.find(p.property_policy_id);
        if (!policy || !policy->splittable)
          fail("LotPropertySplitForbidden", lot.id);
      }
      out.selections.push_back({&lot, amount});
      left -= amount;
    }
    if (left)
      fail("EconomicMaterialInsufficientGoods", inventory.id);
    out.quantity = quantity;
    return out;
  });
}
Status NativeEconomicMaterialRead::visit_candidate_pins(
    const EconomicMaterialNeedKey &key, const InventoryView &inventory,
    const Owner &owner, Integer tick, LotPropertiesSourcePinSink &sink,
    const SourceRef *before) const {
  return guard<Done>([&] {
    const auto rule = take(read_need(key));
    original_owner(x_, key, inventory, owner);
    validate_material_boundary(x_, key, inventory, tick, before);
    for (const auto &lot : inventory.lots) {
      eligible(x_, rule, owner, lot, tick, before);
      if (lot.item_id != *rule.item_id || !owner_equal(lot.owner, owner) ||
          !lot.reserved_by.empty())
        continue;
      if (lot.item_instance_id)
        take(visit_item_instance_original_output_pins(
            x_.common, *x_.instances, *x_.item_catalog, *x_.outputs,
            *lot.item_instance_id, sink, &x_.clock));
      if (lot.lot_properties_ref)
        take(visit_lot_properties_original_pins(
            x_.common, *x_.properties, *x_.item_catalog, *x_.outputs,
            *lot.lot_properties_ref, sink, &x_.clock));
    }
    return Done{};
  });
}
Outcome<EconomicMaterialConsumptionRecipe>
prepare_economic_material_consumption(
    const NativeEconomicMaterialRead &materials,
    std::span<const EconomicMaterialConsumption> requests, Integer tick) {
  return guard<EconomicMaterialConsumptionRecipe>([&] {
    CheckpointVector<EconomicMaterialFifoRead> actual;
    actual.reserve(requests.size());


    for (std::size_t index = 0; index < requests.size(); ++index) {
      const auto &r = requests[index];
      if (r.quantity <= 0)
        fail("InvalidEconomicMaterialQuantity", r.inventory.id);
      if (r.need.kind != requests.front().need.kind ||
          r.need.original_business_key !=
              requests.front().need.original_business_key ||
          (r.need.kind == EconomicMaterialKind::ration && requests.size() != 1))
        fail("EconomicMaterialOriginalBusinessMismatch", r.inventory.id);
      for (std::size_t prior = 0; prior < index; ++prior)
        if (requests[prior].inventory.id == r.inventory.id &&
            requests[prior].need.need_slot_ordinal == r.need.need_slot_ordinal)
          fail("EconomicMaterialDuplicateInput", r.inventory.id);
      actual.push_back(take(
          materials.select(r.need, r.inventory, r.owner, r.quantity, tick)));


      for (const auto &row : actual.back().selections) {


      }
    }
    EconomicMaterialConsumptionRecipe out(CheckpointCopyLease::reserve());
    out.recipe_.operations.reserve(requests.size());
    for (std::size_t i = 0; i < requests.size(); ++i) {
      const auto &r = requests[i];
      ConsumeSelectedLots op;
      op.inventory_id = checkpoint_fresh_string_copy(r.inventory.id);
      op.owners.reserve(1);
      op.owners.push_back(r.owner);
      op.selections.reserve(actual[i].selections.size());
      for (const auto &row : actual[i].selections)
        op.selections.push_back({r.inventory.id, row.lot->id, row.quantity});
      out.recipe_.operations.emplace_back(std::move(op));
    }
    return out;
  });
}
} // namespace hanmo::resident
