#include "political_p1_native_scope.hpp"
#include "resident/kernel/exact_complete_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
namespace {
class WorldClock final : public OrganizationAuthorityClock {
  const WorldBundle &world_;

public:
  explicit WorldClock(const WorldBundle &world) : world_(world) {}
  Outcome<WorldMoment> read_organization_authority_now() const override {
    return world_.actual_phase_moment();
  }
};
Status prerequisite_roots(const WorldBundle &w) {
  if (!w.catalog || !w.catalog->common || !w.catalog->organization ||
      !w.catalog->organization_lifecycle || !w.catalog->politics ||
      !w.catalog->local_economy || !w.organization_runtime ||
      !w.economic_obligation_sources || !w.read_stamps ||
      (w.household_purchases && !w.population_transport))
    return common_failure("PoliticalP1PrerequisiteWorldRootsRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
Status calendar_roots(const WorldBundle &w) {
  auto valid = prerequisite_roots(w);
  if (!valid.ok)
    return valid;
  if (!w.catalog->exact_time ||
      !exact_complete_selected(*w.catalog->exact_time) ||
      !w.catalog->complete_exact_time_runtime || !w.catalog->actor_foundation ||
      !w.actor_foundation || !w.world_timeline || !w.production_cycles ||
      !w.population_transport || !w.household_purchases ||
      !w.transport_registrations || !w.spatial_carriers || !w.movement_exact)
    return common_failure("PoliticalP1CalendarWorldRootsRequired", {},
                          ErrorKind::unsupported_capability);
  return common_success();
}
PublicMarketOrganizationReadContext
market_context(const WorldBundle &w, const OrganizationAuthorityClock &clock) {
  return {w.common,
          w.organization,
          *w.catalog->organization,
          *w.organization_runtime,
          w.catalog->identity,
          *w.household_purchases,
          w.local_economy,
          *w.population_transport,
          clock};
}
// 状态选择取原目录，不能以表空代替未选或给已选军籍装空来源。
Status population_binding(const WorldBundle &w,
                          const PopulationMilitaryReadContext *p) {
  if (bool(p) != w.population_military_enabled())
    return common_failure("PoliticalP1PopulationMilitarySourceRequired", {},
                          ErrorKind::unsupported_capability);
  if (!p)
    return common_success();
  if (!w.catalog->military || &p->common != &w.common ||
      &p->economy != &w.local_economy ||
      &p->economy_catalog != w.catalog->local_economy.get() ||
      &p->organization != &w.organization ||
      &p->organization_catalog != w.catalog->organization.get() ||
      &p->military != &w.military ||
      &p->military_catalog != w.catalog->military.get() ||
      &p->state != &w.population_military ||
      &p->catalog != w.catalog->population_military.get() ||
      !w.military_exact || p->military_exact != &*w.military_exact ||
      p->selected_version != 4 || !p->actual_clock ||
      !p->dynamic_land_sources ||
      p->population_transport != &*w.population_transport ||
      p->actual_integration != w.siege_integration_enabled())
    return common_failure("PoliticalP1PopulationMilitaryBindingMismatch");
  auto now = w.actual_phase_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto actual = p->actual_clock->read_current_moment();
  if (!actual.ok)
    return {false, {}, actual.error};
  auto same = compare_world_moments(now.value, actual.value);
  if (!same.ok)
    return {false, {}, same.error};
  return same.value == 0
             ? common_success()
             : common_failure("PoliticalP1PopulationMilitaryClockMismatch");
}
// 先持所有被借 context，再造前置来源与完整叶；固定对象也归同 cold job。
struct Sources final : CheckpointAllocatedObject {
  const WorldBundle &world;
  WorldMoment at;
  NativePoliticalP1ReadInputs input;
  WorldClock clock;
  OrganizationCurrentMomentAdapter current;
  PublicMarketOrganizationReadContext market;
  PopulationChangeTimeContext population;
  std::unique_ptr<NativePoliticalP1Prerequisites> prerequisites;
  Sources(const WorldBundle &w, const WorldMoment &m,
          NativePoliticalP1ReadInputs x)
      : world(w), at(m), input(x), clock(w), current(clock),
        market(market_context(w, clock)),
        population{w.common,
                   w.local_economy,
                   *w.population_transport,
                   w.organization,
                   *w.transport_registrations,
                   current,
                   input.population_military} {}
  NativePoliticalExactDependencies dependencies() const {
    const auto &w = world;
    return {{market, w.politics, *w.world_timeline, *w.production_cycles,
             *w.actor_foundation, *w.catalog->actor_foundation,
             w.catalog->complete_exact_time_runtime->action_times,
             input.action_time, input.ages},
            *w.catalog->common,
            *w.catalog->local_economy,
            *w.catalog->politics,
            *w.catalog->organization_lifecycle,
            prerequisites->lifecycle_sources(),
            prerequisites->relation_sources(),
            prerequisites->office_transitions(),
            input.deaths,
            prerequisites->authority_sources(),
            prerequisites->budgets(),
            input.original.stamp,
            *w.read_stamps,
            input.staffing,
            population,
            *w.spatial_carriers,
            *w.movement_exact,
            input.spatial,
            w.services,
            w.services,
            input.organization_resources.resource_history,
            input.organization_resources.prospective_grants,
            input.territories,
            input.treaty_exact,
            input.original.formations,
            input.original.returns,
            input.original.periods};
  }
};
Outcome<std::unique_ptr<Sources>> sources(const WorldBundle &world,
                                          const WorldMoment &at,
                                          NativePoliticalP1ReadInputs input) {
  auto valid = calendar_roots(world);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = population_binding(world, input.population_military);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto out = std::make_unique<Sources>(world, at, input);
  auto original =
      make_native_political_p1_prerequisites(world, at, input.original);
  if (!original.ok)
    return {false, {}, original.error};
  out->prerequisites = std::move(original.value);
  // 不接回外层工厂，不在未安装 body 上宣称完整原源闭环。
  valid = out->prerequisites->validate_selected_sources();
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
class ReadLease final : public PoliticalExactReadLease,
                        public CheckpointAllocatedObject {
  std::unique_ptr<Sources> sources_;
  NativePoliticalExactReadLease original_;

public:
  explicit ReadLease(std::unique_ptr<Sources> x)
      : sources_(std::move(x)),
        original_(sources_->world, sources_->at, sources_->dependencies()) {}
  Status validate_bindings() const { return original_.validate_bindings(); }
  PoliticalExactReadBinding bindings() const override {
    return original_.bindings();
  }
};
class WriteLease final : public PoliticalExactWriteLease,
                         public CheckpointAllocatedObject {
  std::unique_ptr<Sources> sources_;
  NativePoliticalExactWriteLease original_;

public:
  WriteLease(WorldBundle &w, std::unique_ptr<Sources> x,
             const NativePoliticalP1WriteInputs &input)
      : sources_(std::move(x)),
        original_(w, sources_->at,
                  {sources_->dependencies(), w.common, w.organization,
                   *w.organization_runtime, w.local_economy,
                   *w.population_transport, *w.household_purchases, w.politics,
                   input.membership_releases, input.staffing_effects,
                   input.production_population, input.treaty_exact}) {}
  Status validate_bindings() const { return original_.validate_bindings(); }
  PoliticalExactWriteBinding bindings() override {
    return original_.bindings();
  }
};
} // namespace
Outcome<std::unique_ptr<NativePoliticalP1Prerequisites>>
make_native_political_p1_prerequisites(
    const WorldBundle &w, const WorldMoment &at,
    const NativePoliticalP1SourceInputs &input) {
  auto valid = prerequisite_roots(w);
  if (!valid.ok)
    return {false, {}, valid.error};
  WorldClock clock(w);
  std::optional<PublicMarketOrganizationReadContext> market;
  if (w.household_purchases)
    market.emplace(market_context(w, clock));
  // 前置原 owner 会复制 context 并将时钟重绑自己的 WorldClock，不借此栈对象。
  return NativePoliticalP1Prerequisites::create(
      w, at,
      {{{w.common, *w.catalog->organization, *w.catalog->organization_lifecycle,
         *w.catalog->local_economy, w.local_economy,
         *w.economic_obligation_sources},
        input.installation,
        input.formations,
        input.returns,
        input.periods},
       {market ? &*market : nullptr, input.fiscal, input.takeover,
        input.governance, input.budgets},
       input.stamp});
}
Outcome<std::unique_ptr<PoliticalExactReadLease>>
make_native_political_p1_read_lease(const WorldBundle &w, const WorldMoment &at,
                                    NativePoliticalP1ReadInputs input) {
  auto owned = sources(w, at, input);
  if (!owned.ok)
    return {false, {}, owned.error};
  auto out = std::unique_ptr<ReadLease>(new ReadLease(std::move(owned.value)));
  auto valid = out->validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
Outcome<std::unique_ptr<PoliticalExactWriteLease>>
make_native_political_p1_write_lease(WorldBundle &w, const WorldMoment &at,
                                     NativePoliticalP1WriteInputs input) {
  if (bool(input.read.staffing) != bool(input.staffing_effects))
    return {false,
            {},
            common_failure("PoliticalP1StaffingWritePairRequired").error};
  auto owned = sources(w, at, input.read);
  if (!owned.ok)
    return {false, {}, owned.error};
  auto out = std::unique_ptr<WriteLease>(
      new WriteLease(w, std::move(owned.value), input));
  auto valid = out->validate_bindings();
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
