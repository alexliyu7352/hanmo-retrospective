// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 同一实际 command 的本人同意、真实产权头、用途和开工结果必须全部一致。
#include "production_asset_construction_authority.hpp"
#include "resident/kernel/command_types.hpp"
#include "resident/kernel/common_command_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"

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
int compare(const WorldMoment &a, const WorldMoment &b) {
  return take(compare_world_moments(a, b));
}
bool same_rule(const EconomicRuleRef &a, const EconomicRuleRef &b) {
  return a.id == b.id && a.version == b.version &&
         a.catalog_digest == b.catalog_digest;
}
// 只借真实同意行的原 Common 头；目录名字、任意 fact 或重复行均不能替代。
const ProductionAssetConstructionConsent &
consent(const ProductionAssetLifecycleState &state,
        const MomentOwnerSource &source) {
  if (source.source_schema != 1 ||
      source.source_ref.domain != "local_economy" ||
      source.source_ref.kind != "production_asset_construction_consented")
    fail("ProductionAssetOriginalConstructionConsentRequired",
         source.source_ref.id);
  const ProductionAssetConstructionConsent *found = nullptr;
  for (const auto &row : state.construction_consents)
    if (row.consent_id == source.source_ref.id) {
      if (found)
        fail("DuplicateProductionAssetConstructionConsent",
             source.source_ref.id);
      found = &row;
    }
  if (!found)
    fail("ProductionAssetOriginalConstructionConsentRequired",
         source.source_ref.id);
  return *found;
}
// 原有限开工writer只消费施工本人私产。冷门必须反查实际耗料/扣款前帧，
// 不因actor今天仍持有该资产就允许把公库或他人材料补写成当年的成本。
void personal_cost(const FacilityConstructionAdoptionContext &context,
                   const FacilityConstruction &started, const StableId &actor) {
  const ConstructionStartCost *cost = nullptr;
  for (const auto &row : context.state.start_costs)
    if (row.receipt_id == started.actual_cost_ref) {
      if (cost)
        fail("DuplicateConstructionCost", started.actual_cost_ref);
      cost = &row;
    }
  if (!cost)
    fail("ConstructionOriginalCostMismatch", started.actual_cost_ref);
  const auto private_owner = [&](const Owner &owner) {
    return owner.kind == Owner::Kind::actor && owner.id == actor &&
           !owner.extra;
  };
  // adoption已用唯一原语正文验完整帧与原批摘要，下面只核当时合法主体。
  for (const auto &effect : cost->actual_resources.materials) {
    if (effect.request.owners.size() != 1 ||
        !private_owner(effect.request.owners.front()) ||
        effect.inventories.size() != 1 || !effect.inventories.front().before ||
        !private_owner(effect.inventories.front().before->owner))
      fail("ConstructionOriginalPersonalMaterialAuthorityRequired",
           cost->receipt_id);
  }
  for (const auto &material : cost->consumed)
    if (!private_owner(material.original_owner))
      fail("ConstructionOriginalPersonalMaterialAuthorityRequired",
           material.actual.lot_id);
  if (cost->actual_resources.payment &&
      !private_owner(cost->actual_resources.payment->debit.before.owner))
    fail("ConstructionOriginalPersonalPayerAuthorityRequired",
         cost->receipt_id);
}
} // namespace

