#include "native_political_world_scope.hpp"
#include "economic_supply_policy_integration.hpp"
#include "estate_employment_integration.hpp"
#include "native_character_family_world.hpp"
#include "native_complete_world_core.hpp"
#include "native_domain_read_versions.hpp"
#include "native_production_cycles.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/family/family_life_exact.hpp"
#include "resident/domains/growth/growth_fixed_spatial_inputs.hpp"
#include "resident/domains/local_economy/production/production_input_lease.hpp"
#include "resident/domains/local_economy/supply/supply_policy_profile.hpp"
#include "resident/domains/politics/political_p1_native_scope.hpp"
#include "resident/domains/politics/politics_fiscal_time2.hpp"
#include "resident/domains/politics/takeover/politics_takeover_fiscal.hpp"
#include "resident/domains/politics/takeover/politics_takeover_original_access.hpp"
#include "resident/domains/politics/takeover/politics_takeover_profile.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/domains/politics/takeover/politics_occupation_fiscal_profile.hpp"
#include "resident/domains/tasks/task_public_account_payment.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include <new>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void check(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
[[noreturn]] void fail(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
void same_at(const WorldMoment &at, Outcome<WorldMoment> value) {
  if (take(compare_world_moments(at, take(std::move(value)))) != 0)
    fail("PoliticalWorldScopeMomentMismatch");
}
// 只转发本 scope 的稳定原绑定；不调用 Complete，也不另建业务源。
class ForwardPoliticalReadLease final : public PoliticalExactReadLease {
  const PoliticalExactReadBinding &binding_;

public:
  explicit ForwardPoliticalReadLease(const PoliticalExactReadBinding &binding)
      : binding_(binding) {}
  PoliticalExactReadBinding bindings() const override { return binding_; }
};
// 真死亡读要求每个被查人物有 actor 资源票；全政治/Office 读覆盖所有实际人物。
// 临时普通 vector 及字符串先预留，持续持账到原 capture 复制完成。
std::shared_ptr<const ReadStamp>
capture_actors(const WorldBundle &world,
               const DomainReadVersionSource &versions) {
  const auto &actors = world.common.state().actors;


  for (const auto &row : actors.rows()) {
    if (row.first.empty() || row.second.id != row.first)
      fail("PoliticalWorldScopeActorIdentityMismatch");

  }
  auto memory = CheckpointMemoryCharge(0);
  std::vector<ResourceKey> resources;
  resources.reserve(actors.size());
  for (const auto &row : actors.rows())
    resources.push_back(
        {ResourceKey::Kind::actor, checkpoint_fresh_string_copy(row.first)});
  return take(
      WorldCurrentReadStampRead::capture_binding(world, versions, resources));
}
} // namespace
// 成员次序就是寿命：原身体/版本/死亡/票据先活，P1 与写 binding 后活先析构。
struct NativePoliticalWorldScope::Sources final : CheckpointAllocatedObject {
  const NativeCompleteWorldCoreSources &core;
  const WorldBundle &world;
  WorldMoment at;
  NativePoliticalWorldLateRead late;
  class AuthorityClock final : public OrganizationAuthorityClock {
    const WorldBundle &world_;

