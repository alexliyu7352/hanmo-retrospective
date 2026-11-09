// 当时本人权源与实际 Common 原语一起采用；不增加第二材料账/付款账。
#include "facility_construction_cost_writer.hpp"
#include "facility_construction_cost_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include "resident/storage/money_transfer_recipe.hpp"
#include <limits>

namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
// 首次记录字段只走原 fresh-copy 构造；预留由真正 private candidate 持有。
StableId fresh(const StableId &value) {
  return checkpoint_fresh_string_copy(value);
}
const LotView &material(const CommonStore &common, const CargoSelection &part,
                        const StableId &actor) {
  return *take(
      read_facility_construction_selected_material(common, part, actor));
}
// 私有同根、完整内容锁与实际 allocator 缺一不可，不能借旧 clone 信用增长。
void candidate(const FacilityConstructionCostWriteContext &x) {
  const auto memory = current_checkpoint_memory();
  const auto owner = x.owned_fields.resource();
  if (&x.common != &x.read.common || &x.state != &x.read.state ||
      &x.assets.common != &x.common ||
      &x.read.assets != &x.assets.read.installation.production_assets ||
      &x.read.facilities != &x.assets.read.installation.catalog ||
      !same_original_catalog_identity(x.read.identity,
                                      x.assets.read.identity) ||
      !memory || !owner || !memory->shares_job(*owner))
    fail("ConstructionActualCandidateRequired");
  const auto same = [&](const auto &rows) {
    const auto allocator = rows.get_allocator();
    const auto resource = allocator.resource();
    return resource && memory->shares_job(*resource);
  };
  if (!same(x.state.constructions) || !same(x.state.start_costs) ||
      !same(x.state.periods) || !same(x.state.stages))
    fail("ConstructionActualCandidateAllocatorRequired");
}
// 精确新壳只消费原 allocator 预留；中途失败连已移动前像一并弃置。
void append_cost(CheckpointVector<ConstructionStartCost> &rows,
                 ConstructionStartCost &&row) {
  if (rows.size() == rows.max_size())
    fail("ConstructionCostCapacityExceeded");
  CheckpointVector<ConstructionStartCost> next(rows.get_allocator());
  next.reserve(rows.size() + 1);
  for (auto &old : rows)
    next.emplace_back(std::move(old));
  next.emplace_back(std::move(row));
  rows.swap(next);
}
} // namespace

