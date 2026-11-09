#include "resident/kernel/catalog_section_observation.hpp"
#include "native_production_cycles.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "world_bundle.hpp"
#include <new>
#include <optional>
#include <stdexcept>
namespace hanmo::resident {
namespace {
// 身份准入和冷目录共享有限生产目录解码；无外层候选时保原独立解码行为。
Outcome<EconomicWorkCatalog>
decode_original_production_catalog(const BoundaryValue &input) {
  std::optional<ScopedCheckpointJsonDecode> scope;
  if (ScopedCheckpointBoundaryDecode::current())
    scope.emplace(CheckpointCatalogJsonKind::production_cycles);
  return decode_economic_work_catalog(input);
}
struct Failure {
  RuleError error;
};
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
[[noreturn]] void fail(const char *code, ErrorKind kind = ErrorKind::internal) {
  throw Failure{common_failure(code, {}, kind).error};
}
// 作者只明确选择原版本和原目录，未知字段/重复选择不参与推断。
const BoundaryValue *selection(const BoundaryValue &scenario) {
  const auto *dynamic = scenario.find("dynamic_world");
  if (!dynamic)
    return nullptr;
  const BoundaryValue *selected = nullptr;
  for (const auto &row : boundary::object(*dynamic, "scenario/dynamic_world"))
    if (row.first == "production_cycles") {
      if (selected)
        boundary::fail("DuplicateProductionCyclesSelection", row.first);
      selected = &row.second;
    }
  if (!selected)
    return nullptr;
  boundary::fields(*selected, {"version", "catalog"}, {}, "production_cycles");
  if (boundary::object(*selected, "production_cycles").size() != 2 ||
      boundary::get_int(*selected, "version", "production_cycles") != 3)
    boundary::unsupported("UnsupportedProductionCyclesVersion",
                          "production_cycles");
  auto exact = take(decode_exact_time_profile(scenario));
  if (!exact || !exact_complete_selected(*exact))
    fail("ProductionCyclesCompleteProfileRequired",
         ErrorKind::unsupported_capability);
  return &boundary::field(*selected, "catalog", "production_cycles");
}
void append(CatalogIdentity &identity, bool selected) {
  for (const auto &row : identity.module_versions)
    if (row.first == "production_cycles" ||
        (selected && row.first == "civilian_transport_population"))
      fail("DuplicateProductionCyclesModule");
  if (selected) {
    identity.module_versions.emplace_back("production_cycles", "3");
    // 原production/3消费P02/3劳力历史；两者是同一已选合同，不推断新玩法。
    identity.module_versions.emplace_back("civilian_transport_population", "3");
  }
}
// 本次真实时刻和各引用都须属于同一 WorldBundle，不能以等值外部世界替换。
void calendar_binding(const WorldBundle &world,
                      const EconomicCalendarReadContext &original) {
  if (!world.production_cycles || !world.actor_foundation ||
      &original.common != &world.common ||
      &original.economy != &world.local_economy ||
      &original.sources != &*world.production_cycles ||
      &original.foundation != &*world.actor_foundation ||
      &original.foundation_catalog != world.catalog->actor_foundation.get() ||
      &original.organization != &world.organization)
    fail("ProductionCyclesCandidateBindingMismatch");
  if (take(compare_world_moments(take(original.clock.read_current_moment()),
                                 take(world.actual_phase_moment()))) != 0)
    fail("ProductionCyclesActualMomentMismatch");
}
void read_binding(const WorldBundle &world,
                  const EconomicExactReadBinding &actual) {
  calendar_binding(world, actual.original.calendar);
  const auto &production = actual.original.production;
  if (&actual.catalog != world.catalog->local_economy.get() ||
      &production.catalog != world.catalog->production_cycles.get() ||
      &production.sources != &*world.production_cycles ||
      &production.clock != &actual.original.calendar.clock ||
      production.units_per_half_day !=
          world.catalog->exact_time->scale_ref.units_per_half_day)
    fail("ProductionCyclesReadBindingMismatch");
}
void write_binding(const WorldBundle &world,
                   const EconomicExactWriteBinding &actual) {
  calendar_binding(world, actual.calendar);
  if (&actual.common != &world.common ||
      &actual.economy != &world.local_economy ||
      &actual.catalog != world.catalog->local_economy.get() ||
      &actual.production.catalog != world.catalog->production_cycles.get() ||
      &actual.production.state != &*world.production_cycles ||
      &actual.production.clock != &actual.calendar.clock ||
      actual.production.units_per_half_day !=
          world.catalog->exact_time->scale_ref.units_per_half_day)
    fail("ProductionCyclesWriteBindingMismatch");
}
// 写 lease 同样保住外层真实 owner，返回后不留下只借已析构适配器的 context。
class ProductionWriteLease final : public EconomicExactWriteLease,
                                   public CheckpointAllocatedObject {
  std::unique_ptr<CompleteExactWorldBindingOwner> owner_;
  std::shared_ptr<EconomicExactWriteLease> original_;

public:
  ProductionWriteLease(std::unique_ptr<CompleteExactWorldBindingOwner> owner,
                       std::shared_ptr<EconomicExactWriteLease> original)
      : owner_(std::move(owner)), original_(std::move(original)) {}
  EconomicExactWriteBinding bindings() override {
    return original_->bindings();
  }
};
Outcome<std::shared_ptr<const EconomicExactReadLease>>
read_factory(const WorldBundle &world, const WorldMoment &at) {
  auto now = world.actual_phase_moment();
  if (!now.ok)
    return {false, {}, std::move(now.error)};
  auto same = compare_world_moments(now.value, at);
  if (!same.ok)
    return {false, {}, std::move(same.error)};
  if (same.value)
    return {false,
            {},
            common_failure("ProductionCyclesActualMomentMismatch", {},
                           ErrorKind::internal)
                .error};
  auto actual = NativeProductionCyclesRead::create(world);
  if (!actual.ok)
    return {false, {}, std::move(actual.error)};
  return {true, checkpoint_share(std::move(actual.value)), {}};
}
Outcome<std::shared_ptr<EconomicExactWriteLease>>
write_factory(WorldBundle &world, const WorldMoment &at,
              const DynamicStepInput *phase) {
  try {
    take(validate_native_production_cycles_layout(world));
    if (!world.production_cycles)
      fail("ProductionCyclesNotSelected", ErrorKind::unsupported_capability);
    if (take(compare_world_moments(take(world.actual_phase_moment()), at)))
      fail("ProductionCyclesActualMomentMismatch");
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner || !owner->reads().economics.read ||
        !owner->reads().economics.write)
      fail("ProductionCyclesActualFactoryRequired",
           ErrorKind::unsupported_capability);
    if (owner->reads().economics.read == read_factory ||
        owner->reads().economics.write == write_factory)
      fail("ProductionCyclesRecursiveFactory");
    auto actual = take(owner->reads().economics.write(world, at, phase));
    if (!actual)
      fail("ProductionCyclesWriteLeaseRequired",
           ErrorKind::unsupported_capability);
    write_binding(world, actual->bindings());
    return {true,
            checkpoint_share(std::unique_ptr<ProductionWriteLease>(
                new ProductionWriteLease(std::move(owner), std::move(actual)))),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("ProductionCyclesAllocationFailed", {},
                           ErrorKind::internal)
                .error};
  }
}
} // namespace
// 持久版本只来自实际锁身份；不存在的原段不会从当前 cycles 数量猜出。
Outcome<Integer>
selected_native_production_cycles_version(const CatalogIdentity &identity) {
  Integer version = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == "production_cycles") {
      if (version || row.second != "3")
        return {false, 0,
                common_failure("ProductionCyclesModuleMismatch", {},
                               ErrorKind::internal)
                    .error};
      version = 3;
    }
  return {true, version, {}};
}
// 保留原明示P02/2读源；P02/3仅作为已选production/3的必要依赖。
Outcome<Integer>
selected_native_population_transport_version(const CatalogIdentity &identity) {
  auto production = selected_native_production_cycles_version(identity);
  if (!production.ok)
    return {false, 0, std::move(production.error)};
  Integer version = 0;
  for (const auto &row : identity.module_versions)
    if (row.first == "civilian_transport_population") {
      if (version || (row.second != "2" && row.second != "3"))
        return {false, 0,
                common_failure("PopulationTransportModuleMismatch", row.first)
                    .error};
      version = row.second == "2" ? 2 : 3;
    }
  if ((production.value && version != 3) || (!production.value && version == 3))
    return {false, 0,
            common_failure("ProductionPopulationOriginalPairRequired").error};
  return {true, version, {}};
}
Status append_native_production_cycles_module(const BoundaryValue &scenario,
                                              CatalogIdentity &identity) {
  try {
    const auto *selected = selection(scenario);
    if (selected)
      take(decode_original_production_catalog(*selected));
    append(identity, selected != nullptr);
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status
validate_native_production_cycles_catalog(const PreparedCatalog &catalog) {
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 153, "PreparedCatalog", "validate_native_production_cycles_catalog", &catalog);

  try {
    const auto selected =
        take(selected_native_production_cycles_version(catalog.identity));
    take(selected_native_population_transport_version(catalog.identity));
    if (bool(selected) != bool(catalog.production_cycles))
      fail("ProductionCyclesCatalogSelectionMismatch");
    if (!selected)
      return common_success();
    if (!catalog.exact_time || !exact_complete_selected(*catalog.exact_time) ||
        !catalog.local_economy || !catalog.local_economy->enabled ||
        !catalog.actor_foundation || !catalog.organization)
      fail("ProductionCyclesOriginalSourcesRequired",
           ErrorKind::unsupported_capability);
    return validate_economic_work_catalog(*catalog.production_cycles,
                                          *catalog.local_economy);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Status prepare_native_production_cycles_catalog(const BoundaryValue &scenario,
                                                PreparedCatalog &catalog) {
  try {
    if (catalog.production_cycles)
      fail("ProductionCyclesCatalogAlreadyPrepared");
    const auto *selected = selection(scenario);
    if (bool(take(selected_native_production_cycles_version(
            catalog.identity))) != (selected != nullptr))
      fail("ProductionCyclesAuthorModuleMismatch");
    if (selected)
      catalog.production_cycles =
          checkpoint_make_shared<const EconomicWorkCatalog>(
              take(decode_original_production_catalog(*selected)));
    return validate_native_production_cycles_catalog(catalog);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
Status validate_native_production_cycles_layout(const WorldBundle &world) {
  try {
    if (!world.catalog)
      fail("ProductionCyclesCatalogRequired");
    take(validate_native_production_cycles_catalog(*world.catalog));
    if (bool(world.catalog->production_cycles) != bool(world.production_cycles))
      fail("ProductionCyclesSelectedStateMismatch");
    const auto population = take(
        selected_native_population_transport_version(world.catalog->identity));
    if (bool(population) != bool(world.population_transport) ||
        (world.population_transport &&
         world.population_transport->version != population))
      fail("PopulationTransportSelectedStateMismatch");
    if (world.production_cycles && !world.production_cycles->calendar_origin)
      fail("ProductionCyclesCalendarOriginRequired");
    return common_success();
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
bool is_native_production_cycle_fact(const FactRecord &fact) noexcept {
  return fact.kind == "production_cycle_started" ||
         fact.kind == "production_input_segment" ||
         fact.kind == "economic_period_opened";
}
Status
validate_native_production_cycle_fact(const EconomicWorkSourcesState &state,
                                      const FactRecord &fact) {
  if (fact.domain != "local_economy" || fact.id.empty())
    return common_failure("ProductionCyclesFactOwnerMismatch", fact.id,
                          ErrorKind::internal);
  if (fact.kind == "production_cycle_started") {
    const auto *row = state.cycles.find(fact.id);
    if (row && row->cycle_source_ref == fact.id)
      return common_success();
  } else if (fact.kind == "production_input_segment") {
    const auto *row = state.segments.find(fact.id);
    if (row && row->id == fact.id)
      return common_success();
  } else if (fact.kind == "economic_period_opened") {
    std::size_t count = 0;
    for (const auto &row : state.period_openings.rows())
      if (row.second.source_id == fact.id && row.first == row.second.period_id)
        ++count;
    if (count == 1)
      return common_success();
  }
  return common_failure("ProductionCyclesFactPayloadRequired", fact.id,
                        ErrorKind::internal);
}
NativeProductionCyclesRead::NativeProductionCyclesRead(
    std::unique_ptr<CompleteExactWorldBindingOwner> owner,
    std::shared_ptr<const EconomicExactReadLease> original)
    : owner_(std::move(owner)), original_(std::move(original)) {}
Outcome<std::unique_ptr<NativeProductionCyclesRead>>
NativeProductionCyclesRead::create(const WorldBundle &world) {
  try {
    take(validate_native_production_cycles_layout(world));
    if (!world.production_cycles)
      fail("ProductionCyclesNotSelected", ErrorKind::unsupported_capability);
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner || !owner->reads().economics.read ||
        !owner->reads().economics.write)
      fail("ProductionCyclesActualFactoryRequired",
           ErrorKind::unsupported_capability);
    if (owner->reads().economics.read == read_factory ||
        owner->reads().economics.write == write_factory)
      fail("ProductionCyclesRecursiveFactory");
    auto actual = take(owner->reads().economics.read(
        world, take(world.actual_phase_moment())));
    if (!actual)
      fail("ProductionCyclesReadLeaseRequired",
           ErrorKind::unsupported_capability);
    read_binding(world, actual->bindings());
    return {true,
            std::unique_ptr<NativeProductionCyclesRead>(
                new NativeProductionCyclesRead(std::move(owner),
                                               std::move(actual))),
            {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("ProductionCyclesAllocationFailed", {},
                           ErrorKind::internal)
                .error};
  }
}
// 仅首次实际安装生成 ScenarioOrigin 和原 cycle；restore 永不调用此 writer。
Status initialize_native_production_cycles_in_candidate(WorldBundle &world) {
  try {
    if (!world.catalog)
      fail("ProductionCyclesCatalogRequired");
    take(validate_native_production_cycles_catalog(*world.catalog));
    if (bool(world.catalog->production_cycles) != bool(world.production_cycles))
      fail("ProductionCyclesSelectedStateMismatch");
    if (!world.production_cycles)
      return common_success();
    const auto &state = *world.production_cycles;
    if (state.calendar_origin || !state.cycles.empty() ||
        !state.active_cycles_by_unit.empty() || !state.segments.empty() ||
        !state.labor_remainders.empty() || !state.settlements.empty() ||
        !state.period_openings.empty() || world.common.revision() != 0 ||
        !world.common.state().history.empty() ||
        !world.common.state().results.empty())
      fail("ProductionCyclesOriginalFreshStateRequired");
    // 首次原初始化也必须有同候选真实读票；phase先声明，晚于全部借用lease析构。
    const auto initial_at = take(world.actual_phase_moment());
    std::unique_ptr<PhaseCurrentReadScope> initial_phase;
    if (!world.has_active_read_phase())
      initial_phase = take(world.enter_phase_read_context(
          "production_initialization", "local_economy",
          {initial_at, initial_at}, initial_at));
    auto owner = take(world.complete_exact_time_bindings());
    if (!owner || !owner->reads().economics.read ||
        !owner->reads().economics.write)
      fail("ProductionCyclesActualFactoryRequired",
           ErrorKind::unsupported_capability);
    if (owner->reads().economics.read == read_factory ||
        owner->reads().economics.write == write_factory)
      fail("ProductionCyclesRecursiveFactory");
    auto actual = take(owner->reads().economics.write(
        world, take(world.actual_phase_moment()), nullptr));
    if (!actual)
      fail("ProductionCyclesWriteLeaseRequired",
           ErrorKind::unsupported_capability);
    auto binding = actual->bindings();
    write_binding(world, binding);
    struct WriteBoundary {
      WorldBundle &world;
      ~WriteBoundary() { world.note_write_boundary(); }
    } boundary{world};
    take(initialize_economic_exact_sources_in_candidate(binding));
    return validate_native_production_cycles_layout(world);
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  } catch (const std::bad_alloc &) {
    return common_failure("ProductionCyclesAllocationFailed", {},
                          ErrorKind::internal);
  }
}
Status validate_native_production_cycles_world(const WorldBundle &world) {
  try {
    take(validate_native_production_cycles_layout(world));
    if (!world.production_cycles)
      return common_success();
    // 导出和二进制冷验也须借真实当刻读票；已有合法phase不替换，自有票晚于全部lease析构。
    std::unique_ptr<PhaseCurrentReadScope> validation_phase;
    if (!world.has_active_read_phase() && world.read_stamps) {
      const auto current = take(world.actual_phase_moment());
      validation_phase = take(world.enter_phase_read_context(
          "production_validation", "local_economy", {current, current},
          current));
    }
    // 缺票根仍交原owner硬拒，不能在建立scope时解引用空根或制造替代来源。
    auto owner = take(NativeProductionCyclesRead::create(world));
    const auto actual = owner->bindings();
    take(validate_economic_production_source_history(
        world.common, world.local_economy, *world.catalog->local_economy,
        actual.original.production));
    return validate_economic_calendar_sources(
        actual.original.calendar, take(world.actual_phase_moment()));
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
EconomicExactWorldFactory native_production_cycles_factory() {
  return {read_factory, write_factory};
}
} // namespace hanmo::resident
