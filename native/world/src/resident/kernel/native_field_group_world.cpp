#include "native_field_group_world.hpp"
#include "native_complete_world_sources.hpp"
#include "native_domain_read_versions.hpp"
#include "native_field_passage.hpp"
#include "native_field_world_scope.hpp"
#include "native_military_training_period.hpp"
#include "native_organization_runtime.hpp"
#include "resident/domains/local_economy/population_manifest_history.hpp"
#include "resident/domains/military/military_group_movement.hpp"
#include "resident/domains/military/military_manifest_history.hpp"
#include "resident/domains/politics/treaty/native_field_group_passage.hpp"
#include "resident/domains/travel/movement_encounter_integration.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "exact_complete_world.hpp"
#include <set>
#include <new>
#include <stdexcept>
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
void take(Status value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
}
[[noreturn]] void fail(const char *code) {
  throw Failure{
      common_failure(code, {}, ErrorKind::unsupported_capability).error};
}
template <class T, class F> Outcome<T> guarded(F &&function) {
  try {
    return {true, function(), {}};
  } catch (Failure &failure) {
    return {false, {}, std::move(failure.error)};
  } catch (const std::bad_alloc &) {
    return {false,
            {},
            common_failure("FieldGroupOriginalResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  } catch (const std::length_error &) {
    return {false,
            {},
            common_failure("FieldGroupOriginalResourceExhausted", {},
                           ErrorKind::internal)
                .error};
  }
}
// 真实选择决定未选商业腿；出现民用/物品扩展时拒绝，不用空 provider 代替。
void roots(const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
           const WorldBundle *candidate) {
  take(validate_native_field_army_transport_roots(w));
  const auto &early = core.characters.early;
  if (&early.world != &w || (candidate && candidate != &w) ||
      !early.formations || !early.returns || !early.periods ||
      !early.field_application_clock || !early.original_read_boundary ||
      !w.catalog->military || !w.catalog->organization ||
      !w.catalog->field_battle || !w.political_relations || !w.read_stamps ||
      !w.catalog->immutable_interaction_flags ||
      &core.characters.spatial.movement != &*w.movement_exact ||
      &core.characters.spatial.carriers != &*w.spatial_carriers ||
      &core.characters.spatial.registrations != &*w.transport_registrations)
    fail("FieldGroupOriginalSameWorldRequired");
  if (w.catalog->common->item_instances_enabled ||
      w.catalog->common->lot_properties_enabled || w.item_instances ||
      w.lot_properties)
    fail("FieldGroupSelectedInventoryItemOriginalRequired");
}
// 只从本候选 S0 已选择的正片窗和原 EconomicOpening 取
// phase，不能按整数时刻伪造。
std::optional<DynamicStepInput>
original_phase(const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
               const DynamicStepInput *supplied) {
  const auto actual = take(w.actual_phase_moment());
  const auto *writes = w.active_exact_write_context();
  const auto *span = writes ? writes->actual_advance_span() : nullptr;
  if (!span) {
    if (supplied || actual.half_day_tick > w.common.tick())
      fail("FieldActualAdvancePhaseRequired");
    return std::nullopt;
  }
  if (!w.has_active_read_phase() || !w.production_cycles || !w.world_timeline ||
      w.world_timeline->boundary_state != PeriodBoundaryState::open ||
      take(compare_world_moments(span->from, span->to)) >= 0 ||
      take(compare_world_moments(actual, span->from)) < 0 ||
      take(compare_world_moments(actual, span->to)) > 0)
    fail("FieldActualAdvancePhaseMismatch");
  const auto period = take(
      core.characters.early.periods->read_training_period(w.common.tick()));
  const auto *opening =
      w.production_cycles->period_openings.find(period.period_ref);
  if (!opening || w.world_timeline->period_id != period.period_ref ||
      opening->period_id != period.period_ref ||
      opening->source_id != period.original_opening.source_ref.id ||
      period.original_opening.source_schema != 1 ||
      take(compare_world_moments(opening->opening_at, period.opening_at)) ||
      opening->through_tick != period.to_tick ||
      take(compare_world_moments(span->from, period.opening_at)) < 0 ||
      take(compare_world_moments(span->to, WorldMoment{period.to_tick, {}})) >
          0)
    fail("FieldOriginalPeriodPhaseMismatch");
  // 这是原 S5 input 的同一冻结集合；不从今天 actions/activities 重建前像。
  DynamicStepInput input{period.from_tick,
                         period.to_tick,
                         opening->existing_activity_ids,
                         {},
                         opening->existing_action_ids};
  if (supplied &&
      (supplied->from_tick != input.from_tick ||
       supplied->to_tick != input.to_tick ||
       supplied->existing_activity_ids != input.existing_activity_ids ||
       supplied->existing_action_ids != input.existing_action_ids ||
       !supplied->due_message_ids.empty()))
    fail("FieldBorrowedOriginalPhaseMismatch");
  return input;
}
} // namespace

// 成员先后就是借用寿命；只有全部真实前序构好后才绑定 Field→Group 回指。
struct NativeFieldGroupWorld::Impl final : CheckpointAllocatedObject {
  const WorldBundle &world;
  const NativeCompleteWorldCoreSources &core;
  WorldBundle *candidate;
  WorldMoment at;
  NativeDomainReadVersionSource versions;
  std::unique_ptr<CurrentReadStampRead> stamps;
  NativeOrganizationAuthorityClock organization_clock;
  std::optional<DynamicStepInput> phase;
  std::unique_ptr<NativeFieldWorldScope> field;
  MilitaryFieldHistoryRead field_history;
  MilitarySelectedHistoryRead selected_history;
  NativeArmyGroupMovementSources army;
  NativeMilitaryTransportManifestHistoryRead military_manifest;
  NativeTransportInventoryEntryRead inventory;
  NativeEconomicTransportManifestHistoryRead manifests;
  NativeMilitaryMarchExpenseRead expenses;
  FieldGroupPassageContext passage_context;
  NativeFieldGroupPassage passage;
  GroupMovementContext group;

  Impl(const WorldBundle &w, const NativeCompleteWorldCoreSources &c,
       WorldBundle *actual_candidate, const DynamicStepInput *supplied_phase)
      : world(w), core(c), candidate(actual_candidate),
        at(take(w.actual_phase_moment())),
        versions(w, c.characters.early.formations, c.characters.early.returns,
                 c.characters.early.periods),
        stamps(w.make_current_read_stamp_reader(versions,
                                                {4096, 4096, 1024 * 1024})),
        organization_clock(w), phase(original_phase(w, c, supplied_phase)),
        field(take(NativeFieldWorldScope::create_from_original_core(
            w, c, phase ? &*phase : nullptr))),
        field_history{*w.field_battle,
                      *c.characters.early.field_application_clock},
        selected_history{w.catalog->identity,
                         w.military_exact ? &*w.military_exact : nullptr,
                         &field_history},
        army(w.common, w.military, *w.catalog->military, selected_history,
             *w.spatial_carriers, *w.movement_exact,
             *w.group_movement_executions, w.services, field->body_sources(),
             *stamps, *field->march_selection(),
             w.population_military_enabled() ? &w.population_military : nullptr,
             w.population_military_enabled()
                 ? w.catalog->population_military.get()
                 : nullptr,
             nullptr, c.characters.early.formations, c.characters.early.returns,
             c.characters.early.periods, field->finite_returns()),
        military_manifest(
            w.common, w.military, *w.catalog->military, selected_history,
            *w.spatial_carriers, *w.movement_exact,
            *w.group_movement_executions, field->body_sources(),
            c.characters.early.clock, *field->march_selection(),
            w.population_military_enabled() ? &w.population_military : nullptr,
            w.population_military_enabled()
                ? w.catalog->population_military.get()
                : nullptr,
            {}, c.characters.early.formations, c.characters.early.returns,
            c.characters.early.periods, field->finite_returns()),
        inventory(w.common, *w.spatial_carriers, *w.movement_exact,
                  *w.group_movement_executions, *w.transport_registrations,
                  *w.catalog->travel, &military_manifest),
        manifests(w.common, *w.catalog->common, *w.transport_registrations,
                  inventory, nullptr, military_manifest),
        expenses(w.common, w.military, *w.catalog->military,
                 *w.spatial_carriers, *w.movement_exact,
                 *w.group_movement_executions, field->body_sources(),
                 *w.catalog->travel, *stamps, *field->march_selection(),
                 c.characters.early.formations, field->finite_returns()),
        passage_context{w.common,
                        *w.political_passage,
                        *w.political_safe_exit,
                        *w.catalog->field_group_passage,
                        *w.catalog->organization,
                        *w.catalog->travel,
                        *w.spatial_carriers,
                        *w.movement_exact,
                        *w.group_movement_executions,
                        manifests,
                        *w.economic_transport_expenses,
                        w.military,
                        *c.characters.early.formations,
                        organization_clock,
                        c.characters.early.clock,
                        *c.characters.early.original_read_boundary,
                        *w.read_stamps,
                        versions,
                        w.has_active_read_phase() ? ReadMode::phase_current
                                                  : ReadMode::committed_current,
                        {},
                        actual_candidate ? &*actual_candidate->political_passage
                                         : nullptr},
        passage(passage_context), group{*w.catalog->travel,
                                        w.services,
                                        c.characters.spatial.original,
                                        army,
                                        std::nullopt,
                                        passage,
                                        manifests,
                                        *w.catalog->common,
                                        nullptr,
                                        &expenses,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        field->member_releases()} {
    take(field->bind_actual_group_context(group));
  }
};
NativeFieldGroupWorld::NativeFieldGroupWorld(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
NativeFieldGroupWorld::~NativeFieldGroupWorld() = default;
Outcome<std::unique_ptr<NativeFieldGroupWorld>> NativeFieldGroupWorld::create(
    const WorldBundle &w, const NativeCompleteWorldCoreSources &core,
    WorldBundle *candidate, const DynamicStepInput *phase) {
  return guarded<std::unique_ptr<NativeFieldGroupWorld>>([&] {
    roots(w, core, candidate);
    auto impl = std::unique_ptr<Impl>(new Impl(w, core, candidate, phase));
    auto out = std::unique_ptr<NativeFieldGroupWorld>(
        new NativeFieldGroupWorld(std::move(impl)));
    take(out->validate_bindings());
    return out;
  });
}
bool NativeFieldGroupWorld::bound_to(const WorldBundle &w) const noexcept {
  return &impl_->world == &w;
}
Status NativeFieldGroupWorld::validate_bindings() const {
  auto result = guarded<Done>([&] {
    const auto &x = *impl_;
    roots(x.world, x.core, x.candidate);
    if (take(
            compare_world_moments(take(x.world.actual_phase_moment()), x.at)) ||
        !x.field->bound_to(x.world) ||
        !x.inventory.bound_to(x.world.common, *x.world.spatial_carriers,
                              *x.world.movement_exact,
                              *x.world.group_movement_executions,
                              *x.world.transport_registrations) ||
        !x.manifests.bound_to(x.world.common, *x.world.transport_registrations))
      fail("FieldGroupOriginalBindingMismatch");
    return Done{};
  });
  return {result.ok, {}, std::move(result.error)};
}
Status NativeFieldGroupWorld::validate() const {
  auto result = guarded<Done>([&] {
    take(validate_bindings());
    const auto &x = *impl_;
    const auto &w = x.world;
    take(x.passage.validate_all());
    take(validate_spatial_carriers(w.common, *w.spatial_carriers,
                                   *w.movement_exact, x.group.carriers));
    // 尚无 Army 旅程也要反证供养只因本期 Opening；已有旅程由下面原完整群旅门一次调用。
    bool army_journey=false;
    for(const auto& row:w.spatial_carriers->journeys.rows())if(std::holds_alternative<ArmyCarrierRef>(row.second.carrier)){army_journey=true;break;}
    if(!army_journey)take(x.group.actual_sources.validate_military_supply_causes());
    take(validate_group_movement_execution(
        w.common, *w.spatial_carriers, *w.movement_exact,
        *w.group_movement_executions, *w.economic_transport_expenses, x.group));
    take(validate_transport_entry_manifest_sources(
        w.common, *w.spatial_carriers, *w.movement_exact,
        *w.group_movement_executions, x.manifests));
    // 旧 Encounter 与实际 Field 终局续行均按各自原源反读，不省略冷期后缀证明。
    take(validate_movement_encounter_continuations(
        w.common, *w.movement_exact, w.interaction, {},
        x.field->exact_contacts(), w.catalog->field_battle.get()));
    take(x.field->validate());
    return Done{};
  });
  return {result.ok, {}, std::move(result.error)};
}
const GroupMovementContext &NativeFieldGroupWorld::group() const noexcept {
  return impl_->group;
}
NativeFieldWorldScope &NativeFieldGroupWorld::field() const noexcept {
  return *impl_->field;
}
const CurrentReadStampRead &NativeFieldGroupWorld::stamps() const noexcept {
  return *impl_->stamps;
}
const NativeTransportInventoryEntryRead &
NativeFieldGroupWorld::inventory_entries() const noexcept {
  return impl_->inventory;
}
const TransportManifestHistorySourceRead &
NativeFieldGroupWorld::manifests() const noexcept {
  return impl_->manifests;
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
// 本 helper 自持的读域先于 owner，最后析构；借原阶段票时绝不提前退出。
std::unique_ptr<PhaseCurrentReadScope> supply_movement_phase(const WorldBundle& w,const WorldMoment& at){
 if(!w.read_stamps)fail("MilitarySupplyMovementReadPhaseRequired");
 if(w.has_active_read_phase())return {};
 return take(w.enter_phase_read_context("military_supply_movement","military",{at,at},at));
}
const NativeCompleteWorldCoreSources& supply_movement_core(const WorldBundle& w,
 const CompleteExactWorldBindingOwner& owner,const ExactPeriodContext& period){
 const auto& reads=owner.reads();const auto* core=reads.original_core;
 if(!core||&core->characters.early.world!=&w||&core->characters.early.complete!=&owner||
    !core->characters.early.periods||!w.group_movement_executions||!w.movement_exact||!w.spatial_carriers||
    take(compare_world_moments(take(w.actual_phase_moment()),period.opening_at)))
  fail("MilitarySupplyMovementSameWorldRequired");
 const auto original=take(core->characters.early.periods->read_training_period(period.opening_at.half_day_tick));
 if(original.period_ref!=period.period_id||original.to_tick!=period.through_tick||
    take(compare_world_moments(original.opening_at,period.opening_at)))
  fail("MilitarySupplyMovementOriginalOpeningRequired");
 return *core;
}
}
Outcome<std::vector<GroupMilitarySupplyRebindSource>> prepare_native_field_supply_movements(
 const WorldBundle& w,const ExactPeriodContext& period){
 return guarded<std::vector<GroupMilitarySupplyRebindSource>>([&]{
  if(!w.catalog)fail("MilitarySupplyMovementCatalogRequired");
  std::vector<GroupMilitarySupplyRebindSource> result;
  if(!w.catalog->field_group_passage)return result;
  auto phase=supply_movement_phase(w,period.opening_at);
  auto owner=take(w.complete_exact_time_bindings());if(!owner)fail("MilitarySupplyMovementOriginalOwnerRequired");
  const auto& core=supply_movement_core(w,*owner,period);
  auto group=take(NativeFieldGroupWorld::create(w,core));
  // 原 Opening 重入只读既有 batch；不可再次造 before 或再扣库存。
  for(const auto& supply:w.military.supply_facts)if(supply.tick==period.through_tick)return result;
  std::set<StableId> armies;
  for(const auto& item:w.spatial_carriers->journeys.rows()){
   const auto& journey=item.second;const auto* army=std::get_if<ArmyCarrierRef>(&journey.carrier);
   if(!army||(journey.state!=GroupJourneyLifecycle::travelling&&journey.state!=GroupJourneyLifecycle::paused)||w.spatial_carriers->terminals.find(journey.journey_id))continue;
   if(!armies.insert(army->army_id).second)fail("MilitarySupplyMultipleCurrentJourneys");
   result.push_back(take(prepare_group_military_supply_rebind(w.common,*w.spatial_carriers,
       *w.movement_exact,*w.group_movement_executions,group->group(),journey.journey_id,period.opening_at)));
  }
  return result;
 });
}
Status apply_native_field_supply_movements(WorldBundle& w,const ExactPeriodContext& period,
 const std::vector<GroupMilitarySupplyRebindSource>& prepared){
 const auto result=guarded<Done>([&]{
  if(!w.catalog||!w.active_exact_write_context())fail("MilitarySupplyMovementActualWriterRequired");
  if(!w.catalog->field_group_passage){if(!prepared.empty())fail("MilitarySupplyMovementUnselectedInput");return Done{};}
  auto phase=supply_movement_phase(w,period.opening_at);
  auto owner=take(w.complete_exact_time_bindings());if(!owner)fail("MilitarySupplyMovementOriginalOwnerRequired");
  const auto& core=supply_movement_core(w,*owner,period);
  auto group=take(NativeFieldGroupWorld::create(w,core,&w));
  for(const auto& before:prepared){
   const MilitarySupplyFact* actual=nullptr;
   for(const auto& supply:w.military.supply_facts)if(supply.tick==period.through_tick&&supply.army_id==before.army_id){if(actual)fail("MilitarySupplyMovementSupplyRepeated");actual=&supply;}
   if(!actual)fail("MilitarySupplyMovementActualSupplyRequired");
   take(apply_group_military_supply_rebind_in_candidate(w.common,*w.spatial_carriers,*w.movement_exact,
       *w.group_movement_executions,group->group(),before,*actual,period.opening_at));
  }
  // 重入及正常路径均反证每个 journey cause 恰有一次原重算，不以空 prepared 放过孤源。
  for(const auto& supply:w.military.supply_facts)if(supply.tick==period.through_tick){
   if(supply.cause_fact_ids.size()!=1)fail("MilitarySupplyActualCauseRequired");
   const auto* cause=w.common.state().facts.find(supply.cause_fact_ids.front());
   if(!cause)fail("MilitarySupplyActualCauseRequired");
   if(cause->domain=="travel"&&cause->kind=="group_journey"){
    const GroupMilitarySupplyRebindSource* actual=nullptr;
    for(const auto& row:w.group_movement_executions->military_supply_rebinds.rows())if(row.second.cause_fact_ids==std::vector<StableId>{supply.id}){if(actual)fail("MilitarySupplyRebindRepeated");actual=&row.second;}
    if(!actual||actual->journey_id!=cause->id)fail("MilitarySupplyRebindCoverageRequired");
    take(group->group().actual_sources.validate_military_supply_rebind(*actual));
   }
  }
  return Done{};
 });
 return {result.ok,{},std::move(result.error)};
}
} // namespace hanmo::resident