  public:
    explicit AuthorityClock(const WorldBundle &w) : world_(w) {}
    Outcome<WorldMoment> read_organization_authority_now() const override {
      return world_.actual_phase_moment();
    }
  };
  AuthorityClock clock;
  CharacterConditionSources body;
  NativeDomainReadVersionSource versions;
  FamilyVerifiedDeathRead deaths;
  std::shared_ptr<const ReadStamp> stamp;
  // 只拥有原真实读对象；内联 optional 的固定对象体随 Sources 一次计费。
  std::optional<EstateEmploymentWorldAccess> employment;
  std::optional<PrivateShopResponsibilitySourceContext> shop_sources;
  std::optional<EconomicSupplyPendingAccess> supply_pending;
  std::optional<PoliticalTakeoverOriginalAccess> takeover_originals;
  std::optional<PoliticalTakeoverContext> takeover;
  std::optional<PoliticalTakeoverTerritoryAccess> territories;
  std::optional<PoliticalTakeoverFiscalAccess> fiscal_occupation;
  std::optional<PoliticalFiscalTime2Context> fiscal;
  std::unique_ptr<PoliticalExactReadLease> read_lease;
  std::optional<PoliticalExactReadBinding> read_binding;
  // 生产借口须与上方真正 P1 lease 的原 authority clock 同对象。
  std::optional<PoliticalTakeoverContext> production_takeover;
  std::optional<PoliticalTakeoverFiscalAccess> read_fiscal_occupation;
  std::optional<PoliticalFiscalTime2Context> read_fiscal;
  std::optional<ForwardPoliticalReadLease> forwarded_read;
  std::optional<EconomicProductionContext> production;
  std::optional<ProductionPopulationChangeContext> population_change;
  std::unique_ptr<PoliticalExactWriteLease> write_lease;
  std::optional<PoliticalExactWriteBinding> write_binding;
  NativeCompleteWorldSourceExports exports;

  Sources(const NativeCompleteWorldCoreSources &c, const WorldMoment &m,
          NativePoliticalWorldLateRead l, CharacterConditionSources b)
      : core(c), world(c.characters.early.world), at(m), late(l), clock(world),
        body(std::move(b)),
        versions(world, c.characters.early.formations,
                 c.characters.early.returns, c.characters.early.periods),
        deaths(world.common, world.family_life, *body.life_sources,
               *world.read_stamps, versions, body.military_history_order) {}

  static std::unique_ptr<Sources>
  create(const WorldBundle &world, const NativeCompleteWorldCoreSources &core,
         const WorldMoment &at, NativePoliticalWorldLateRead late) {
    const auto &early = core.characters.early;
    if (&early.world != &world || !world.catalog || !world.catalog->common ||
        !world.catalog->actor_foundation || !world.actor_foundation ||
        !world.catalog->organization || !world.organization_runtime ||
        !world.catalog->politics || !world.catalog->local_economy ||
        !world.read_stamps || !world.has_active_read_phase())
      fail("PoliticalWorldScopeOriginalRootsRequired");
    same_at(at, world.actual_phase_moment());
    same_at(at, early.clock.read_current_moment());
    check(core.characters.family.validate_bindings());
    auto body = take(core.characters.family.body().sources());
    if (&body.common != &world.common ||
        &body.state != &*world.actor_foundation ||
        &body.catalog != world.catalog->actor_foundation.get() ||
        body.shared != world.catalog->common.get() ||
        body.deaths != &world.family_life || !body.life_sources ||
        &body.life_sources->military != &world.military ||
        body.life_sources->military_exact !=
            (world.military_exact ? &*world.military_exact : nullptr) ||
        body.life_sources->catalog_selection != &world.catalog->identity ||
        body.life_sources->military_formations != early.formations ||
        body.life_sources->military_returns != early.returns)
      fail("PoliticalWorldScopeFamilyBindingMismatch");
    check(validate_family_life4_original_military_selection(
        world.common, *body.life_sources));
    same_at(at, {true, body.actual_now, {}});
    auto value =
        std::unique_ptr<Sources>(new Sources(core, at, late, std::move(body)));
    value->stamp = capture_actors(world, value->versions);
    check(world.read_stamps->validate(
        *value->stamp, world.common.state().identity, ReadMode::phase_current,
        world.common, value->versions));
    same_at(at, {true, value->stamp->span().to, {}});
    value->bind_political_sources();
    return value;
  }