Outcome<const ProductionAssetConstructionConsent *>
read_production_asset_original_construction_consent(
    const ProductionAssetLifecycleReadContext &x,
    const FacilityConstructionAdoptionContext &construction,
    const MomentOwnerSource &actual_start) {
  try {
    // 唯一原 title cut 已核两个 context 的 Common/资产/设施/完整锁和时钟。
    // adoption 不回入本权源读，避免以循环自证同意或工程完整合法性。
    const auto title = take(read_production_asset_title_before_construction(
        x, construction, actual_start));
    const auto *started =
        take(read_facility_construction_adoption_for_original_dependency(
            construction, actual_start));
    const auto *terms = take(read_facility_construction_terms(
        construction.catalog, started->terms_ref));
    if (started->authority_sources.size() != 1)
      fail("ProductionAssetPersonalConstructionAuthorityRequired",
           started->construction_id);
    const auto &source = started->authority_sources.front();
    const auto &agreed = consent(x.state, source);
    if (agreed.asset_ref != terms->asset_ref ||
        agreed.owner_actor_id != agreed.builder_actor_id ||
        agreed.builder_actor_id != started->builder_actor_id ||
        !same_rule(agreed.construction_terms_ref, started->terms_ref) ||
        compare(agreed.consented_at, started->started_at) ||
        !production_asset_title_owned_by(title, Owner::Kind::actor,
                                         agreed.owner_actor_id) ||
        !production_asset_title_source_equal(title, agreed.title_source))
      fail("ProductionAssetActualConstructionConsentMismatch",
           agreed.consent_id);
    personal_cost(construction, *started, agreed.builder_actor_id);
    const auto &common = construction.common;
    const auto consent_order =
        take(read_actual_common_fact_position(common, source.source_ref));
    const auto cost_order = take(read_actual_common_fact_position(
        common,
        SourceRefBuildView{"local_economy", "facility_construction_cost",
                           started->actual_cost_ref}));
    const auto start_order =
        take(read_actual_common_fact_position(common, actual_start.source_ref));
    if (consent_order >= cost_order || cost_order >= start_order)
      fail("ProductionAssetConstructionConsentOrderMismatch",
           agreed.consent_id);
    // 本分支只借仍未终止的原本人初始用途。购买后新权源尚未实现，不能
    // 把新买方 title 或将来买回的同一 Owner 变成旧许可重新生效。
    if (!title.initial_owner ||
        title.initial_owner->kind != Owner::Kind::actor ||
        title.initial_owner->id != agreed.owner_actor_id)
      fail("ProductionAssetConstructionUsageSourceRequired", agreed.consent_id);
    const auto usage = take(read_production_asset_initial_usage_history(
        x, agreed.usage_source, *title.initial_owner));
    if (usage.facility != title.original_facility ||
        usage.facility->place_ref != terms->place_id || !usage.opened_at ||
        compare(*usage.opened_at, started->started_at) > 0)
      fail("ProductionAssetConstructionUsageMismatch", agreed.consent_id);
    if (usage.ended_at) {
      if (!usage.actual_termination)
        fail("ProductionAssetConstructionUsageTerminationRequired",
             agreed.consent_id);
      const auto ended = compare(*usage.ended_at, started->started_at);
      const auto ended_order = take(read_actual_common_fact_position(
          common, SourceRefBuildView{"local_economy",
                                     "production_asset_title_transferred",
                                     usage.actual_termination->transfer_id}));
      if (ended < 0 || (ended == 0 && ended_order < start_order))
        fail("ProductionAssetConstructionUsageEnded", agreed.consent_id);
    }
    // 不是任意成功命令：必须原 PlanFacilityCapacity 的实际本人、原半日、
    // 完整同意/成本/开工顺序以及唯一成本回执，全部从真正成功缓存反读。
    const auto original =
        take(read_common_command_source(common, agreed.owner_command_source));
    if (original.history->actor_id != agreed.owner_actor_id ||
        original.result->kind != "plan_facility_capacity" ||
        original.result->tick != agreed.consented_at.half_day_tick ||
        !original.result->details)
      fail("ProductionAssetActualConstructionCommandRequired",
           agreed.consent_id);
    const auto *effects =
        std::get_if<LocalEconomyEffects>(&original.result->details->data);
    if (!effects || effects->fact_ids.size() != 3 ||
        effects->fact_ids[0] != agreed.consent_id ||
        effects->fact_ids[1] != started->actual_cost_ref ||
        effects->fact_ids[2] != started->construction_id ||
        effects->receipt_ids.size() != 1 ||
        effects->receipt_ids[0] != started->actual_cost_ref)
      fail("ProductionAssetConstructionCommandResultsMismatch",
           agreed.consent_id);
    // 同一个实际同意只给该次锁定开工，不能用旧 command 重开第二个工程。
    for (const auto &other : construction.state.constructions)
      if (&other != started)
        for (const auto &authority : other.authority_sources)
          if (same_source_ref(authority.source_ref, source.source_ref))
            fail("ProductionAssetConstructionConsentReused", agreed.consent_id);
    return {true, &agreed, {}};
  } catch (const Failure &failure) {
    return {false, nullptr, failure.error};
  }
}
Status validate_production_asset_construction_period_usage(
    const ProductionAssetLifecycleReadContext &assets,
    const FacilityConstructionAdoptionContext &adoption,
    const FacilityConstruction &construction, const WorkPeriodRef2 &period) {
  try {


    auto memory = CheckpointMemoryCharge(0);
    const auto &ref = construction.execution_binding.source_owner_ref;
    const MomentOwnerSource source{{checkpoint_fresh_string_copy(ref.domain),
                                    checkpoint_fresh_string_copy(ref.kind),
                                    checkpoint_fresh_string_copy(ref.id)},
                                   1};
    const auto *actual =
        take(read_facility_construction_adoption_for_original_dependency(
            adoption, source));
    const auto *binding = std::get_if<ActorActionWorkBinding>(&period.binding);
    if (actual != &construction || !binding ||
        binding->action_id != construction.construction_id ||
        binding->action_kind != construction.execution_binding.action_kind ||
        !same_source_ref(binding->source_owner_ref, ref))
      fail("ConstructionActualPeriodOwnerRequired",
           construction.construction_id);
    take(validate_work_period_ref2(period));
    const auto *consent =
        take(read_production_asset_original_construction_consent(
            assets, adoption, source));
    const Owner original_owner{
        Owner::Kind::actor,
        checkpoint_fresh_string_copy(consent->owner_actor_id),
        {}};
    const auto right = take(read_production_asset_initial_usage_history(
        assets, consent->usage_source, original_owner));
    if (!right.opened_at || compare(*right.opened_at, period.from) > 0 ||
        (right.ended_at && compare(*right.ended_at, period.until) < 0))
      fail("ConstructionActualPeriodUsageCoverageRequired",
           construction.construction_id);
    return common_success();
  } catch (const Failure &failure) {
    return {false, {}, failure.error};
  }
}
} // namespace hanmo::resident