Outcome<PendingFacilityConstructionCost>
apply_facility_construction_start_cost_in_candidate(
    FacilityConstructionCostWriteContext &x,
    const ProductionAssetPendingConstructionConsent &pending,
    const ProducedStableId &construction_id,
    std::span<const CargoSelection> selections) {
  try {
    candidate(x);
    const auto *consent = take(
        read_pending_production_asset_construction_consent(x.assets, pending));
    take(validate_facility_construction_catalog(
        x.read.catalog, x.read.identity, x.read.common_catalog, x.read.assets,
        x.read.facilities));
    const auto *terms = take(read_facility_construction_terms(
        x.read.catalog, consent->construction_terms_ref));
    const auto title = take(
        read_current_production_asset_title(x.assets.read, terms->asset_ref));
    if (construction_id.id().empty() || selections.empty() ||
        consent->asset_ref != terms->asset_ref ||
        consent->owner_actor_id != consent->builder_actor_id ||
        !production_asset_title_owned_by(title, Owner::Kind::actor,
                                         consent->owner_actor_id) ||
        !production_asset_title_source_equal(title, consent->title_source) ||
        !title.initial_owner)
      fail("ConstructionActualPersonalConsentRequired", construction_id.id());
    const auto usage = take(read_production_asset_initial_usage_history(
        x.assets.read, consent->usage_source, *title.initial_owner));
    const auto now = take(x.read.clock.read_current_moment());
    if (usage.ended_at || usage.facility != title.original_facility ||
        usage.facility->place_ref != terms->place_id ||
        take(compare_world_moments(consent->consented_at, now)) ||
        take(compare_world_moments(
            now, take(x.assets.read.clock.read_current_moment()))))
      fail("ConstructionActualConsentMomentRequired", construction_id.id());
    for (const auto &row : x.state.start_costs)
      if (row.construction_id == construction_id.id())
        fail("ConstructionStartCostAlreadyApplied", construction_id.id());
    if (!x.common.resource_execution_capture_identity())
      fail("ConstructionOriginalResourceCaptureRequired", construction_id.id());
    const auto selected = take(read_facility_construction_cost_selection(
        x.common, *terms, consent->builder_actor_id, selections,
        construction_id.id()));
    const auto *actor = selected.actor;
    const auto *payer = selected.payer;
    const auto inventories = selected.inventory_count;
    auto receipt_id = take(x.common.next_owned_id("construction_cost"));


    for (std::size_t i = 0; i < selections.size(); ++i) {
      const auto &part = selections[i];
      const auto &lot = material(x.common, part, actor->id);


      bool first = true;
      std::size_t size = 0;
      for (std::size_t j = 0; j < selections.size(); ++j)
        if (selections[j].inventory_id == part.inventory_id) {
          first &= j >= i;
          ++size;
        }


    }
    (x.owned_fields).grow(0);
    auto recipe_memory = CheckpointMemoryCharge(0);
    ResourceRecipe recipe;
    recipe.operations.reserve(inventories + (terms->money_cost ? 2 : 0));
    recipe.expected_versions.reserve(inventories);
    ConstructionStartCost result{
        fresh(receipt_id.id()),
        fresh(construction_id.id()),
        now,
        {},
        terms->money_cost ? fresh(payer->id) : StableId{},
        terms->money_cost ? fresh(terms->payee_wallet_id) : StableId{},
        terms->money_cost};
    result.consumed.reserve(selections.size());
    for (std::size_t i = 0; i < selections.size(); ++i) {
      const auto &part = selections[i];
      bool first = true;
      std::size_t size = 0;
      for (std::size_t j = 0; j < selections.size(); ++j)
        if (selections[j].inventory_id == part.inventory_id) {
          first &= j >= i;
          ++size;
        }
      if (!first)
        continue;


      auto key_memory = CheckpointMemoryCharge(0);
      const ResourceKey key{ResourceKey::Kind::inventory,
                            fresh(part.inventory_id)};
      recipe.expected_versions.emplace_back(x.common.read_version(key));
      ConsumeSelectedLots consume;
      consume.inventory_id = fresh(part.inventory_id);
      consume.selections.reserve(size);
      consume.owners.reserve(1);
      consume.owners.emplace_back(
          Owner{Owner::Kind::actor, fresh(actor->id), {}});
      for (const auto &selected : selections)
        if (selected.inventory_id == part.inventory_id) {
          const auto &lot = material(x.common, selected, actor->id);
          consume.selections.emplace_back(
              CargoSelection{fresh(selected.inventory_id),
                             fresh(selected.lot_id), selected.quantity});
          ConstructionConsumedMaterial actual{
              {fresh(selected.inventory_id), fresh(selected.lot_id),
               selected.quantity},
              fresh(lot.item_id),
              {lot.owner.kind, fresh(lot.owner.id), {}, {}},
              {}};
          actual.original_owner.field_order.reserve(
              lot.owner.field_order.size());
          for (const auto &field : lot.owner.field_order)
            actual.original_owner.field_order.emplace_back(fresh(field));
          if (lot.lot_properties_ref)
            actual.properties_ref =
                LotPropertiesRef{fresh(lot.lot_properties_ref->properties_id)};
          result.consumed.emplace_back(std::move(actual));
        }
      recipe.operations.emplace_back(std::move(consume));
    }
    if (terms->money_cost > 0)
      take(resource_recipe::append_money_transfer(
          recipe, result.payer_wallet_id, result.payee_wallet_id,
          result.cash_paid, resource_recipe::MoneyTransferPolicy::spendable));
    const auto capture_index = x.common.resource_execution_drafts().size();
    take(x.common.apply_resources(recipe));
    const auto &drafts = x.common.resource_execution_drafts();
    if (drafts.size() != capture_index + 1)
      fail("ConstructionOriginalResourceCaptureRequired", result.receipt_id);
    result.actual_resources =
        take(copy_facility_construction_start_cost_capture(
            x.common, capture_index, x.owned_fields));
    take(validate_facility_construction_start_cost_capture(
        x.common, *terms, result, drafts.back(), x.read.clock));
    const auto index = x.state.start_costs.size();
    append_cost(x.state.start_costs, std::move(result));
    take(x.common.append_fact_borrowed(
        receipt_id.id(), "facility_construction_cost", "local_economy"));
    return {true, {index}, {}};
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