  void bind_political_sources() {
    const auto &w = world;
    const auto &c = *w.catalog;
    if (late.actual_phase)
      (void)take(
          political_takeover_evaluation_tick(w.common, late.actual_phase));
    const bool selected = c.political_takeover && c.political_takeover->enabled;
    check(validate_political_takeover_layout(
        selected, c.identity.module_versions,
        w.political_takeover_seed_complete ? &w.political_takeover : nullptr,
        w.economic_takeover_seed_complete
            ? std::optional<Integer>{w.economic_takeover_execution.version}
            : std::nullopt,
        w.tasks.takeover_execution
            ? std::optional<Integer>{w.tasks.takeover_execution->version}
            : std::nullopt,
        c.political_takeover ? c.political_takeover->task_execution_version : 1,
        2));
    check(validate_political_takeover_command_history(w.common, selected));
    if (!selected)
      check(validate_unselected_political_takeover_sources(w));
    if (selected) {
      if (c.political_takeover->version != 2 || !c.military || !c.tasks ||
          !w.dynamic_enabled() || !w.politics_enabled())
        fail("PoliticalWorldScopeTakeoverRootsRequired");
      // 沿原 factory 的真实结构借用；不经只支持 /1 的旧 WorldAccess。
      if (w.estate_employment_enabled()) {
        if (!w.estate_employment_seed_complete || !c.estate || !c.interaction ||
            !c.estate_employment)
          fail("PoliticalWorldScopeEmploymentRootsRequired");
        employment.emplace(w, late.actual_phase);
      }
      if (w.private_shops_enabled()) {
        if (!w.private_shops_seed_complete || !c.private_shops)
          fail("PoliticalWorldScopeShopRootsRequired");
        if (w.private_shop_daily_rent_enabled() &&
            !w.private_shop_daily_rent_seed_complete)
          fail("PoliticalWorldScopeRentRootsRequired");
        shop_sources.emplace(PrivateShopResponsibilitySourceContext{
            w.common, w.local_economy, *c.local_economy, w.private_shops,
            *c.private_shops, late.actual_phase,
            c.private_shop_daily_rent.get(),
            w.private_shop_daily_rent_enabled() ? &w.private_shop_daily_rent
                                                : nullptr});
      }
      if (w.fiscal_settlement_enabled() && !w.fiscal_settlement_seed_complete)
        fail("PoliticalWorldScopeFiscalSettlementRootsRequired");
      if (w.public_relief_enabled() && !w.public_relief_seed_complete)
        fail("PoliticalWorldScopeReliefRootsRequired");
      if (w.governance_tasks_enabled() && !w.governance_tasks_seed_complete)
        fail("PoliticalWorldScopeGovernanceTaskRootsRequired");
      if (w.envoy_tasks_enabled() && !w.envoy_tasks_seed_complete)
        fail("PoliticalWorldScopeEnvoyTaskRootsRequired");
      if (w.occupation_fiscal_enabled() &&
          !w.occupation_fiscal_settlement_seed_complete)
        fail("PoliticalWorldScopeOccupationFiscalRootsRequired");
      // 原供给 /1 布局尚未接受 Takeover2，照原门拒绝，不造空 pending reader。
      check(validate_economic_supply_layout(c.economic_supply_policies_enabled,
                                            c.identity.module_versions));
      if (!c.economic_supply_policies_enabled)
        check(validate_economic_supply_world(w));
      if (c.economic_supply_policies_enabled) {
        if (!c.economic_supply_policies ||
            !w.economic_supply_policies_seed_complete ||
            !w.political_supply_adoptions_seed_complete)
          fail("PoliticalWorldScopeSupplyRootsRequired");
        supply_pending.emplace(EconomicSupplyPendingContext{
            w.common, *c.common, w.local_economy, *c.local_economy,
            *c.economic_supply_policies, w.economic_supply_policies,
            late.actual_phase});
      }
      takeover_originals.emplace(PoliticalTakeoverOriginalContext{
          w.common, w.military, *c.military,
          core.characters.early.military_access, w.local_economy,
          *c.local_economy,
          w.fiscal_settlement_enabled() ? &w.fiscal_settlement : nullptr,
          w.fiscal_settlement_enabled() ? c.fiscal_settlement.get() : nullptr,
          w.public_relief_enabled() ? &w.public_relief : nullptr, w.tasks,
          *c.tasks,
          w.governance_tasks_enabled() ? &w.governance_tasks : nullptr,
          w.envoy_tasks_enabled() ? &w.envoy_tasks : nullptr, late.actual_phase,
          w.occupation_fiscal_enabled() ? &w.occupation_fiscal_settlement
                                        : nullptr,
          employment ? &employment->context() : nullptr,
          shop_sources ? &*shop_sources : nullptr});
      takeover.emplace(PoliticalTakeoverContext{
          w.common, *c.common, w.organization, *c.organization, *c.politics,
          *c.political_takeover, w.political_takeover, w.military, *c.military,
          core.characters.early.military_access, *takeover_originals,
          late.actual_phase,
          c.economic_supply_policies_enabled ? &w.political_supply_adoptions
                                             : nullptr,
          supply_pending ? &*supply_pending : nullptr, &clock,
          &*w.organization_runtime, &w.politics});
      territories.emplace(*takeover);
      fiscal_occupation.emplace(*takeover);
    }
    const bool fiscal_selected =
        c.political_fiscal && c.political_fiscal->enabled;
    unsigned fiscal_modules = 0;
    for (const auto &module : c.identity.module_versions)
      if (module.first == "political_fiscal") {
        if (module.second != "2" || ++fiscal_modules != 1)
          fail("PoliticalWorldScopeFiscalSelectionMismatch");
      }
    if (fiscal_selected != (fiscal_modules == 1) ||
        fiscal_selected != w.political_fiscal_seed_complete)
      fail("PoliticalWorldScopeFiscalSelectionMismatch");
    if (fiscal_selected) {
      if (c.political_fiscal->version != 2 || w.political_fiscal.version != 2)
        fail("PoliticalWorldScopeFiscalRootsRequired");
      fiscal.emplace(PoliticalFiscalTime2Context{
          w.common, *c.common, w.politics, w.political_fiscal,
          *c.political_fiscal, w.organization, *c.organization,
          *w.organization_runtime, clock, core.characters.family.ages(),
          fiscal_occupation ? &*fiscal_occupation : nullptr,
          takeover ? &*takeover : nullptr});
    } else {
      if (!w.political_fiscal.policies.empty())
        fail("PoliticalWorldScopeUnexpectedFiscalState");
      for (const auto &row : w.common.state().facts.rows())
        if (row.second.kind == "political_fiscal_policy_changed")
          fail("PoliticalWorldScopeUnexpectedFiscalState");
    }
  }
  NativePoliticalP1ReadInputs read_inputs() const {
    const auto &early = core.characters.early;
    return {{*stamp, late.installation, fiscal ? &*fiscal : nullptr,
             takeover ? &*takeover : nullptr, late.governance, early.formations,
             early.returns, early.periods},
            core.action_time,
            core.characters.family.ages(),
            deaths,
            core.characters.spatial.original,
            core.organization_resources,
            late.staffing,
            late.population_military,
            territories ? &*territories : nullptr,
            late.treaty_exact};
  }
  void bind_read() {
    read_lease =
        take(make_native_political_p1_read_lease(world, at, read_inputs()));
    read_binding.emplace(read_lease->bindings());
    if (takeover) {
      production_takeover.emplace(*takeover);
      production_takeover->actual_clock = &read_binding->organization.clock;
      same_at(
          at,
          production_takeover->actual_clock->read_organization_authority_now());
    }
    if (production_takeover)
      read_fiscal_occupation.emplace(*production_takeover);
    if (fiscal) {
      const auto &f = *fiscal;
      read_fiscal.emplace(PoliticalFiscalTime2Context{
          f.common, f.common_catalog, f.politics, f.state, f.catalog,
          f.organizations, f.organization_catalog, f.runtime,
          read_binding->organization.clock, f.ages,
          read_fiscal_occupation ? &*read_fiscal_occupation : nullptr,
          production_takeover ? &*production_takeover : nullptr});
    }
    read_binding->takeover =
        production_takeover ? &*production_takeover : nullptr;
    read_binding->fiscal = read_fiscal ? &*read_fiscal : nullptr;
    forwarded_read.emplace(*read_binding);
    exports.political = &*read_binding;
  }
  // 只采用已选经济 owner 的真实原输入；同钟地址强核，不凭数值等时替代同源。
  void bind_write(WorldBundle &w, const NativePoliticalWorldLateWrite &input) {
    if (&w != &world || !read_binding || !w.catalog->production_cycles ||
        !w.catalog->exact_time || !w.production_cycles)
      fail("PoliticalWorldScopeProductionRootsRequired");
    check(validate_native_production_cycles_layout(w));
    check(input.production_inputs.validate_bindings());
    const auto &original = input.production_inputs.population_changes();
    const auto &p = input.production;
    if (&p.catalog != w.catalog->production_cycles.get() ||
        &p.state != &*w.production_cycles ||
        &p.inputs != &input.production_inputs.inputs() ||
        &p.clock != &original.population.clock ||
        p.units_per_half_day !=
            w.catalog->exact_time->scale_ref.units_per_half_day ||
        &original.population.common != &w.common ||
        &original.population.economy != &w.local_economy ||
        &original.population.organizations != &w.organization ||
        &original.training.common != &w.common ||
        &original.training.organization != &w.organization ||
        &original.training.catalog != w.catalog->organization.get() ||
        &original.training.runtime != &*w.organization_runtime ||
        &original.training.economy != &w.local_economy)
      fail("PoliticalWorldScopeProductionBindingMismatch");
    same_at(at, p.clock.read_current_moment());
    production.emplace(p);
    population_change.emplace(ProductionPopulationChangeContext{
        w.common, w.local_economy, *w.catalog->local_economy, *production,
        input.production_inputs.adoptions(), original});
    write_lease = take(make_native_political_p1_write_lease(
        w, at,
        {read_inputs(), w.services, *population_change, input.staffing_effects,
         input.treaty_exact}));
    write_binding.emplace(write_lease->bindings());
    exports.political_write = &*write_binding;
  }
};
NativePoliticalWorldScope::NativePoliticalWorldScope(std::unique_ptr<Sources> s)
    : sources_(std::move(s)) {}
