#include "native_land_motion_phase.hpp"
#include "movement_exact_detail.hpp"
#include "resident/domains/military/field_battle/field_deployment_command_model.hpp"
#include "resident/domains/military/field_battle/field_view_model.hpp"
#include "resident/kernel/source_owned_fields.hpp"
#include "resident/kernel/native_transport_catalog.hpp"
#include "resident/kernel/native_field_passage.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <limits>
#include <type_traits>

namespace hanmo::resident {
namespace {
using namespace movement_detail;
bool sea_kind(std::string_view kind) {
  // 不含原陆路 rescue；它的真实完成货损仍由原 Action 完成腿处理。
  return kind.starts_with("sea_") || kind.starts_with("naval_") ||
         kind == "environmental_sea" || kind.starts_with("vessel_") ||
         kind.starts_with("shore_assault_");
}
bool sea_domain(std::string_view domain) {
  return domain == "military_naval" || domain == "dynamic_sea_rescue" ||
         domain == "naval_carriers";
}
[[noreturn]] void sea_source(const StableId &id = {}) {
  fail("LandMotionHasOriginalSeaSources", id);
}
template <class> inline constexpr bool unsupported_source_type = false;
// 借同一有限拥有字段序逐项读真实来源，不编码、不复制、不跳未知载荷。
// 新持久型没有原字段访问时编译失败，不能悄悄遗漏新增 Sea 引用。
struct SourceScan {
  template <class... T> void operator()(const std::variant<T...> &v) const {
    std::visit(*this, v);
  }
  template <class T> void operator()(const std::optional<T> &v) const {
    if (v) (*this)(*v);
  }
  template <class A, class B> void operator()(const std::pair<A, B> &v) const {
    (*this)(v.first); (*this)(v.second);
  }
  void operator()(const std::shared_ptr<const CompatibilityFields> &v) const {
    // 原兼容叶只有 strict_original 枚举，没有自由 JSON 或业务引用。
    if (v && v->profile != CompatibilityFields::Profile::strict_original)
      fail("LandMotionUnknownCompatibilityProfile");
  }
  void operator()(const std::shared_ptr<const ReadStamp> &v) const {
    if (v) fail("LandMotionRetainedReadStampNotAllowed");
  }
  template <class T> void operator()(const T &v) const {
    if constexpr (std::is_same_v<T, SourceRef>) {
      if (sea_domain(v.domain) || sea_kind(v.kind)) sea_source(v.id);
    } else if constexpr (std::is_same_v<T, ReservationRef>) {
      if (sea_domain(v.domain_id) || sea_kind(v.activity_kind)) sea_source(v.id);
    } else if constexpr (std::is_same_v<T, FleetCarrierRef> ||
                         std::is_same_v<T, ShoreAssaultMovementExecution> ||
                         std::is_same_v<T, EnvironmentalSeaDeathCause4> ||
                         std::is_same_v<T, SeaEmbarkCareBindingRef> ||
                         std::is_same_v<T, VesselRef>) {
      sea_source();
    } else if constexpr (std::is_same_v<T, StableId> || std::is_arithmetic_v<T> ||
                         std::is_enum_v<T> || std::is_same_v<T, std::monostate> ||
                         std::is_same_v<T, CanonicalFraction> ||
                         std::is_same_v<T, WorldMoment> ||
                         std::is_same_v<T, WorkUnits> ||
                         std::is_same_v<T, HalfDayDuration>) {
      // 普通名字/数值不是 SourceRef；不按任意文本猜业务身份。
    } else {
      if constexpr (std::is_same_v<T, PopulationSelection>) {
        if (v.purpose == PopulationSelectionPurpose::sea_transfer) sea_source(v.id);
      } else if constexpr (std::is_same_v<T, LifeTerminationConditionBusinessKey>) {
        if (v.cause_tag == 13) sea_source(v.source_id);
      } else if constexpr (std::is_same_v<T, TerrainTravelParams>) {
        if (v.mode == "boat") sea_source(v.plan_id);
      } else if constexpr (std::is_same_v<T, MovementUnitState>) {
        if (v.naval_finalized_source) sea_source(v.id);
      } else if constexpr (std::is_same_v<T, SpatialWriterBinding>) {
        if (v.subject_kind == "fleet") sea_source(v.subject_id);
      } else if constexpr (std::is_same_v<T, ActionCoreView>) {
        if (sea_kind(v.kind)) sea_source(v.id);
      } else if constexpr (std::is_same_v<T, ActionElapsedState>) {
        if (sea_kind(v.execution_source_ref) || sea_kind(v.owner_cost_state_ref))
          sea_source(v.action_ref);
      } else if constexpr (std::is_same_v<T, ActionElapsedTerminalBinding>) {
        if (sea_kind(v.action_kind)) sea_source(v.action_ref);
      }
      if constexpr (requires { v.rows(); }) {
        for (const auto &row : v.rows()) (*this)(row);
      } else if constexpr (requires { v.begin(); v.end(); }) {
        for (const auto &row : v) (*this)(row);
      } else if constexpr (requires { source_owned_fields::visit(v, *this); }) {
        source_owned_fields::visit(v, *this);
      } else if constexpr (requires { source_owned_fields::visit_payload(v, *this); }) {
        source_owned_fields::visit_payload(v, *this);
      } else {
        static_assert(unsupported_source_type<T>, "LandMotionOriginalSourceVisitorRequired");
      }
    }
  }
};
void naval_empty(const NavalState &n) {
  if (!n.vessels.empty() || !n.fleets.empty() || !n.field_order.empty() ||
      !n.battle.battles.empty() || !n.battle.contacts.empty() ||
      !n.battle.plans.empty() || !n.battle.rounds.empty() ||
      !n.battle.captures.empty() || !n.battle.declines.empty() ||
      !n.battle.continuations.empty() || !n.battle.field_order.empty() ||
      !n.repairs.starts.empty() || !n.repairs.results.empty() ||
      !n.repairs.field_order.empty() || !n.repair_charges.charges.empty() ||
      !n.repair_charges.field_order.empty() || !n.navigation.voyages.empty() ||
      !n.navigation.steps.empty() || !n.navigation.field_order.empty() ||
      !n.gunnery.preparations.empty() || !n.gunnery.field_order.empty())
    sea_source("military_naval");
}
Status no_sea_calendar(WorldBundle &w, ExactStage stage,
                       const ExactPeriodContext &period,
                       ExactPhaseWriteContext &context) {
  return status([&] {
    if (stage != ExactStage::accident_due_loss || period.period_id.empty() ||
        !w.world_timeline ||
        w.world_timeline->boundary_state != PeriodBoundaryState::open ||
        w.world_timeline->period_id != period.period_id ||
        period.opening_at.half_day_tick != w.common.tick() ||
        period.opening_at.half_day_tick < 0 ||
        period.opening_at.half_day_tick == std::numeric_limits<Integer>::max() ||
        period.through_tick != period.opening_at.half_day_tick + 1 ||
        w.active_exact_write_context() != &context ||
        !w.common.actor_condition_capture_identity() ||
        take(compare_fractions(period.opening_at.sub_half_day, {})) != 0 ||
        take(compare_world_moments(take(w.actual_phase_moment()),
                                   WorldMoment{period.through_tick, {}})) != 0)
      fail("LandMotionActualAccidentBoundaryRequired");
    take(validate_native_land_motion_no_sea_sources(w));
  });
}
} // namespace

Status validate_native_land_motion_no_sea_sources(const WorldBundle &w) {
  return movement_detail::status([&] {
    using namespace movement_detail;
    if (!w.catalog || !w.catalog->travel || !w.movement_exact ||
        !w.spatial_carriers || !w.group_movement_executions ||
        !w.transport_registrations || !w.economic_transport_expenses ||
        !w.actor_foundation || !w.action_elapsed)
      fail("LandMotionActualWorldRootsRequired");
    const auto &identity = w.catalog->identity;
    const auto &common = w.common.state();
    if (common.metadata.content_id != identity.content_id ||
        common.metadata.content_version != identity.content_version ||
        common.metadata.content_digest != identity.content_digest ||
        common.metadata.ruleset_id != identity.ruleset_id)
      fail("LandMotionActualCatalogIdentityRequired");
    const auto selected = take(selected_native_transport_versions(identity));
    // 个人 travel4 原组合不变；显式 Field Army6 另借完整原 Group 图，仍拒 Sea。
    if (selected.travel != 4 ||
        (selected.group != 0 && (selected.group != 6 || !w.catalog->field_group_passage)))
      fail("LandMotionOriginalSelectionRequired");
    if (selected.group) take(validate_native_field_army_transport_roots(w));
    for (const auto &module : identity.module_versions)
      if (sea_domain(module.first) || module.first.starts_with("naval_"))
        fail("LandMotionSelectedSeaProviderRequired", module.first);
    if (w.catalog->naval || w.naval_seed_complete)
      fail("LandMotionSelectedSeaProviderRequired", "military_naval");
    take(validate_native_transport_layout(w));
    naval_empty(w.naval);
    for (const auto &[id, fact] : common.facts.rows())
      if (sea_domain(fact.domain) || sea_kind(fact.kind)) sea_source(id);
    for (const auto &[id, action] : common.actions.rows())
      if (sea_kind(action.kind)) sea_source(id);
    for (const auto &history : common.history)
      if (sea_kind(history.kind)) sea_source(history.result_id);
    for (const auto &[id, result] : common.results.rows()) {
      if (!result) continue;
      if (sea_kind(result->kind)) sea_source(id);
      if (result->details &&
          (std::holds_alternative<NavalCommandReceipt>(result->details->data) ||
           std::holds_alternative<NavalBattleCommandReceipt>(result->details->data) ||
           std::holds_alternative<NavalRepairReceipt>(result->details->data)))
        sea_source(id);
    }
    const SourceScan scan;
    for (const auto &[id, wallet] : common.wallets.rows())
      for (const auto &reserved : wallet.reserved) scan(reserved.action_id);
    for (const auto &[id, inventory] : common.inventories.rows())
      for (const auto &lot : inventory.lots) scan(lot.reserved_by);
    for (const auto &[id, journey] : w.travel.journey_facts.rows())
      if (journey.mode == "boat") sea_source(id);
    for (const auto &[id, action] : w.travel.actions.rows())
      if (action.mode == "boat") sea_source(id);
    scan(*w.movement_exact);
    scan(*w.spatial_carriers);
    scan(*w.group_movement_executions);
    scan(*w.transport_registrations);
    scan(*w.economic_transport_expenses);
    scan(*w.action_elapsed);
    if (w.population_transport) scan(*w.population_transport);
    for (const auto &[id, death] : w.family_life.deaths.rows()) {
      if (sea_kind(death.cause_kind)) sea_source(id);
      if (death.precise) scan(death.precise->cause);
    }
    for (const auto &[id, receipt] :
         w.actor_foundation->character_condition_extensions.applied_receipts.rows()) {
      scan(receipt.business_key);
      scan(receipt.cause);
    }
    // Naval/SEA 新拥有槽尚未存在，只有未选且上述真实根及交叉引用全空才可走此组合。
    // 新增该拥有槽时必须一起扩本反扫，不能继续用旧空接口代替实际 owner。
  });
}

ExactPhaseRegistration make_native_land_motion_phase_registration(
    MotionExactWorldFactory factory, ExactArithmeticBudget budget) {
  auto registration = make_motion_exact_phase_registration(factory, budget);
  registration.stages.push_back(ExactStage::accident_due_loss);
  registration.calendar_stages.push_back(ExactStage::accident_due_loss);
  const auto max_limbs = budget.max_limbs, max_steps = budget.max_steps;
  registration.validate_author_selection = [factory](const WorldBundle &w) {
    auto empty = validate_native_land_motion_no_sea_sources(w);
    if (!empty.ok) return empty;
    return validate_original_motion_phase_author_selection(w, factory);
  };
  registration.validate_world_sources = [factory, max_limbs, max_steps](
                                           const WorldBundle &w,
                                           const WorldMoment &at) {
    auto empty = validate_native_land_motion_no_sea_sources(w);
    if (!empty.ok) return empty;
    return validate_original_motion_phase_world_sources(
        w, at, factory, {max_limbs, max_steps, {}});
  };
  // 不产生 due、SourceRef、一次键或资源后果；原所有回调及 effect writer 键保持原样。
  registration.apply_calendar_stage = &no_sea_calendar;
  return registration;
}
} // namespace hanmo::resident
