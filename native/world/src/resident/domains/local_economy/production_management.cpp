// 生产管理只配置未开始周期；真实增量劳力预留、取消与产能来源同经济候选原子提交。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "production_management.hpp"
#include "local_economy_codec_detail.hpp"
#include "local_economy_internal.hpp"
#include "takeover/execution_configurations.hpp"
#include <map>
namespace hanmo::resident {
namespace {
using namespace local_economy_detail;
// 管理结果和原生产事实使用同一全局事实序列；调用者持共同保存点。
Outcome<StableId> management_fact(CommonStore &c, const StableId &kind) {
  auto id = c.next_id("production_configuration");
  if (!id.ok)
    return id;
  auto status = c.append_fact({id.value, kind, "local_economy"});
  if (!status.ok)
    return {false, {}, status.error};
  return id;
}
// 待生效的增量占用只能在明确结案时释放，保留原记录供冷读核验。
Status resolve_pending(CommonStore &c, LocalEconomyState &state,
                       const StableId &unit_id, const StableId &kind,
                       Integer tick, LocalEconomyEffects &effects) {
  auto &management = *state.production_management;
  const auto *pending = management.pending_by_unit.find(unit_id);
  if (!pending)
    return common_success();
  const auto *record = management.configurations.find(*pending);
  if (!record)
    return failure("InvalidProductionConfiguration", unit_id);
  for (const auto &id : record->reservation_allocation_ids)
    if (!id.empty()) {
      auto *allocation = state.labor_allocations.find(id);
      if (!allocation || allocation->commitment_id != record->id)
        return failure("InvalidProductionLaborReservation", id);
      allocation->workers = 0;
    }
  auto fact = management_fact(c, "production_configuration_" + kind);
  if (!fact.ok)
    return {false, {}, fact.error};
  management.results.insert(fact.value, {fact.value, record->id, kind, tick});
  management.pending_by_unit.erase(unit_id);
  effects.fact_ids.push_back(fact.value);
  effects.receipt_ids.push_back(fact.value);
  return common_success();
}
} // namespace
// 实际产能按声明的田地与设施取上限，季节只在结算时选固定整数段。
// 旧调用仍走同一规则，中性输入不改变任何舍入、持久字段或结果。
Outcome<ProductionContext>
production_context(const LocalEconomyCatalog &catalog,
                   const ProductionUnit &unit, Integer due_tick) {
  return production_context(catalog, unit, due_tick,
                            ProductionGovernanceInput{});
}
// 非中性治理须先有可回读的原来源，不能让自由数值伪装为已经采用的政策。
Outcome<ProductionContext>
production_context(const LocalEconomyCatalog &catalog,
                   const ProductionUnit &unit, Integer due_tick,
                   const ProductionGovernanceInput &governance) {
  if (governance.factor_bp < 0)
    return {false,
            {},
            common_failure("InvalidProductionGovernanceFactor", unit.id).error};
  if (governance.factor_bp != 10000)
    return {false,
            {},
            common_failure("ProductionGovernanceOriginalSourceRequired",
                           unit.id, ErrorKind::unsupported_capability)
                .error};
  const auto *recipe = catalog.recipes.find(unit.recipe_id);
  if (!recipe)
    return {false,
            {},
            common_failure("UnknownProductionRecipe", unit.recipe_id).error};
  ProductionContext out;
  out.capacity = unit.capacity;
  out.region_factor_bp = recipe->region_factor_bp;
  out.season_factor_bp = recipe->season_factor_bp;
  if (!catalog.production_management)
    return {true, std::move(out), {}};
  const auto &management = *catalog.production_management;
  const auto *plan = management.plans.find(unit.id);
  if (!plan)
    return {true, std::move(out), {}};
  Integer land = 0, facility = 0;
  bool has_land = false, has_facility = false;
  for (const auto &use : plan->asset_uses) {
    const auto *asset = management.assets.find(use.asset_id);
    Integer capacity = 0;
    if (!asset || !multiply(use.units, use.batches_per_unit, capacity))
      return {
          false, {}, failure("InvalidProductionAssetUse", use.asset_id).error};
    auto &total = asset->kind == ProductionAssetKind::land ? land : facility;
    if (!add(total, capacity, total))
      return {
          false, {}, failure("EconomyQuantityOverflow", use.asset_id).error};
    if (asset->kind == ProductionAssetKind::land)
      has_land = true;
    else
      has_facility = true;
    out.asset_ids.push_back(asset->id);
  }
  if (has_land)
    out.capacity = std::min(out.capacity, land);
  if (has_facility)
    out.capacity = std::min(out.capacity, facility);
  if (!plan->season_profile_id.empty()) {
    const auto *season = management.seasons.find(plan->season_profile_id);
    if (!season || season->phase_ticks <= 0 || season->factors_bp.empty() ||
        due_tick < season->origin_tick)
      return {
          false,
          {},
          failure("InvalidProductionSeason", plan->season_profile_id).error};
    out.season_profile_id = season->id;
    out.season_phase =
        ((due_tick - season->origin_tick) / season->phase_ticks) %
        static_cast<Integer>(season->factors_bp.size());
    out.season_factor_bp =
        season->factors_bp[static_cast<std::size_t>(out.season_phase)];
  }
  return {true, std::move(out), {}};
}
// 报价按完整目标人数核Grant上限，并只在副本预演未来预留；不取得ID或改变当期产出。
Outcome<ProductionConfigurationQuote> preview_configure_local_production(
    const CommonStore &common, const LocalEconomyState &state,
    const LocalEconomyCatalog &catalog, const OrganizationRead &organizations,
    const ConfigureLocalProductionCommand &command,
    const EconomicTakeoverReadContext *takeover) {
  const auto fail = [](const StableId &code, const StableId &id = StableId{})
      -> Outcome<ProductionConfigurationQuote> {
    return {false, {}, common_failure(code, id).error};
  };
  if (!catalog.enabled || !catalog.production_management ||
      catalog.production_management->version != 1 ||
      !state.production_management || state.production_management->version != 1)
    return fail("ProductionManagementUnavailable");
  const auto *actor = common.actor(command.actor_id);
  const auto *unit = state.production_units.find(command.unit_id);
  const auto *binding =
      unit ? state.production_bindings.find(unit->id) : nullptr;
  const auto *plan =
      unit ? catalog.production_management->plans.find(unit->id) : nullptr;
  if (!actor || !actor->alive || !actor->available)
    return fail("ProductionManagerUnavailable", command.actor_id);
  if (!unit || !binding || binding->policy_owner != ProvisionOwner::macro ||
      !plan)
    return fail("ProductionUnitNotManageable", command.unit_id);
  const auto location = common.resolved_location(command.actor_id);
  const auto *here =
      location.ok ? std::get_if<SettlementLocation>(&location.value) : nullptr;
  if (!here ||
      (here->place_id != unit->site_id && here->settlement_id != unit->site_id))
    return fail("ProductionManagerNotPresent", unit->id);
  if (takeover &&
      (&takeover->common != &common || &takeover->economy != &state ||
       &takeover->catalog != &catalog))
    return fail("EconomicConfigurationContextMismatch", unit->id);
  Owner manager_owner = unit->owner;
  if (takeover) {
    const auto *pending =
        state.production_management->pending_by_unit.find(unit->id);
    auto owner = economic_configuration_command_owner(
        *takeover, unit->id, pending ? *pending : StableId{});
    if (!owner.ok)
      return {false, {}, owner.error};
    if (owner.value)
      manager_owner = *owner.value;
  }
  if (manager_owner.kind == Owner::Kind::actor) {
    if (manager_owner.id != command.actor_id || command.authority)
      return fail("ProductionOwnerDenied", unit->id);
  } else if (manager_owner.kind == Owner::Kind::organization) {
    if (!command.authority || command.authority->actor_id != command.actor_id ||
        command.authority->organization_id != manager_owner.id)
      return fail("ProductionAuthorityRequired", unit->id);
    Integer requested_workers = 0;
    if (command.kind == ProductionConfigurationKind::configure)
      for (const auto &target : command.targets)
        if (target.workers < 0 ||
            !add(requested_workers, target.workers, requested_workers))
          return fail("InvalidProductionLaborAmount", unit->id);
    auto proof = organizations.authorize(
        *command.authority, "configure_local_production",
        {ScopeRef::Kind::site, unit->site_id}, requested_workers);
    if (!proof.ok)
      return {false, {}, proof.error};
  } else
    return fail("ProductionOwnerUnsupported", unit->id);
  ProductionConfigurationQuote quote;
  quote.command = command;
  quote.requested_tick = common.tick();
  quote.effective_tick = unit->next_due_tick;
  const auto *recipe = catalog.recipes.find(unit->recipe_id);
  if (!recipe || unit->next_due_tick <= common.tick() ||
      !add(unit->next_due_tick, recipe->period_ticks, quote.first_due_tick))
    return fail("InvalidProductionNextCycle", unit->id);
  const auto *pending =
      state.production_management->pending_by_unit.find(unit->id);
  if (pending)
    quote.previous_configuration_id = *pending;
  if (command.kind == ProductionConfigurationKind::cancel_pending) {
    if (!pending || !command.targets.empty())
      return fail("NoPendingProductionConfiguration", unit->id);
    return {true, std::move(quote), {}};
  }
  if (command.targets.size() != unit->labor_allocation_ids.size() ||
      command.targets.size() > 256)
    return fail("IncompleteProductionLaborPlan", unit->id);
  auto candidate = state;
  if (pending) {
    const auto *previous =
        candidate.production_management->configurations.find(*pending);
    if (!previous)
      return fail("InvalidProductionConfiguration", *pending);
    for (const auto &id : previous->reservation_allocation_ids)
      if (!id.empty()) {
        auto *allocation = candidate.labor_allocations.find(id);
        if (!allocation)
          return fail("InvalidProductionLaborReservation", id);
        allocation->workers = 0;
      }
  }
  std::set<StableId> seen;
  Integer ordinal = 0;
  for (const auto &target : command.targets) {
    const auto *current =
        candidate.labor_allocations.find(target.allocation_id);
    const auto *authored = catalog.labor_allocations.find(target.allocation_id);
    const auto *cohort =
        current ? candidate.cohorts.find(current->cohort_id) : nullptr;
    if (!seen.insert(target.allocation_id).second ||
        !has(unit->labor_allocation_ids, target.allocation_id) || !current ||
        !authored || !cohort ||
        current->activity_or_production_id != unit->id ||
        current->cohort_id != authored->cohort_id || target.workers < 0 ||
        target.workers > authored->workers ||
        current->through_tick < quote.first_due_tick ||
        current->from_tick > quote.effective_tick)
      return fail("InvalidProductionLaborTarget", target.allocation_id);
    if (target.workers > 0 && (cohort->status == PopulationStatus::training ||
                               cohort->status == PopulationStatus::serving ||
                               cohort->status == PopulationStatus::wounded ||
                               cohort->status == PopulationStatus::captive ||
                               cohort->site_id != unit->site_id))
      return fail("ProductionLaborUnavailable", cohort->id);
    const Integer increase =
        std::max(Integer(0), target.workers - current->workers);
    if (increase) {
      StableId id = "production_preview." + std::to_string(ordinal++);
      while (candidate.labor_allocations.find(id))
        id += "_";
      auto status = allocate_labor(common, candidate,
                                   {id, current->cohort_id, unit->id, increase,
                                    quote.effective_tick, current->through_tick,
                                    "production_preview"});
      if (!status.ok)
        return {false, {}, status.error};
    }
  }
  return {true, std::move(quote), {}};
}
// 命令组合已持共同事务；本叶只暂存经济写集，不再嵌套Common保存点。
Outcome<LocalEconomyEffects> apply_configure_local_production_in_candidate(
    CommonStore &common, LocalEconomyState &state,
    const LocalEconomyCatalog &catalog, const OrganizationRead &organizations,
    const ConfigureLocalProductionCommand &command,
    EconomicTakeoverPhaseContext *takeover) {
  if (takeover &&
      (&takeover->read.common != &common || &takeover->read.economy != &state ||
       &takeover->read.catalog != &catalog || takeover->state.version != 1))
    return {
        false,
        {},
        common_failure("EconomicConfigurationContextMismatch", command.unit_id)
            .error};
  auto quoted = preview_configure_local_production(
      common, state, catalog, organizations, command,
      takeover ? &takeover->read : nullptr);
  if (!quoted.ok)
    return {false, {}, quoted.error};
  CandidateStateGuard<LocalEconomyState> state_guard(state);
  auto &candidate = state;
  std::optional<CandidateStateGuard<EconomicTakeoverExecutionState>>
      execution_guard;
  if (takeover)
    execution_guard.emplace(takeover->state);
  LocalEconomyEffects effects;
  auto resolved = resolve_pending(
      common, candidate, command.unit_id,
      command.kind == ProductionConfigurationKind::cancel_pending
          ? "cancelled"
          : "superseded",
      common.tick(), effects);
  if (!resolved.ok)
    return {false, {}, resolved.error};
  if (command.kind == ProductionConfigurationKind::configure) {
    auto id = management_fact(common, "production_configuration_requested");
    if (!id.ok)
      return {false, {}, id.error};
    ProductionConfigurationRecord record;
    record.id = id.value;
    record.unit_id = command.unit_id;
    record.actor_id = command.actor_id;
    record.authority = command.authority;
    record.requested_tick = common.tick();
    record.effective_tick = quoted.value.effective_tick;
    record.first_due_tick = quoted.value.first_due_tick;
    record.targets = command.targets;
    for (const auto &target : record.targets) {
      const auto *allocation =
          candidate.labor_allocations.find(target.allocation_id);
      record.prior_workers.push_back(allocation->workers);
      StableId reservation;
      const Integer increase =
          std::max(Integer(0), target.workers - allocation->workers);
      if (increase) {
        auto next = common.next_id("production_labor_reservation");
        if (!next.ok)
          return {false, {}, next.error};
        reservation = next.value;
        LaborAllocation held{
            reservation, allocation->cohort_id, command.unit_id,
            increase,    record.effective_tick, allocation->through_tick,
            record.id};
        auto status = allocate_labor(common, candidate, held);
        if (!status.ok)
          return {false, {}, status.error};
      }
      record.reservation_allocation_ids.push_back(reservation);
    }
    candidate.production_management->configurations.insert(record.id, record);
    candidate.production_management->pending_by_unit.set(command.unit_id,
                                                         record.id);
    effects.fact_ids.push_back(record.id);
    effects.receipt_ids.push_back(record.id);
  }
  if (takeover) {
    auto recorded = record_economic_configuration_command_sources(
        takeover->read, takeover->state, command, quoted.value, effects);
    if (!recorded.ok)
      return {false, {}, recorded.error};
  }
  auto validated = validate_production_management(
      common, candidate, catalog, takeover ? &takeover->read : nullptr,
      takeover ? &takeover->state : nullptr);
  if (!validated.ok)
    return {false, {}, validated.error};
  state_guard.commit();
  if (execution_guard)
    execution_guard->commit();
  return {true, std::move(effects), {}};
}
// 独立领域调用仍有自己的完整回滚门；原预览错误优先级保持，不依赖调用者额外恢复。
Outcome<LocalEconomyEffects>
apply_configure_local_production(CommonStore &common, LocalEconomyState &state,
                                 const LocalEconomyCatalog &catalog,
                                 const OrganizationRead &organizations,
                                 const ConfigureLocalProductionCommand &command,
                                 EconomicTakeoverPhaseContext *takeover) {
  auto preview = preview_configure_local_production(
      common, state, catalog, organizations, command,
      takeover ? &takeover->read : nullptr);
  if (!preview.ok)
    return {false, {}, preview.error};
  PhaseSavepoint guard(common);
  if (!guard.token().ok)
    return {false, {}, guard.token().error};
  CandidateStateGuard<LocalEconomyState> state_guard(state);
  std::optional<CandidateStateGuard<EconomicTakeoverExecutionState>>
      execution_guard;
  if (takeover)
    execution_guard.emplace(takeover->state);
  auto result = apply_configure_local_production_in_candidate(
      common, state, catalog, organizations, command, takeover);
  if (!result.ok)
    return result;
  auto committed = guard.commit();
  if (!committed.ok)
    return {false, {}, committed.error};
  state_guard.commit();
  if (execution_guard)
    execution_guard->commit();
  return result;
}
// 旧签名不隐式启用接管；历史Profile继续走原公私权限路径。
Outcome<ProductionConfigurationQuote> preview_configure_local_production(
    const CommonStore &c, const LocalEconomyState &s,
    const LocalEconomyCatalog &cat, const OrganizationRead &org,
    const ConfigureLocalProductionCommand &command) {
  return preview_configure_local_production(c, s, cat, org, command, nullptr);
}
Outcome<LocalEconomyEffects> apply_configure_local_production(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &cat,
    const OrganizationRead &org,
    const ConfigureLocalProductionCommand &command) {
  return apply_configure_local_production(c, s, cat, org, command, nullptr);
}
Outcome<LocalEconomyEffects> apply_configure_local_production_in_candidate(
    CommonStore &c, LocalEconomyState &s, const LocalEconomyCatalog &cat,
    const OrganizationRead &org,
    const ConfigureLocalProductionCommand &command) {
  return apply_configure_local_production_in_candidate(c, s, cat, org, command,
                                                       nullptr);
}
// 旧周期先结算，随后才改既有劳力人数；记录中的旧人数保证其他命令不能偷用待移交人手。
Status activate_production_configuration(CommonStore &common,
                                         LocalEconomyState &state,
                                         const StableId &unit_id,
                                         Integer due_tick,
                                         LocalEconomyEffects &effects) {
  if (!state.production_management)
    return common_success();
  auto &management = *state.production_management;
  const auto *id = management.pending_by_unit.find(unit_id);
  if (!id)
    return common_success();
  const auto *record = management.configurations.find(*id);
  if (!record || record->effective_tick != due_tick ||
      record->targets.size() != record->prior_workers.size())
    return failure("InvalidProductionConfiguration", unit_id);
  for (std::size_t i = 0; i < record->targets.size(); ++i) {
    auto *allocation =
        state.labor_allocations.find(record->targets[i].allocation_id);
    if (!allocation || allocation->workers != record->prior_workers[i])
      return failure("ProductionConfigurationLaborChanged", unit_id);
    allocation->workers = record->targets[i].workers;
  }
  return resolve_pending(common, state, unit_id, "applied", due_tick, effects);
}
// 配置命令只接受明确枚举、完整目标和真实公职身份，不接任意字典setter。
Outcome<ConfigureLocalProductionCommand>
decode_configure_local_production(const BoundaryValue &value) {
  try {
    using namespace boundary;
    fields(value, {"actor_id", "unit_id", "kind", "authority", "targets"}, {},
           "/configure_local_production");
    ConfigureLocalProductionCommand out;
    out.actor_id = get_string(value, "actor_id", "");
    out.unit_id = get_string(value, "unit_id", "");
    const auto kind = get_string(value, "kind", "");
    if (kind == "configure")
      out.kind = ProductionConfigurationKind::configure;
    else if (kind == "cancel_pending")
      out.kind = ProductionConfigurationKind::cancel_pending;
    else
      fail("InvalidProductionConfigurationKind", "/kind");
    dynamic_economy_codec::decode(field(value, "authority", ""), out.authority,
                                  "/authority");
    dynamic_economy_codec::decode(field(value, "targets", ""), out.targets,
                                  "/targets");
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
// 保存命令只输出既有typed参数，不包含任意世界字段或可执行脚本。
BoundaryValue encode_configure_local_production(
    const ConfigureLocalProductionCommand &command) {
  using V = BoundaryValue;
  return boundary::obj(
      {{"actor_id", V{command.actor_id}},
       {"unit_id", V{command.unit_id}},
       {"authority", dynamic_economy_codec::encode(command.authority)},
       {"kind", V{command.kind == ProductionConfigurationKind::configure
                      ? "configure"
                      : "cancel_pending"}},
       {"targets", dynamic_economy_codec::encode(command.targets)}});
}
// 预览明确显示旧周期照常完成以及新计划第一次产出时间，前端不回算或预加产量。
BoundaryValue encode_production_configuration_quote(
    const ProductionConfigurationQuote &quote) {
  using V = BoundaryValue;
  return boundary::obj(
      {{"unit_id", V{quote.command.unit_id}},
       {"kind", V{quote.command.kind == ProductionConfigurationKind::configure
                      ? "configure"
                      : "cancel_pending"}},
       {"requested_tick", V{quote.requested_tick}},
       {"effective_tick", V{quote.effective_tick}},
       {"first_due_tick", V{quote.first_due_tick}},
       {"previous_configuration_id", V{quote.previous_configuration_id}},
       {"targets", dynamic_economy_codec::encode(quote.command.targets)}});
}
} // namespace hanmo::resident