NativePoliticalWorldScope::~NativePoliticalWorldScope() = default;
Outcome<std::shared_ptr<const NativePoliticalWorldScope>>
NativePoliticalWorldScope::create_read(
    const WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    const WorldMoment &at, NativePoliticalWorldLateRead input) {
  try {
    auto sources = Sources::create(world, core, at, input);
    sources->bind_read();
    std::shared_ptr<NativePoliticalWorldScope> out(
        new NativePoliticalWorldScope(std::move(sources)),
        std::default_delete<NativePoliticalWorldScope>{},
        CheckpointAllocator<NativePoliticalWorldScope>{});
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("PoliticalWorldScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
Outcome<std::shared_ptr<NativePoliticalWorldScope>>
NativePoliticalWorldScope::create_write(
    WorldBundle &world, const NativeCompleteWorldCoreSources &core,
    const WorldMoment &at, NativePoliticalWorldLateWrite input) {
  try {
    auto sources = Sources::create(world, core, at, input.read);
    sources->bind_read();
    sources->bind_write(world, input);
    std::shared_ptr<NativePoliticalWorldScope> out(
        new NativePoliticalWorldScope(std::move(sources)),
        std::default_delete<NativePoliticalWorldScope>{},
        CheckpointAllocator<NativePoliticalWorldScope>{});
    return {true, std::move(out), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("PoliticalWorldScopeResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
// 未选必须同时来自原目录和模块；即使所有业务表为空，已选也不得进入此门。
Status validate_unselected_political_takeover_sources(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->organization || !w.organization_runtime)
    return common_failure("PoliticalTakeoverAbsenceRootsRequired");
  const auto &c = *w.catalog;
  if (c.political_takeover || c.task_takeover_execution_enabled ||
      c.occupation_fiscal || c.occupation_fiscal_settlement_enabled ||
      c.occupation_fiscal_needs_enabled || c.fiscal_taxpayer_continuity_enabled ||
      c.political_governance_continuations)
    return common_failure("PoliticalTakeoverActualUnselectedRequired");
  auto valid = validate_political_takeover_layout(
      false, c.identity.module_versions,
      w.political_takeover_seed_complete ? &w.political_takeover : nullptr,
      w.economic_takeover_seed_complete
          ? std::optional<Integer>{w.economic_takeover_execution.version}
          : std::nullopt,
      w.tasks.takeover_execution
          ? std::optional<Integer>{w.tasks.takeover_execution->version}
          : std::nullopt,
      1, 2);
  if (!valid.ok)
    return valid;
  valid = validate_political_takeover_command_history(w.common, false);
  if (!valid.ok)
    return valid;
  valid = validate_task_public_account_execution_layout(
      w.common, false, c.identity.module_versions,
      w.tasks.takeover_execution ? &*w.tasks.takeover_execution : nullptr);
  if (!valid.ok)
    return valid;
  // Task 可选段只要存在就拒，不能以其两表为空绕过物理段选择。
  if (w.political_takeover.version != 1 ||
      !w.political_takeover.registrations.empty() ||
      !w.political_takeover.approvals.empty() ||
      !w.political_takeover.decisions.empty() ||
      w.economic_takeover_execution.version != 1 ||
      !w.economic_takeover_execution.payments.empty() ||
      !w.economic_takeover_execution.rations.empty() ||
      !w.economic_takeover_execution.production.empty() ||
      !w.economic_takeover_execution.relief.empty() ||
      !w.economic_takeover_execution.configurations.empty() ||
      !w.economic_takeover_execution.configuration_requests.empty())
    return common_failure("PoliticalTakeoverUnselectedResidualState");
  // 这些既有后继均以前置接管为必要源，不能借关闭前置留下独立作用。
  valid = validate_occupation_fiscal_layout(
      false, c.identity.module_versions,
      w.occupation_fiscal_seed_complete ? &w.occupation_fiscal : nullptr,
      w.occupation_fiscal_settlement_seed_complete
          ? std::optional<Integer>{w.occupation_fiscal_settlement.version}
          : std::nullopt);
  if (!valid.ok)
    return valid;
  valid = validate_occupation_fiscal_command_history(w.common, false);
  if (!valid.ok)
    return valid;
  if (!w.occupation_fiscal.policies.empty() ||
      !w.occupation_fiscal_settlement.buckets.empty() ||
      !w.occupation_fiscal_settlement.legacy_carry_claims.empty() ||
      !w.occupation_fiscal_settlement.periods.empty() ||
      !w.occupation_fiscal_settlement.suspended_periods.empty() ||
      !w.occupation_fiscal_settlement.assessments.empty() ||
      !w.occupation_fiscal_settlement.payments.empty() ||
      w.fiscal_taxpayer_continuity_seed_complete ||
      !w.fiscal_taxpayer_continuity.suspended_periods.empty() ||
      w.political_governance_continuations)
    return common_failure("PoliticalTakeoverUnselectedDependentState");
  for (const auto &module : c.identity.module_versions)
    if (module.first == "occupation_fiscal_needs" ||
        module.first == fiscal_taxpayer_continuity_module ||
        module.first == political_governance_continuation_module)
      return common_failure("PoliticalTakeoverUnselectedDependentModule",
                            module.first);
  // 原 profile 已扫命令 history/results；这里再闭合全部保留 kind 在四种容器的残留。
  const auto reserved = [](const StableId &kind) {
    return kind == political_takeover_command_kind ||
           kind == political_takeover_pending_fact_kind ||
           kind == political_takeover_fact_kind ||
           kind == political_takeover_decision_fact_kind ||
           kind == task_contract_payment_fact_kind ||
           kind == task_static_payment_fact_kind ||
           kind == "production_configuration_deferred" ||
           kind == political_occupation_fiscal_command_kind ||
           kind == political_occupation_fiscal_fact_kind ||
           kind == occupation_fiscal_period_kind ||
           kind == occupation_fiscal_assessment_kind ||
           kind == occupation_fiscal_suspension_kind ||
           kind == occupation_fiscal_payment_kind ||
           kind == political_governance_continue_command ||
           kind == political_governance_continue_kind;
  };
  for (const auto &row : w.common.state().facts.rows())
    if (reserved(row.second.kind) ||
        row.second.domain == political_governance_continuation_module)
      return common_failure("PoliticalTakeoverUnselectedReservedFact", row.first);
  for (const auto &row : w.common.state().actions.rows())
    if (reserved(row.second.kind))
      return common_failure("PoliticalTakeoverUnselectedReservedAction", row.first);
  for (const auto &row : w.common.state().history)
    if (reserved(row.kind))
      return common_failure("PoliticalTakeoverUnselectedReservedHistory", row.result_id);
  for (const auto &row : w.common.state().results.rows())
    if (row.second && reserved(row.second->kind))
      return common_failure("PoliticalTakeoverUnselectedReservedResult", row.first);
  for (const auto &row : w.organization_runtime->authority_bases.rows()) {
    const auto &kind = row.second.issued_source.kind;
    if (kind == political_takeover_pending_fact_kind ||
        kind == political_takeover_fact_kind ||
        kind == political_takeover_decision_fact_kind ||
        kind == political_occupation_fiscal_fact_kind ||
        kind == political_governance_continue_kind)
      return common_failure("PoliticalTakeoverUnselectedAuthorityBasis",
                            row.first);
  }
  return common_success();
}
Status validate_native_political_world_business_sources(
    const WorldBundle &w, const PoliticalExactReadBinding &b) {
  try {
    if (!w.catalog || !w.catalog->common || !w.catalog->organization ||
        !w.catalog->politics || !w.organization_runtime ||
        &b.common_catalog != w.catalog->common.get() ||
        &b.politics_catalog != w.catalog->politics.get() ||
        &b.politics != &w.politics ||
        &b.organization.market.common != &w.common ||
        &b.organization.market.organization != &w.organization ||
        &b.organization.market.catalog != w.catalog->organization.get() ||
        &b.organization.market.economy != &w.local_economy ||
        &b.organization.runtime != &*w.organization_runtime)
      fail("PoliticalWorldBusinessBindingMismatch");
    const auto &c = *w.catalog;
    const auto now = take(w.actual_phase_moment());
    same_at(now, b.organization.clock.read_organization_authority_now());
    const bool takeover_selected =
        c.political_takeover && c.political_takeover->enabled;
    check(validate_political_takeover_layout(
        takeover_selected, c.identity.module_versions,
        w.political_takeover_seed_complete ? &w.political_takeover : nullptr,
        w.economic_takeover_seed_complete
            ? std::optional<Integer>{w.economic_takeover_execution.version}
            : std::nullopt,
        w.tasks.takeover_execution
            ? std::optional<Integer>{w.tasks.takeover_execution->version}
            : std::nullopt,
        c.political_takeover ? c.political_takeover->task_execution_version : 1,
        2));
    check(validate_political_takeover_command_history(w.common,
                                                      takeover_selected));
    if (takeover_selected != bool(b.takeover))
      fail("PoliticalWorldBusinessTakeoverSelectionMismatch");
    if (b.takeover) {
      const auto &t = *b.takeover;
      if (!c.military || t.catalog.version != 2 || t.state.version != 2 ||
          &t.common != &w.common || &t.common_catalog != c.common.get() ||
          &t.organizations != &w.organization ||
          &t.organization_catalog != c.organization.get() ||
          &t.politics_catalog != c.politics.get() ||
          &t.catalog != c.political_takeover.get() ||
          &t.state != &w.political_takeover || &t.military != &w.military ||
          &t.military_catalog != c.military.get() ||
          t.organization_runtime != &*w.organization_runtime ||
          t.politics_state != &w.politics ||
          t.actual_clock != &b.organization.clock ||
          t.military_access.common_catalog != c.common.get() ||
          t.military_access.travel_catalog != c.travel.get() ||
          t.military_access.travel_state != &w.travel ||
          t.supply_adoptions != (c.economic_supply_policies_enabled
                                     ? &w.political_supply_adoptions
                                     : nullptr) ||
          bool(t.supply_policies) != c.economic_supply_policies_enabled)
        fail("PoliticalWorldBusinessTakeoverBindingMismatch");
      if (t.phase)
        (void)take(political_takeover_evaluation_tick(w.common, t.phase));
      same_at(now, t.actual_clock->read_organization_authority_now());
      check(validate_political_takeover_state(t));
    } else {
      check(validate_unselected_political_takeover_sources(w));
    }
    const bool fiscal_selected =
        c.political_fiscal && c.political_fiscal->enabled;
    unsigned fiscal_modules = 0;
    for (const auto &module : c.identity.module_versions)
      if (module.first == "political_fiscal" &&
          (module.second != "2" || ++fiscal_modules != 1))
        fail("PoliticalWorldBusinessFiscalSelectionMismatch");
    if (fiscal_selected != (fiscal_modules == 1) ||
        fiscal_selected != w.political_fiscal_seed_complete ||
        fiscal_selected != bool(b.fiscal))
      fail("PoliticalWorldBusinessFiscalSelectionMismatch");
    if (b.fiscal) {
      const auto &f = *b.fiscal;
      const auto &a = f.ages;
      if (!w.actor_foundation || !c.actor_foundation || !c.family ||
          f.catalog.version != 2 || f.state.version != 2 ||
          &f.common != &w.common || &f.common_catalog != c.common.get() ||
          &f.politics != &w.politics || &f.state != &w.political_fiscal ||
          &f.catalog != c.political_fiscal.get() ||
          &f.organizations != &w.organization ||
          &f.organization_catalog != c.organization.get() ||
          &f.runtime != &*w.organization_runtime ||
          &f.clock != &b.organization.clock || f.takeover != b.takeover ||
          bool(f.occupation) != bool(b.takeover) || &a.common != &w.common ||
          &a.foundation != &*w.actor_foundation ||
          &a.foundation_catalog != c.actor_foundation.get() ||
          &a.family != &w.family || &a.births != &w.family_birth ||
          &a.control.common != &w.common || &a.control.family != &w.family ||
          &a.control.catalog != c.family.get() ||
          &a.control.shared != c.common.get())
        fail("PoliticalWorldBusinessFiscalBindingMismatch");
      same_at(now, f.clock.read_organization_authority_now());
      check(validate_political_fiscal2(f));
    } else {
      if (!w.political_fiscal.policies.empty())
        fail("PoliticalWorldScopeUnexpectedFiscalState");
      for (const auto &row : w.common.state().facts.rows())
        if (row.second.kind == "political_fiscal_policy_changed")
          fail("PoliticalWorldScopeUnexpectedFiscalState");
    }
    // 未选模块不能遗留唯一权源；空业务表不作为反向覆盖的替代证据。
    for (const auto &row : w.organization_runtime->authority_bases.rows()) {
      const auto &source = row.second.issued_source;
      if ((!fiscal_selected &&
           source.kind == "political_fiscal_policy_changed") ||
          (!takeover_selected &&
           (source.kind == political_takeover_pending_fact_kind ||
            source.kind == political_takeover_fact_kind ||
            source.kind == political_takeover_decision_fact_kind)))
        fail("PoliticalWorldBusinessUnselectedAuthorityBasis");
    }
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("PoliticalWorldScopeResourceExhausted", {},
                          ErrorKind::internal);
  }
}
const PoliticalExactReadLease &
NativePoliticalWorldScope::political_read_lease() const noexcept {
  return *sources_->forwarded_read;
}
const PoliticalTakeoverContext *
NativePoliticalWorldScope::takeover_context() const noexcept {
  return sources_->production_takeover ? &*sources_->production_takeover
                                       : nullptr;
}
const PoliticalFiscalTime2Context *
NativePoliticalWorldScope::fiscal_context() const noexcept {
  return sources_->read_fiscal ? &*sources_->read_fiscal : nullptr;
}
bool NativePoliticalWorldScope::bound_to(
    const WorldBundle &world) const noexcept {
  return &sources_->world == &world;
}
NativeCompleteWorldRoute NativePoliticalWorldScope::route() const noexcept {
  return NativeCompleteWorldRoute::political;
}
const WorldMoment &NativePoliticalWorldScope::actual_at() const noexcept {
  return sources_->at;
}
const NativeCompleteWorldSourceExports &
NativePoliticalWorldScope::sources() const noexcept {
  return sources_->exports;
}
} // namespace hanmo::resident
