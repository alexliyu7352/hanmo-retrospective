#include "actor_spatial_history.hpp"
#include "local_move_source.hpp"
#include "../../storage/common_store.hpp"
#include "../actor_foundation/created_origin.hpp"
#include "../military/military_spatial_handoff.hpp"
#include "../military/named_military_custody_history_model.hpp"
#include "../military/named_military_custody_placement.hpp"
#include "birth_placement.hpp"
#include "field_battle_exit_placement.hpp"
#include "group_member_loss_handoff.hpp"
#include "group_movement.hpp"
#include "military_action_placement.hpp"
#include "movement_control.hpp"
#include "movement_exact_detail.hpp"
#include "movement_result_read.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include <algorithm>
#include <set>
#include <string>
#include <type_traits>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
class PreparedActorSpatialInitialSource final {
  CheckpointMemoryCharge copied_fields_;
  SourceRef source_;
  static CheckpointMemoryCharge memory(const SourceRefBuildView &source) {


    return CheckpointMemoryCharge(0);
  }

public:
  explicit PreparedActorSpatialInitialSource(const SourceRefBuildView &source)
      : copied_fields_(memory(source)),
        source_{checkpoint_fresh_text_copy(source.domain),
                checkpoint_fresh_text_copy(source.kind),
                checkpoint_fresh_text_copy(source.id)} {}
  ~PreparedActorSpatialInitialSource() = default;
  PreparedActorSpatialInitialSource(const PreparedActorSpatialInitialSource &) =
      delete;
  PreparedActorSpatialInitialSource &
  operator=(const PreparedActorSpatialInitialSource &) = delete;
  PreparedActorSpatialInitialSource(PreparedActorSpatialInitialSource &&) =
      delete;
  PreparedActorSpatialInitialSource &
  operator=(PreparedActorSpatialInitialSource &&) = delete;
  const SourceRef &source() const noexcept { return source_; }
};
bool has_actor(const std::vector<StableId> &ids, const StableId &actor) {
  return std::find(ids.begin(), ids.end(), actor) != ids.end();
}
void head(const CommonStore &common, const SourceRef &ref) {
  const auto *f = common.state().facts.find(ref.id);
  if (!f || f->domain != ref.domain || f->kind != ref.kind)
    fail("ActorSpatialHistorySourceHeadRequired", ref.id);
}
bool overlap(const MomentSpan &a, const MomentSpan &z,
             ExactArithmeticBudget b) {
  return take(compare_world_moments(a.from, z.to, b)) < 0 &&
         take(compare_world_moments(z.from, a.to, b)) < 0;
}
bool unit_actor(const MovementExactState &movement,
                const MovementUnitState &unit, const StableId &actor) {
  bool found = false;
  for (const auto &ref : unit.effort_basis_refs) {
    const auto *basis = movement.exertion_bases.find(ref.id);
    if (ref.domain != "travel" || ref.kind != "travel_exertion_basis" ||
        !basis || basis->unit_ref != unit.id ||
        execution_key(basis->execution) != execution_key(unit.execution))
      fail("ActorSpatialHistoryBasisRequired", unit.id);
    if (has_actor(basis->actual_participants, actor))
      found = true;
  }
  return found;
}
MovementAnchor point(const MovementBasis &basis,
                     const CanonicalFraction &cursor, ExactArithmeticBudget b) {
  MovementPrefixState frozen;
  frozen.basis = basis;
  frozen.cursor = cursor;
  frozen.lifecycle = MovementActive{};
  return take(read_movement_position(frozen, b));
}
} // namespace
Outcome<Integer> actor_spatial_history_source_schema(const SourceRef &ref) {
  return guard<Integer>([&]() -> Integer {
    if (!source_valid(ref))
      fail("ActorSpatialHistorySourceRequired", ref.id);
    if (ref.domain == "actor_foundation" && ref.kind == "scenario_placement")
      return 1;
    if (ref.domain == "family" && ref.kind == "actor_creation")
      return 2;
    if (ref.domain == "travel" && ref.kind == "travel_dependent_binding_change")
      return 2;
    if (is_military_action_placement_source_kind(ref))
      return 2;
    if (is_named_military_custody_placement_source(ref))
      return 1;
    if (is_field_battle_exit_placement_source(ref) ||
        is_field_member_loss_placement_source(ref))
      return 1;
    if (ref.domain == "military" && ref.kind == "field_army_exit_ready")
      return 1; // 仅分类；真正位置必须由下方同World stable-ready provider枚举。
    if (ref.domain == "travel" && ref.kind == "group_contact_member_release")
      return 1;
    if (ref.domain == "travel")
      for (const auto *kind :
           {"local_move", "travel_exertion_basis", "terrain_journey", "road_action",
            "movement_unit_close", "movement_arrival", "movement_contact",
            "movement_unit_boundary", "movement_basis_transition",
            "movement_contact_continuation", "group_movement_opening",
            "group_movement_control", "personal_movement_control",
            "personal_movement_terminal", "transport_registration",
            "transport_release"})
        if (ref.kind == kind)
          return 1;
    throw Failure{common_failure("ActorSpatialHistorySourceKindUnsupported",
                                 ref.domain + "/" + ref.kind,
                                 ErrorKind::unsupported_capability)
                      .error};
  });
}
static Outcome<ActorStationaryLocationSpan> read_actor_history_impl(
    const CommonStore &common, const CommonCatalog &catalog,
    const MovementExactState &movement, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actor_catalog,
    const SpatialCarrierSources &sources, const StableId &actor,
    const SourceRef &accepted, const MomentSpan &span,
    const MilitarySpatialHandoffState *military, ExactArithmeticBudget b,
    const SourceRef *observation = nullptr,
    const FamilyCatalog *family_catalog = nullptr,
    const std::vector<ActualSpatialSourceRef> *frozen_sources = nullptr,
    std::vector<ActorSpatialHeadChange> *actual_events = nullptr,
    const ActorSpatialActualFactPrefix *actual_prefix = nullptr,
    const BirthPlacementRead *births = nullptr,
    const MilitaryActionPlacementRead *military_actions = nullptr,
    const NamedMilitaryCustodyPlacementRead *custody_releases = nullptr,
    const FieldBattleExitPlacementRead *field_exits = nullptr) {
  return guard<ActorStationaryLocationSpan>([&] {
    const bool point_read = observation || frozen_sources || actual_prefix;
    // 歷史點讀保原來源切口及同刻比較；只有正服務區間才要求非零時長。
    if (!point_read)
      take(make_moment_span(span.from, span.to, b));
    if ((point_read
             ? take(compare_world_moments(span.from, span.to, b)) != 0
             : take(compare_world_moments(span.from, span.to, b)) >= 0) ||
        take(compare_world_moments(span.to, take(sources.current_moment()),
                                   b)) > 0 ||
        !source_valid(accepted) || !common.actor(actor))
      fail("ActorStationarySpanRequired", actor);
    std::size_t observation_order = 0, frozen_order = 0;
    auto fact_order = [&](const SourceRef &ref) {
      head(common, ref);
      const auto &ordered = common.state().facts.order();
      auto found = std::find(ordered.begin(), ordered.end(), ref.id);
      if (found == ordered.end())
        fail("ActorSpatialHistorySourceOrderRequired", ref.id);
      return std::size_t(found - ordered.begin());
    };
    if (observation) {
      if (!source_valid(*observation))
        fail("ActorSpatialObservationSourceRequired", actor);
      observation_order = fact_order(*observation);
    }
    if (actual_prefix) {
      if (observation || frozen_sources)
        fail("ActorSpatialActualFactPrefixMismatch", actor);
      auto prefix = read_actual_common_fact_prefix(common, *actual_prefix);
      if (!prefix.ok) {
        const auto &error = prefix.error.original;
        if (error.code == "CommonFactPrefixSourceHeadRequired")
          fail("ActorSpatialHistorySourceHeadRequired", error.path);
        if (error.code == "CommonFactPrefixSourceOrderRequired")
          fail("ActorSpatialHistorySourceOrderRequired", error.path);
        if (error.code == "CommonActualFactPrefixMismatch")
          fail("ActorSpatialActualFactPrefixMismatch",
               error.path.empty() ? actor : error.path);
        throw Failure{std::move(prefix.error)};
      }
      observation_order = prefix.value;
    }
    auto visible = [&](const WorldMoment &at, const SourceRef &ref) {
      const auto order = take(compare_world_moments(at, span.from, b));
      if (order > 0)
        return false;
      if (!point_read)
        return true;
      if (ref.domain == "actor_foundation" && ref.kind == "scenario_placement")
        return true;
      const auto ordinal = fact_order(ref) + 1;
      const bool before = frozen_sources ? ordinal <= frozen_order
                                         : ordinal <= observation_order;
      if (order < 0 && !before)
        fail("ActorSpatialObservationCausalOrderMismatch", ref.id);
      return before;
    };
    take(validate_movement_exact_values(movement, b));
    take(validate_personal_movement_controls(common, movement, b));
    std::optional<MovementAnchor> latest;
    WorldMoment latest_at;
    std::size_t latest_event_order = 0;
    SourceRef latest_source, latest_causal_source;
    bool accepted_found = false;
    // 原 Field owner 先全验真实双/单入口和实际 S1 成员交接；这里只消费借行。
    // 两个返回均只活于这次 read job，不把临时视图保存到世界或用今日位置倒证。
    struct FieldBoundary {
      const FactRecord *entered = nullptr;
      const WorldMoment *entered_at = nullptr;
      const FactRecord *released = nullptr;
      const WorldMoment *released_at = nullptr;
      const GroupContactMemberReleaseSource *release = nullptr;
      const GroupMemberLossHandoffSource *loss = nullptr;
      const GroupMovementControlSource *stable_return = nullptr;
    };
    if (field_exits && !field_exits->bound_to(common, movement))
      fail("CrossCandidateFieldSpatialHistoryRead", actor);
    auto field_engagements = [&]() {
      return field_exits
                 ? take(field_exits->read_field_actor_engagement_history(actor))
                 : CheckpointVector<FieldActorEngagementRead>{};
    }();
    auto field_placements = [&]() {
      return field_exits
                 ? take(field_exits->read_field_exit_placement_history(actor))
                 : CheckpointVector<ActualActorSpatialWriteRead>{};
    }();
    auto field_losses = [&]() {
      return field_exits
                 ? take(field_exits->read_field_member_loss_placement_history(
                       actor))
                 : CheckpointVector<FieldActorMemberLossPlacementRead>{};
    }();
    auto field_stable_returns = [&]() {
      return field_exits
          ? take(field_exits->read_field_army_exit_stable_placement_history(actor))
          : CheckpointVector<ActualActorSpatialWriteRead>{};
    }();
    CheckpointVector<FieldBoundary> field_boundaries;
    if (field_exits) {
      for (const auto &engagement : field_engagements) {
        if (!engagement.battle_id || engagement.battle_id->empty() ||
            !engagement.journey_id || engagement.journey_id->empty())
          fail("FieldSpatialEngagementIdentityRequired", actor);
        FieldBoundary boundary;
        std::visit(
            [&](const auto &entry) {
              using T = std::decay_t<decltype(entry)>;
              if constexpr (std::is_same_v<T, FieldActorDoubleContactEntry>) {
                if (!entry.installation || !entry.contact ||
                    movement.contact_installations.find(
                        entry.installation->id) != entry.installation ||
                    movement.contacts.find(entry.contact->id) !=
                        entry.contact ||
                    entry.installation->contact_ref != entry.contact->id ||
                    entry.installation->business_source_schema != 1)
                  fail("FieldSpatialDoubleEntryRequired", actor);
                const auto *actual =
                    common.state().facts.find(entry.installation->id);
                if (!actual || actual->domain != "travel" ||
                    actual->kind != "movement_contact_installation")
                  fail("FieldSpatialDoubleEntryHeadRequired", actor);
                take(read_actual_common_fact_position(common, *actual));
                boundary.entered = actual;
                boundary.entered_at = &entry.installation->occurred_at;
              } else if constexpr (std::is_same_v<
                                       T, FieldActorSingleBoundaryEntry>) {
                if (!entry.contact || !entry.field_contact_head ||
                    !entry.entered_head || !entry.entered_at ||
                    movement.boundary_contacts.find(entry.contact->id) !=
                        entry.contact ||
                    common.state().facts.find(entry.field_contact_head->id) !=
                        entry.field_contact_head ||
                    common.state().facts.find(entry.entered_head->id) !=
                        entry.entered_head ||
                    entry.field_contact_head->domain != "military" ||
                    entry.field_contact_head->kind != "field_battle_contact" ||
                    entry.entered_head->domain != "military" ||
                    entry.entered_head->kind != "field_battle_entered")
                  fail("FieldSpatialSingleEntryRequired", actor);
                const auto before = take(read_actual_common_fact_position(
                    common, *entry.field_contact_head));
                const auto entered = take(read_actual_common_fact_position(
                    common, *entry.entered_head));
                if (before >= entered ||
                    take(compare_world_moments(entry.contact->occurred_at,
                                               *entry.entered_at, b)) > 0)
                  fail("FieldSpatialSingleEntryOrderMismatch", actor);
                boundary.entered = entry.entered_head;
                boundary.entered_at = entry.entered_at;
              }
            },
            engagement.entry);
        if (!boundary.entered || !boundary.entered_at ||
            take(compare_world_moments(*boundary.entered_at,
                                       take(sources.current_moment()), b)) > 0)
          fail("FieldSpatialEntryMomentRequired", actor);
        if (const auto *ordinary =
                std::get_if<const GroupContactMemberReleaseSource *>(
                    &engagement.release)) {
          if (!*ordinary)
            fail("FieldSpatialReleaseOriginalRowRequired", actor);
          const auto &release = **ordinary;
          if (release.journey_id != *engagement.journey_id ||
              release.release_schema != 1 ||
              std::count(release.named_participants.begin(),
                         release.named_participants.end(), actor) != 1 ||
              !is_field_battle_exit_placement_source(
                  release.actual_release_source))
            fail("FieldSpatialReleaseMemberMismatch", actor);
          const auto *actual = common.state().facts.find(release.id);
          if (!actual || actual->domain != "travel" ||
              actual->kind != "group_contact_member_release")
            fail("FieldSpatialReleaseHeadRequired", actor);
          const auto entered =
              take(read_actual_common_fact_position(common, *boundary.entered));
          const auto exit = take(read_actual_common_fact_position(
              common, release.actual_release_source));
          const auto handed =
              take(read_actual_common_fact_position(common, *actual));
          if (entered >= exit || exit >= handed ||
              take(compare_world_moments(*boundary.entered_at,
                                         release.occurred_at, b)) > 0 ||
              take(compare_world_moments(
                  release.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialReleaseOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &release.occurred_at;
          boundary.release = &release;
        } else if (const auto *lost =
                       std::get_if<const GroupMemberLossHandoffSource *>(
                           &engagement.release)) {
          if (!*lost)
            fail("FieldSpatialLossOriginalRowRequired", actor);
          const auto &loss = **lost;
          if (loss.journey_id != *engagement.journey_id ||
              loss.actor_id != actor)
            fail("FieldSpatialLossMemberMismatch", actor);
          const auto *actual = common.state().facts.find(loss.id);
          if (!actual || actual->domain != "travel" ||
              actual->kind != "group_member_loss_handoff")
            fail("FieldSpatialLossHeadRequired", actor);
          const auto entered =
              take(read_actual_common_fact_position(common, *boundary.entered));
          const auto casualty = take(
              read_actual_common_fact_position(common, loss.actual_casualty));
          const auto handed =
              take(read_actual_common_fact_position(common, *actual));
          if (entered >= casualty || casualty >= handed ||
              take(compare_world_moments(*boundary.entered_at, loss.occurred_at,
                                         b)) > 0 ||
              take(compare_world_moments(
                  loss.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialLossOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &loss.occurred_at;
          boundary.loss = &loss;
        } else if (const auto *continued =
                       std::get_if<const MovementContactContinuationSource *>(
                           &engagement.release)) {
          // 完整 Field reader 已证明本场胜方、终局段及下一真实 Opening。
          // S1 仍逐原表/原单位/实际成员反核；仅关闭参战区间，不发静置落点。
          if (!*continued || movement.contact_continuations.find(
                                 (*continued)->id) != *continued)
            fail("FieldSpatialContinuationOriginalRowRequired", actor);
          const auto &continuation = **continued;
          const auto *entry =
              std::get_if<FieldActorDoubleContactEntry>(&engagement.entry);
          const auto *old = movement.units.find(continuation.closed_unit_ref);
          const auto *next = movement.units.find(continuation.new_unit_ref);
          const auto *group =
              next ? std::get_if<GroupMovementExecution>(&next->execution)
                   : nullptr;
          const auto *old_group =
              old ? std::get_if<GroupMovementExecution>(&old->execution)
                  : nullptr;
          if (!entry || !entry->contact ||
              entry->contact->id != continuation.contact_ref || !old || !next ||
              !group || !old_group ||
              group->journey_id != *engagement.journey_id ||
              old_group->journey_id != group->journey_id ||
              execution_key(old->execution) != execution_key(next->execution) ||
              continuation.held || continuation.resolution_schema != 1 ||
              continuation.actual_resolution_source.domain != "military" ||
              continuation.actual_resolution_source.kind !=
                  "field_battle_segment" ||
              !unit_actor(movement, *old, actor) ||
              !unit_actor(movement, *next, actor))
            fail("FieldSpatialContinuationMemberMismatch", actor);
          const auto *actual = common.state().facts.find(continuation.id);
          if (!actual || actual->domain != "travel" ||
              actual->kind != "movement_contact_continuation")
            fail("FieldSpatialContinuationHeadRequired", actor);
          const auto entered =
              take(read_actual_common_fact_position(common, *boundary.entered));
          const auto terminal = take(read_actual_common_fact_position(
              common, continuation.actual_resolution_source));
          const auto resumed =
              take(read_actual_common_fact_position(common, *actual));
          if (entered >= terminal || terminal >= resumed ||
              take(compare_world_moments(*boundary.entered_at,
                                         continuation.occurred_at, b)) > 0 ||
              take(compare_world_moments(continuation.occurred_at,
                                         take(sources.current_moment()), b)) >
                  0)
            fail("FieldSpatialContinuationOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &continuation.occurred_at;
        } else if (const auto *continued =
                       std::get_if<const MovementBoundaryContinuationSource *>(
                           &engagement.release)) {
          // 已入场边界赢家的真实单prefix续程，只关闭参战区间，不造静置落点。
          if (!*continued || movement.boundary_continuations.find((*continued)->id) != *continued)
            fail("FieldSpatialBoundaryContinuationOriginalRowRequired", actor);
          const auto &continuation = **continued;
          const auto *entry = std::get_if<FieldActorSingleBoundaryEntry>(&engagement.entry);
          const auto *old = movement.units.find(continuation.closed_unit_ref);
          const auto *next = movement.units.find(continuation.new_unit_ref);
          const auto *group = next ? std::get_if<GroupMovementExecution>(&next->execution) : nullptr;
          const auto *old_group = old ? std::get_if<GroupMovementExecution>(&old->execution) : nullptr;
          if (!entry || !entry->contact || !entry->field_contact_head ||
              !same_source(continuation.boundary_contact,
                           {"travel", "movement_boundary_contact", entry->contact->id}) ||
              !same_source(continuation.field_contact,
                           {"military", "field_battle_contact", entry->field_contact_head->id}) ||
              continuation.prefix_id != entry->contact->prefix_id || !old || !next ||
              !group || !old_group || group->journey_id != *engagement.journey_id ||
              old_group->journey_id != group->journey_id ||
              execution_key(old->execution) != execution_key(next->execution) ||
              continuation.held || continuation.terminal_segment.domain != "military" ||
              continuation.terminal_segment.kind != "field_battle_segment" ||
              !unit_actor(movement, *old, actor) || !unit_actor(movement, *next, actor))
            fail("FieldSpatialBoundaryContinuationMemberMismatch", actor);
          const auto *actual = common.state().facts.find(continuation.id);
          if (!actual || actual->domain != "travel" || actual->kind != "movement_boundary_continuation")
            fail("FieldSpatialBoundaryContinuationHeadRequired", actor);
          const auto entered = take(read_actual_common_fact_position(common, *boundary.entered));
          const auto terminal = take(read_actual_common_fact_position(common, continuation.terminal_segment));
          const auto resumed = take(read_actual_common_fact_position(common, *actual));
          if (entered >= terminal || terminal >= resumed ||
              take(compare_world_moments(*boundary.entered_at, continuation.occurred_at, b)) > 0 ||
              take(compare_world_moments(continuation.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialBoundaryContinuationOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &continuation.occurred_at;
        } else if (const auto *stable =
                       std::get_if<const GroupMovementControlSource *>(&engagement.release)) {
          // 只有完整Field原owner证明的同目标ready取消才可作终点；cancel锚仍是旧contact。
          if (!*stable) fail("FieldSpatialStableReturnOriginalRowRequired", actor);
          const auto &control = **stable;
          const auto *actual = common.state().facts.find(control.id);
          if (control.journey_id != *engagement.journey_id ||
              control.operation != GroupControlOperation::cancel ||
              control.authority_source.domain != "military" || control.authority_source.kind != "field_army_exit_ready" ||
              !actual || actual->id != control.id || actual->domain != "travel" || actual->kind != "group_movement_control")
            fail("FieldSpatialStableReturnOriginalSourceRequired", actor);
          const ActualActorSpatialWriteRead *point = nullptr;
          for (const auto &value : field_stable_returns)
            if (value.immutable_source_head.size() == 2 &&
                value.immutable_source_head[0].source.id == control.id) {
              if (point) fail("FieldSpatialStableReturnPlacementAmbiguous", actor);
              point = &value;
            }
          if (!point || !same_source(point->immutable_source_head[1].source, control.authority_source) ||
              take(compare_world_moments(point->occurred_at, control.occurred_at, b)) != 0)
            fail("FieldSpatialStableReturnPlacementRequired", actor);
          const auto entered = take(read_actual_common_fact_position(common, *boundary.entered));
          const auto ready = take(read_actual_common_fact_position(common, control.authority_source));
          const auto adopted = take(read_actual_common_fact_position(common, *actual));
          if (entered >= ready || ready >= adopted ||
              take(compare_world_moments(*boundary.entered_at, control.occurred_at, b)) > 0 ||
              take(compare_world_moments(control.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialStableReturnOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &control.occurred_at;
          boundary.stable_return = &control;
        } else if (const auto *returned =
                       std::get_if<const GroupMovementOpeningSource *>(
                           &engagement.release)) {
          // Field原reader已证明ready、旧contact取消与真实新opening同源；这里只关闭
          // 仍held成员的旧参战区间，新行程继续原Army carrier，绝不生成静置placement。
          if (!*returned) fail("FieldSpatialFiniteReturnOriginalRowRequired", actor);
          const auto &opening = **returned;
          const auto *order = opening.military_order ? &*opening.military_order : nullptr;
          const auto *old = carriers.journeys.find(*engagement.journey_id);
          const auto *next = carriers.journeys.find(opening.journey_id);
          const auto *army = std::get_if<ArmyCarrierRef>(&opening.carrier_origin);
          const auto *old_army = old ? std::get_if<ArmyCarrierRef>(&old->carrier) : nullptr;
          const auto *new_army = next ? std::get_if<ArmyCarrierRef>(&next->carrier) : nullptr;
          if (!order || order->kind != MilitaryMarchOrderKind::finite_return ||
              !order->original_return_cause || order->original_return_cause->domain != "military" ||
              order->original_return_cause->kind != "field_army_exit_ready" ||
              !old || !next || old->journey_id == next->journey_id ||
              !army || !old_army || !new_army || army->army_id != order->army_id ||
              old_army->army_id != army->army_id || new_army->army_id != army->army_id ||
              std::count(opening.named_participants.begin(), opening.named_participants.end(), actor) != 1)
            fail("FieldSpatialFiniteReturnMemberMismatch", actor);
          const MovementUnitState *first = nullptr;
          for (const auto &row : movement.units.rows()) {
            const auto *group = std::get_if<GroupMovementExecution>(&row.second.execution);
            if (!group || group->journey_id != opening.journey_id || row.second.ordinal != 0) continue;
            if (first || row.first != row.second.id ||
                spatial_carrier_key(group->carrier) != spatial_carrier_key(next->carrier))
              fail("FieldSpatialFiniteReturnFirstUnitRequired", actor);
            first = &row.second;
          }
          if (!first || !unit_actor(movement, *first, actor))
            fail("FieldSpatialFiniteReturnOriginalMemberRequired", actor);
          const auto *actual = common.state().facts.find(opening.id);
          if (!actual || actual->id != opening.id || actual->domain != "travel" ||
              actual->kind != "group_movement_opening")
            fail("FieldSpatialFiniteReturnHeadRequired", actor);
          const auto entered = take(read_actual_common_fact_position(common, *boundary.entered));
          const auto ready = take(read_actual_common_fact_position(common, *order->original_return_cause));
          const auto adopted = take(read_actual_common_fact_position(common, *actual));
          if (entered >= ready || ready >= adopted ||
              take(compare_world_moments(*boundary.entered_at, opening.occurred_at, b)) > 0 ||
              take(compare_world_moments(opening.occurred_at, order->accepted_at, b)) != 0 ||
              take(compare_world_moments(opening.occurred_at, next->started_at, b)) != 0 ||
              take(compare_world_moments(opening.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialFiniteReturnOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &opening.occurred_at;
        } else if (const auto *arrived =
                       std::get_if<const MovementArrivalSource *>(
                           &engagement.release)) {
          // 原道路端点没有正余程时，只借实际到达头结束参战。
          // 不造零长度单位、不把成员卸载为静置；Army 的真实到场仍沿原源。
          if (!*arrived || movement.arrivals.find((*arrived)->id) != *arrived)
            fail("FieldSpatialArrivalOriginalRowRequired", actor);
          const auto &arrival = **arrived;
          const SourceRef source{"travel", "movement_arrival", arrival.id};
          take(read_verified_movement_arrival(movement, source, 1, b));
          const auto *entry =
              std::get_if<FieldActorDoubleContactEntry>(&engagement.entry);
          const auto *unit = movement.units.find(arrival.unit_ref);
          const auto *close =
              unit ? movement.unit_closes.find(unit->id + ":close") : nullptr;
          const auto *group =
              unit ? std::get_if<GroupMovementExecution>(&unit->execution)
                   : nullptr;
          const auto *anchor =
              std::get_if<OriginalMovementAnchor>(&arrival.actual_anchor);
          if (!entry || !entry->contact || !unit || !close || !group ||
              group->journey_id != *engagement.journey_id ||
              execution_key(arrival.execution) !=
                  execution_key(unit->execution) ||
              close->kind != MovementCloseKind::contact ||
              !same_source(close->actual_cause, {"travel", "movement_contact",
                                                 entry->contact->id}) ||
              (entry->contact->first_prefix_id != arrival.prefix_id &&
               entry->contact->second_prefix_id != arrival.prefix_id) ||
              !unit_actor(movement, *unit, actor) || !anchor ||
              anchor->actual_source.domain != "military" ||
              anchor->actual_source.kind != "field_battle_segment" ||
              !std::holds_alternative<SettlementLocation>(anchor->location))
            fail("FieldSpatialArrivalMemberMismatch", actor);
          const auto *actual = common.state().facts.find(arrival.id);
          if (!actual || actual->domain != "travel" ||
              actual->kind != "movement_arrival")
            fail("FieldSpatialArrivalHeadRequired", actor);
          const auto entered =
              take(read_actual_common_fact_position(common, *boundary.entered));
          const auto terminal = take(
              read_actual_common_fact_position(common, anchor->actual_source));
          const auto handed =
              take(read_actual_common_fact_position(common, *actual));
          if (entered >= terminal || terminal >= handed ||
              take(compare_world_moments(*boundary.entered_at,
                                         arrival.occurred_at, b)) > 0 ||
              take(compare_world_moments(
                  arrival.occurred_at, take(sources.current_moment()), b)) > 0)
            fail("FieldSpatialArrivalOrderMismatch", actor);
          boundary.released = actual;
          boundary.released_at = &arrival.occurred_at;
        }
        field_boundaries.push_back(boundary);
      }
      // 所有真实重绑成员都保留稳定新点，包括早已有序退出者；后者不新增参战终点。
      for (const auto &actual : field_stable_returns) {
        if (actual.actor_id != actor || actual.immutable_source_head.size() != 2)
          fail("FieldSpatialStableReturnPlacementIdentityRequired", actor);
        const auto &cancel = actual.immutable_source_head[0], &ready = actual.immutable_source_head[1];
        if (cancel.schema != 1 || ready.schema != 1 || cancel.source.domain != "travel" ||
            cancel.source.kind != "group_movement_control" || ready.source.domain != "military" ||
            ready.source.kind != "field_army_exit_ready" ||
            take(read_actual_common_fact_position(common, ready.source)) >=
                take(read_actual_common_fact_position(common, cancel.source)) ||
            std::count_if(field_stable_returns.begin(), field_stable_returns.end(), [&](const auto &other) {
              return !other.immutable_source_head.empty() && same_source(other.immutable_source_head.front().source, cancel.source);
            }) != 1)
          fail("FieldSpatialStableReturnPlacementSourceRequired", actor);
      }
      for (const auto &placement : field_placements) {
        if (placement.actor_id != actor ||
            placement.immutable_source_head.size() != 1)
          fail("FieldSpatialOriginalPlacementRequired", actor);
        const auto &source = placement.immutable_source_head.front();
        if (source.schema != 1 ||
            !is_field_battle_exit_placement_source(source.source))
          fail("FieldSpatialOriginalPlacementSourceRequired", actor);
        std::size_t matched = 0;
        for (const auto &boundary : field_boundaries)
          if (boundary.release &&
              same_source(boundary.release->actual_release_source,
                          source.source)) {
            if (take(compare_world_moments(*boundary.released_at,
                                           placement.occurred_at, b)) != 0)
              fail("FieldSpatialReleaseMomentMismatch", actor);
            ++matched;
          }
        if (matched != 1)
          fail("FieldSpatialPlacementAdoptionRequired", actor);
      }
      for (const auto &boundary : field_boundaries)
        if (boundary.release) {
          const auto count = std::count_if(
              field_placements.begin(), field_placements.end(),
              [&](const auto &placement) {
                return placement.immutable_source_head.size() == 1 &&
                       same_source(
                           placement.immutable_source_head.front().source,
                           boundary.release->actual_release_source);
              });
          if (count != 1)
            fail("FieldSpatialReleasePlacementRequired", actor);
        }
      // 死俘 placement 与实际 engagement 终点必须一一对应；不能由一个裸
      // casualty/Capacity 行伪造位置，也不能把本 actor 的另一段历史混入。
      for (const auto &loss : field_losses) {
        const auto &placement = loss.actual_placement;
        if (!loss.original_loss || loss.original_loss->actor_id != actor ||
            placement.actor_id != actor ||
            placement.immutable_source_head.size() != 1)
          fail("FieldSpatialLossPlacementRequired", actor);
        const auto &source = placement.immutable_source_head.front();
        if (source.schema != 1 ||
            !is_field_member_loss_placement_source(source.source) ||
            source.source.id != loss.original_loss->id ||
            bool(loss.carried_army_id) !=
                std::holds_alternative<GroupMemberCaptivityApplication>(
                    loss.original_loss->actual_body) ||
            (loss.carried_army_id && loss.carried_army_id->empty()))
          fail("FieldSpatialLossPlacementSourceMismatch", actor);
        std::size_t matched = 0;
        for (const auto &boundary : field_boundaries)
          if (boundary.loss == loss.original_loss) {
            if (take(compare_world_moments(*boundary.released_at,
                                           placement.occurred_at, b)) != 0)
              fail("FieldSpatialLossPlacementMomentMismatch", actor);
            ++matched;
          }
        if (matched != 1)
          fail("FieldSpatialLossPlacementAdoptionRequired", actor);
      }
      for (const auto &boundary : field_boundaries)
        if (boundary.loss &&
            std::count_if(field_losses.begin(), field_losses.end(),
                          [&](const auto &loss) {
                            return loss.original_loss == boundary.loss;
                          }) != 1)
          fail("FieldSpatialLossOriginalPlacementRequired", actor);
    } else {
      for (const auto &row : common.state().facts.rows()) {
        const auto &source = row.second;
        if ((source.domain == "military" &&
             (source.kind == "field_battle_contact" ||
              source.kind == "field_battle_entered" ||
              source.kind == "field_battle_exit" || source.kind == "field_army_exit_ready")) ||
            (source.domain == "travel" &&
             (source.kind == "group_contact_member_release" ||
              source.kind == "group_member_loss_handoff")))
          fail("FieldSpatialOriginalHistoryRequired", actor);
      }
    }
    // One source walk is shared by selecting the real opening position and
    // checking every zero-duration transition inside the service span. No
    // history mirror.
    auto walk = [&](auto &&emit) {
      if (const auto *definition = actor_catalog.actors.find(actor)) {
        const auto origin = take(read_authored_character_origin_v2(
            common.state(), foundation, actor_catalog, actor));
        const auto *domain =
            foundation.actor_start_context.actor_domain_bindings.find(actor);
        if (definition->placement.kind !=
                AuthoredStartPlacement::Kind::at_place ||
            !domain ||
            domain->placement_ref.kind !=
                AuthoredStartPlacement::Kind::at_place ||
            domain->placement_ref.place_ref != definition->placement.place_ref)
          fail("ActorOriginalPlacementRequired", actor);
        const auto *place =
            catalog.places.find(definition->placement.place_ref);
        if (!place)
          fail("ActorOriginalPlaceRequired", actor);
        const SourceRef ref{"actor_foundation", "scenario_placement", actor};
        emit(origin.created_at.exact_moment, 0,
             MovementAnchor{OriginalMovementAnchor{
                 SettlementLocation{place->settlement_id, place->id}, ref}},
             ref, ref);
      } else if (const auto *creation = common.state().facts.find(actor);
                 creation && creation->domain == "family_birth" &&
                 creation->kind == "actor_creation") {
        if (!births || !births->bound_to(common))
          fail("BornActualPlacementHistoryRequired", actor);
        const auto original =
            take(births->read_birth_placement_for_child(actor));
        if (!original.actual_placement || original.child_actor_id != actor ||
            original.location_result.source_schema != 2)
          fail("BornActualPlacementSourceRequired", actor);
        head(common, original.location_result.source_ref);
        emit(original.occurred_at, 0,
             original.actual_placement->resolved_anchor,
             original.location_result.source_ref,
             original.location_result.source_ref);
      } else if (family_catalog) {
        const auto placement = take(read_created_character_initial_placement_v2(
            common, foundation, actor_catalog, *family_catalog, catalog, actor,
            take(sources.current_moment())));
        if (placement.actor_id != actor || placement.source_schema != 2)
          fail("ActorCreatedPlacementSourceRequired", actor);
        head(common, placement.actual_creation_source);
        emit(placement.created_at, 0,
             MovementAnchor{OriginalMovementAnchor{
                 placement.location, placement.actual_creation_source}},
             placement.actual_creation_source,
             placement.actual_creation_source);
      } else
        fail("ActorInitialPlacementOwnerRequired", actor);
      if (military_actions) {
        if (!military_actions->bound_to(common, movement))
          fail("CrossCandidateMilitaryActionPlacementRead", actor);
        for (const auto &actual :
             take(military_actions->read_military_action_placement_history(
                 actor))) {
          if (actual.actor_id != actor ||
              actual.actual_application.source_schema != 2)
            fail("MilitaryActionPlacementActorMismatch", actor);
          head(common, actual.actual_application.source_ref);
          emit(actual.occurred_at, 7, actual.actual_anchor,
               actual.actual_application.source_ref,
               actual.actual_application.source_ref);
        }
      } else
        for (const auto &fact : common.state().facts.rows())
          if (is_military_action_placement_source_kind(
                  {fact.second.domain, fact.second.kind, fact.second.id}))
            fail("MilitaryActionPlacementOriginalReadRequired", actor);
      // 释放是原 Military 的一次性实际落点，不另建 S1 历史镜像或借当前 Common
      // 地址。
      if (custody_releases) {
        if (!custody_releases->bound_to(common, movement))
          fail("CrossCandidateNamedMilitaryCustodyPlacementRead", actor);
        for (const auto *actual :
             take(custody_releases->read_named_custody_placement_history(
                 actor))) {
          if (!actual || actual->actor_id != actor)
            fail("NamedMilitaryCustodyPlacementActorMismatch", actor);
          const SourceRef source{"military", "military_named_custody_released",
                                 actual->id};
          head(common, source);
          emit(actual->released_at, 8, actual->released_anchor, source, source);
        }
      } else
        for (const auto &fact : common.state().facts.rows())
          if (fact.second.domain == "military" &&
              fact.second.kind == "military_named_custody_released")
            fail("NamedMilitaryCustodyPlacementOriginalReadRequired", actor);
      for (const auto &actual : field_placements) {
        const auto &source = actual.immutable_source_head.front().source;
        for (const auto &boundary : field_boundaries)
          if (boundary.release &&
              same_source(boundary.release->actual_release_source, source)) {
            const PreparedActorSpatialInitialSource adopted(
                {boundary.released->domain, boundary.released->kind,
                 boundary.released->id});
            emit(actual.occurred_at, 9, actual.actual_anchor, source,
                 adopted.source());
            emit(actual.occurred_at, 9, actual.actual_anchor, adopted.source(),
                 adopted.source());
          }
      }
      for (const auto &loss : field_losses) {
        // 被俘转为真实 captor carrier，不能把该时刻的世界锚当作其后静置。
        if (loss.carried_army_id)
          continue;
        const auto &placement = loss.actual_placement;
        const auto &source = placement.immutable_source_head.front().source;
        emit(placement.occurred_at, 9, placement.actual_anchor, source, source);
      }
      // 两头指同一次实际采用：ready给真实物理点，cancel是结束旧hold的真实因果头。
      for (const auto &actual : field_stable_returns) {
        const auto &cancel = actual.immutable_source_head[0].source;
        const auto &ready = actual.immutable_source_head[1].source;
        emit(actual.occurred_at, 9, actual.actual_anchor, ready, cancel);
        emit(actual.occurred_at, 9, actual.actual_anchor, cancel, cancel);
      }
      for (const auto &row : movement.units.rows()) {
        const auto &unit = row.second;
        if (!unit_actor(movement, unit, actor))
          continue;
        for (const auto &ref : unit.effort_basis_refs) {
          const auto *basis = movement.exertion_bases.find(ref.id);
          if (!basis || !has_actor(basis->actual_participants, actor))
            continue;
          head(common, ref);
          take(read_movement_source_moment(movement, ref, 1, b));
          auto anchor = point(basis->actual_basis, basis->opened_cursor, b);
          emit(basis->opened_at, 2, anchor, ref, ref);
          emit(basis->opened_at, 2, anchor, basis->original_rule_source, ref);
          for (const auto &work : unit.activity_spans)
            if (work.basis_ref == basis_id(basis->actual_basis)) {
              take(make_moment_span(work.span.from, work.span.to, b));
              if (take(compare_world_moments(work.span.from, work.span.to,
                                             b)) >= 0 ||
                  take(compare_world_moments(work.span.from, basis->opened_at,
                                             b)) < 0 ||
                  compare(work.cursor_to,
                          add(work.cursor_from,
                              mul(work.rate,
                                  take(elapsed_world_duration(work.span.from,
                                                              work.span.to, b))
                                      .value,
                                  b),
                              b),
                          b))
                fail("ActorSpatialHistoryActivityMismatch", unit.id);
              if (!point_read && overlap(work.span, span, b))
                fail("ActorLocationNotStationary", actor);
              // A frozen body-input head may describe an actual in-motion
              // write. The immutable effort plus the retained positive activity
              // reconstructs that point; this does not make motion a stationary
              // service window.
              std::optional<SourceRef> selected_head;
              if (frozen_sources)
                for (const auto &source : *frozen_sources) {
                  if (same_source(source.source, ref))
                    selected_head = ref;
                  else if (source.source.domain == "travel" &&
                           source.source.kind == "personal_movement_control") {
                    const auto *control =
                        movement.personal_controls.find(source.source.id);
                    if (control &&
                        control->operation ==
                            PersonalMovementControlOperation::resume &&
                        control->actor_id == actor &&
                        control->unit_ref == unit.id &&
                        control->basis_ref == basis_id(basis->actual_basis) &&
                        take(compare_world_moments(control->occurred_at,
                                                   work.span.from, b)) <= 0)
                      selected_head = source.source;
                  }
                }
              if (actual_prefix && visible(basis->opened_at, ref)) {
                selected_head = ref;
                std::size_t selected_order = fact_order(ref) + 1;
                for (const auto &row : movement.personal_controls.rows()) {
                  const auto &control = row.second;
                  const SourceRef cause{"travel", "personal_movement_control",
                                        row.first};
                  if (control.actor_id != actor ||
                      control.unit_ref != unit.id ||
                      control.basis_ref != basis_id(basis->actual_basis) ||
                      take(compare_world_moments(control.occurred_at,
                                                 work.span.from, b)) > 0 ||
                      !visible(control.occurred_at, cause))
                    continue;
                  const auto ordinal = fact_order(cause) + 1;
                  if (ordinal <= selected_order)
                    continue;
                  selected_order = ordinal;
                  if (control.operation ==
                      PersonalMovementControlOperation::resume)
                    selected_head = cause;
                  else
                    selected_head.reset();
                }
              }
              if (selected_head &&
                  take(compare_world_moments(work.span.from, span.from, b)) <
                      0 &&
                  take(compare_world_moments(span.from, work.span.to, b)) <=
                      0) {
                const auto cursor =
                    add(work.cursor_from,
                        mul(work.rate,
                            take(elapsed_world_duration(work.span.from,
                                                        span.from, b))
                                .value,
                            b),
                        b);
                emit(span.from, 3, point(basis->actual_basis, cursor, b),
                     *selected_head, *selected_head);
              } else if (point_read &&
                         take(compare_world_moments(work.span.from, span.from,
                                                    b)) < 0 &&
                         take(compare_world_moments(span.from, work.span.to,
                                                    b)) < 0)
                fail("ActorSpatialObservationDuringMotion", actor);
            }
        }
        if (const auto *close = movement.unit_closes.find(unit.id + ":close")) {
          const SourceRef ref{"travel", "movement_unit_close", close->id};
          head(common, ref);
          emit(close->occurred_at, 4, close->actual_anchor, ref, ref);
          emit(close->occurred_at, 4, close->actual_anchor, close->actual_cause,
               ref);
        }
      }
      for (const auto &row : movement.arrivals.rows()) {
        const auto &arrival = row.second;
        const auto *unit = movement.units.find(arrival.unit_ref);
        if (!unit || !unit_actor(movement, *unit, actor))
          continue;
        const SourceRef ref{"travel", "movement_arrival", arrival.id};
        head(common, ref);
        take(read_verified_movement_arrival(movement, ref, 1, b));
        emit(arrival.occurred_at, 5, arrival.actual_anchor, ref, ref);
      }
      for (const auto &row : movement.personal_controls.rows()) {
        const auto &control = row.second;
        const auto *unit = movement.units.find(control.unit_ref);
        if (!unit || !unit_actor(movement, *unit, actor))
          continue;
        const TravelExertionBasis *geometry = nullptr;
        for (const auto &original : unit->effort_basis_refs) {
          const auto *basis = movement.exertion_bases.find(original.id);
          if (basis && basis_id(basis->actual_basis) == control.basis_ref)
            geometry = basis;
        }
        if (!geometry)
          fail("ActorSpatialControlBasisRequired", row.first);
        const SourceRef ref{"travel", "personal_movement_control", row.first};
        head(common, ref);
        emit(control.occurred_at, 4,
             point(geometry->actual_basis, control.cursor, b), ref, ref);
      }
      for(const auto& [key,row]:movement.local_moves.rows()) if(row.actor_id==actor) {
        const SourceRef ref{"travel","local_move",key};
        const auto* actual=take(read_local_move_source(common,movement,ref,1,b));
        const auto* from=catalog.places.find(actual->from.place_id);
        const auto* to=catalog.places.find(actual->to.place_id);
        if(!from || !to || from->settlement_id!=actual->from.settlement_id ||
           to->settlement_id!=actual->to.settlement_id ||
           std::find(from->passages.begin(),from->passages.end(),to->id)==from->passages.end())
          fail("LocalMoveOriginalPassageRequired",key);
        emit(actual->occurred_at,10,MovementAnchor{OriginalMovementAnchor{actual->to,ref}},ref,ref);
      }
      for (const auto &row : registrations.registrations.rows())
        if (has_actor(row.second.accepted_scope.named_participants, actor)) {
          const SourceRef ref{"travel", "transport_registration", row.first};
          emit(row.second.occurred_at, 6, row.second.departure_anchor, ref,
               ref);
        }
      for (const auto &row : registrations.releases.rows())
        if (has_actor(row.second.released_composition.named_participants,
                      actor)) {
          const SourceRef ref{"travel", "transport_release", row.first};
          emit(row.second.occurred_at, 6, row.second.actual_anchor, ref, ref);
        }
    };
    // 每份 local-move 沿同一原事件 walk 核前驱；walk 只读局部源，不回调本完整函数。
    // 真实 Common cut 决定同 M 先后，不能以当前 Actor 投影或有效别人的旧头替代。
    for(const auto& [key,row]:movement.local_moves.rows()) if(row.actor_id==actor) {
      const auto count=take(read_actual_common_fact_prefix(common,row.before_prefix));
      std::optional<MovementAnchor> prior;
      std::optional<WorldMoment> prior_at;
      std::size_t prior_order=0,head_order=0;
      std::vector<bool> found(row.before_head.size(),false);
      walk([&](const WorldMoment& at,Integer,const MovementAnchor& anchor,
               const SourceRef& evidence,const SourceRef& causal) {
        const auto order=causal.domain=="actor_foundation" && causal.kind=="scenario_placement"
            ? std::size_t{0} : fact_order(causal)+1;
        const auto relative=take(compare_world_moments(at,row.occurred_at,b));
        if(order>count) {
          if(relative<0) fail("LocalMoveOriginalPrefixNotChronological",key);
          return;
        }
        if(relative>0) fail("LocalMoveOriginalPrefixInFuture",key);
        if(!prior_at || take(compare_world_moments(*prior_at,at,b))<0 ||
           (same_moment(*prior_at,at,b) && order>prior_order)) {
          prior=anchor; prior_at=at; prior_order=order;
        }
        for(std::size_t i=0;i<row.before_head.size();++i)
          if(same_source(evidence,row.before_head[i].source_ref)) {
            found[i]=true; head_order=std::max(head_order,order);
          }
      });
      const auto* original=prior?std::get_if<OriginalMovementAnchor>(&*prior):nullptr;
      const auto* place=original?std::get_if<SettlementLocation>(&original->location):nullptr;
      if(!place || place->settlement_id!=row.from.settlement_id || place->place_id!=row.from.place_id ||
         head_order!=prior_order || std::find(found.begin(),found.end(),false)!=found.end())
        fail("LocalMoveOriginalPredecessorMismatch",key);
    }
    // A frozen source head is issued by the actual current-location reader at a
    // real owner boundary. Resolve aliases through this same actor history
    // walk; a contact's Common head can precede the close that actually placed
    // the actor. No invented observation fact or stored container ordinal is
    // required.
    std::vector<bool> frozen_found;
    if (frozen_sources) {
      if (frozen_sources->empty())
        fail("ActorSpatialFrozenSourceHeadRequired", actor);
      frozen_found.resize(frozen_sources->size(), false);
      for (std::size_t i = 0; i < frozen_sources->size(); ++i) {
        const auto &ref = (*frozen_sources)[i];
        const auto expected =
            take(actor_spatial_history_source_schema(ref.source));
        if (!source_valid(ref.source) || ref.schema != expected ||
            (ref.source.domain == "travel" &&
             ref.source.kind == "movement_prefix"))
          fail("ActorSpatialImmutableSourceHeadRequired", ref.source.id);
        for (std::size_t j = 0; j < i; ++j)
          if (same_source(ref.source, (*frozen_sources)[j].source))
            fail("DuplicateActorSpatialFrozenSource", ref.source.id);
      }
      walk([&](const WorldMoment &at, Integer, const MovementAnchor &,
               const SourceRef &evidence, const SourceRef &causal) {
        if (take(compare_world_moments(at, span.from, b)) > 0)
          return;
        for (std::size_t i = 0; i < frozen_sources->size(); ++i)
          if (same_source(evidence, (*frozen_sources)[i].source)) {
            frozen_found[i] = true;
            const auto ordinal = causal.domain == "actor_foundation" &&
                                         causal.kind == "scenario_placement"
                                     ? 0
                                     : fact_order(causal) + 1;
            frozen_order = std::max(frozen_order, ordinal);
          }
      });
      if (std::find(frozen_found.begin(), frozen_found.end(), false) !=
          frozen_found.end())
        fail("ActorSpatialFrozenSourceNotActual", actor);
    }
    // Original registration/release lifetime is authoritative even after
    // dissolution.
    for (const auto &row : registrations.registrations.rows()) {
      const auto &registration = row.second;
      if (!has_actor(registration.accepted_scope.named_participants, actor))
        continue;
      const SourceRef ref{"travel", "transport_registration", registration.id};
      head(common, ref);
      const auto *unit =
          carriers.transport_units.find(registration.transport_unit_id);
      if (!unit || !same_source(unit->registration_source_ref, ref))
        fail("ActorTransportHistoryBacklinkRequired", registration.id);
      take(sources.verify_registration_source(*unit));
      const TransportUnitReleaseSource *release = nullptr;
      for (const auto &r : registrations.releases.rows())
        if (r.second.transport_unit_id == registration.transport_unit_id) {
          if (release)
            fail("DuplicateActorTransportRelease", registration.id);
          release = &r.second;
        }
      if (release) {
        head(common, {"travel", "transport_release", release->id});
        if (!has_actor(release->released_composition.named_participants,
                       actor) ||
            take(compare_world_moments(release->occurred_at,
                                       registration.occurred_at, b)) < 0)
          fail("ActorTransportReleaseHistoryMismatch", release->id);
      }
      if (point_read
              ? (visible(registration.occurred_at, ref) &&
                 (!release ||
                  !visible(release->occurred_at,
                           {"travel", "transport_release", release->id})))
              : (take(compare_world_moments(registration.occurred_at, span.to,
                                            b)) < 0 &&
                 (!release || take(compare_world_moments(
                                  span.from, release->occurred_at, b)) < 0)))
        fail("NamedServiceDuringTransportParticipation", actor);
    }
    // 首次捕获到真关闭之间是原看管方 carrier 的位置，不是被俘前的静置点。
    // 当前叶尚不重放这条 carrier 历史，缺源必须明确拒绝，不能让释放写入与
    // 冷读同时借同一个旧点互证。关闭后的原释放位置仍沿上面的真实叶读取。
    if (custody_releases) {
      if (!custody_releases->bound_to(common, movement))
        fail("CrossCandidateNamedMilitaryCustodyPlacementRead", actor);
      const auto releases =
          take(custody_releases->read_named_custody_placement_history(actor));
      for (const auto *term :
           take(custody_releases->read_named_custody_capture_history(actor))) {
        if (!term || term->actor_id != actor)
          fail("NamedMilitaryCustodyPlacementActorMismatch", actor);
        const SourceRef captured{"military", "military_named_custody_term",
                                 term->id};
        head(common, captured);
        const NamedMilitaryCustodyRelease *closed = nullptr;
        for (const auto *release : releases)
          if (release->term_id == term->id) {
            if (closed)
              fail("NamedMilitaryCustodyReleaseRepeated", term->id);
            closed = release;
          }
        const bool active =
            point_read
                ? (visible(term->adopted_at, captured) &&
                   (!closed ||
                    !visible(closed->released_at,
                             {"military", "military_named_custody_released",
                              closed->id})))
                : (take(compare_world_moments(term->adopted_at, span.to, b)) <
                       0 &&
                   (!closed || take(compare_world_moments(
                                   span.from, closed->released_at, b)) < 0));
        if (active)
          fail("NamedMilitaryCustodyCarrierHistoryRequired", actor);
      }
    } else
      for (const auto &row : common.state().facts.rows())
        if (row.second.kind == "military_named_custody_term")
          fail("NamedMilitaryCustodyPlacementOriginalReadRequired", actor);
    for (const auto &boundary : field_boundaries) {
      const PreparedActorSpatialInitialSource entered({boundary.entered->domain,
                                                       boundary.entered->kind,
                                                       boundary.entered->id});
      bool active = false;
      if (point_read) {
        const bool began = visible(*boundary.entered_at, entered.source());
        bool ended = false;
        if (boundary.released) {
          const PreparedActorSpatialInitialSource released(
              {boundary.released->domain, boundary.released->kind,
               boundary.released->id});
          ended = visible(*boundary.released_at, released.source());
        }
        active = began && !ended;
      } else {
        // 同 M 的入场与真实交接仍按 Common 前缀处理瞬时位置；空的
        // [entered, released) 不凭两个有序头变成正时长参战区间。
        active =
            (!boundary.released_at ||
             take(compare_world_moments(*boundary.entered_at,
                                        *boundary.released_at, b)) < 0) &&
            take(compare_world_moments(*boundary.entered_at, span.to, b)) < 0 &&
            (!boundary.released_at ||
             take(compare_world_moments(span.from, *boundary.released_at, b)) <
                 0);
      }
      if (active)
        fail("NamedServiceDuringFieldEngagement", actor);
    }
    // 已交给俘获方不等于普通世界静置。真正 carrier 的后继链尚未接入本口，
    // 因而只有其采用头之前的原读可以继续；不能以入场区间已结束而回落旧点。
    for (const auto &loss : field_losses) {
      if (!loss.carried_army_id)
        continue;
      const auto &placement = loss.actual_placement;
      const auto &source = placement.immutable_source_head.front().source;
      const bool affected =
          point_read
              ? visible(placement.occurred_at, source)
              : take(compare_world_moments(placement.occurred_at, span.to, b)) <
                    0;
      if (affected)
        fail("FieldCapturedCarrierHistoryRequired", actor);
    }
    // A local writer is not stationary world presence. Entry participants come
    // from the actual closed world unit, not today's roster or a carrier name.
    if (!military) {
      for (const auto &row : common.state().facts.rows())
        if (row.second.domain == "military" &&
            row.second.kind == "military_spatial_handoff")
          throw Failure{common_failure("ActorMilitarySpatialHistoryRequired",
                                       actor, ErrorKind::unsupported_capability)
                            .error};
    } else
      for (const auto &row : military->handoffs.rows()) {
        const auto &entry = row.second;
        const auto *entered =
            std::get_if<BattleMovementWriter>(&entry.to_binding.writer);
        if (!entered)
          continue;
        head(common, {"military", "military_spatial_handoff", entry.id});
        const auto *close =
            entry.closed_writer_source_ref.domain == "travel" &&
                    entry.closed_writer_source_ref.kind == "movement_unit_close"
                ? movement.unit_closes.find(entry.closed_writer_source_ref.id)
                : nullptr;
        const auto *unit =
            close ? movement.units.find(close->unit_ref) : nullptr;
        if (!close || !unit || !same_moment(close->occurred_at, entry.at, b))
          fail("ActorMilitaryEntryWorldSourceRequired", entry.id);
        if (!unit_actor(movement, *unit, actor))
          continue;
        const MilitarySpatialHandoff2 *exit = nullptr;
        for (const auto &candidate : military->handoffs.rows()) {
          const auto *from = std::get_if<BattleMovementWriter>(
              &candidate.second.from_binding.writer);
          if (from && from->handoff_id == entry.id) {
            if (exit)
              fail("DuplicateActorMilitaryExit", entry.id);
            exit = &candidate.second;
          }
        }
        if (exit) {
          head(common, {"military", "military_spatial_handoff", exit->id});
          if (take(compare_world_moments(exit->at, entry.at, b)) < 0 ||
              !std::holds_alternative<WorldMovementWriter>(
                  exit->to_binding.writer))
            fail("ActorMilitaryExitHistoryMismatch", exit->id);
        }
        if (point_read
                ? (visible(entry.at, {"military", "military_spatial_handoff",
                                      entry.id}) &&
                   (!exit ||
                    !visible(exit->at, {"military", "military_spatial_handoff",
                                        exit->id})))
                : (take(compare_world_moments(entry.at, span.to, b)) < 0 &&
                   (!exit ||
                    take(compare_world_moments(span.from, exit->at, b)) < 0)))
          fail("NamedServiceDuringMilitarySpatialBinding", actor);
      }
    walk([&](const WorldMoment &at, Integer, const MovementAnchor &anchor,
             const SourceRef &evidence, const SourceRef &causal) {
      if (!visible(at, causal))
        return;
      if (same_source(evidence, accepted))
        accepted_found = true;
      const auto order =
          latest ? take(compare_world_moments(at, latest_at, b)) : 1;
      const std::size_t event_order =
          causal.domain == "actor_foundation" &&
                  causal.kind == "scenario_placement"
              ? 0
              : fact_order(causal) + 1;
      if (order > 0 || (order == 0 && event_order > latest_event_order)) {
        latest = anchor;
        latest_at = at;
        latest_event_order = event_order;
        latest_source = evidence;
        latest_causal_source = causal;
      } else if (order == 0 && event_order == latest_event_order &&
                 !take(same_spatial_anchor(*latest, anchor, b)))
        fail("ActorSpatialHistorySimultaneousPositionMismatch", actor);
    });
    if (!accepted_found || !latest)
      fail("ActorAcceptedPresenceHistoryRequired", accepted.id);
    if (frozen_sources)
      for (const auto &frozen : *frozen_sources) {
        std::optional<MovementAnchor> head_anchor;
        WorldMoment head_at;
        std::size_t head_order = 0;
        walk([&](const WorldMoment &at, Integer, const MovementAnchor &anchor,
                 const SourceRef &evidence, const SourceRef &causal) {
          if (!same_source(evidence, frozen.source) || !visible(at, causal))
            return;
          const auto ordinal = causal.domain == "actor_foundation" &&
                                       causal.kind == "scenario_placement"
                                   ? 0
                                   : fact_order(causal) + 1;
          const auto cmp =
              head_anchor ? take(compare_world_moments(at, head_at, b)) : 1;
          if (cmp > 0 || (cmp == 0 && ordinal >= head_order)) {
            head_anchor = anchor;
            head_at = at;
            head_order = ordinal;
          }
        });
        if (!head_anchor ||
            !take(same_spatial_anchor(*latest, *head_anchor, b)))
          fail("ActorSpatialFrozenHeadPositionMismatch", frozen.source.id);
      }
    auto schema = [](const SourceRef &ref) -> Integer {
      return take(actor_spatial_history_source_schema(ref));
    };
    ActorStationaryLocationSpan out{actor,
                                    accepted,
                                    span,
                                    *latest,
                                    {{accepted, schema(accepted)}},
                                    latest_causal_source};
    auto retain = [&](const SourceRef &ref) {
      if (std::none_of(out.original_sources.begin(), out.original_sources.end(),
                       [&](const auto &old) {
                         return same_source(old.source_ref, ref);
                       }))
        out.original_sources.push_back({ref, schema(ref)});
    };
    retain(latest_source);
    retain(latest_causal_source);
    walk([&](const WorldMoment &at, Integer, const MovementAnchor &anchor,
             const SourceRef &evidence, const SourceRef &causal) {
      if (take(compare_world_moments(at, span.from, b)) <= 0 ||
          take(compare_world_moments(at, span.to, b)) >= 0)
        return;
      if (!take(same_spatial_anchor(out.actual_anchor, anchor, b)))
        fail("ActorLocationChangedDuringService", actor);
      retain(evidence);
      retain(causal);
    });
    // Do not silently return an old opening point after a later positive
    // interval. A genuine end/hold source at or after that work must have
    // established the selected point. This also rejects an observation before
    // its same-M close.
    for (const auto &row : movement.units.rows())
      if (unit_actor(movement, row.second, actor))
        for (const auto &work : row.second.activity_spans) {
          if (take(compare_world_moments(work.span.to, span.from, b)) <= 0 &&
              take(compare_world_moments(work.span.to, latest_at, b)) > 0)
            fail("ActorSpatialStationaryCloseSourceRequired", actor);
        }
    // Unconsumed Running work is not absence of movement. A caller cannot read
    // a future positive service prefix before the unique movement writer
    // settles it.
    for (const auto &row : movement.prefixes.rows()) {
      const auto &prefix = row.second;
      const auto *unit = movement.units.find(prefix.unit_ref);
      if (!unit || !unit_actor(movement, *unit, actor) ||
          std::holds_alternative<MovementClosed>(prefix.lifecycle))
        continue;
      if (const auto *running =
              std::get_if<RunningMovementMap>(&prefix.activity_world_map))
        if (take(compare_world_moments(running->world_from, span.to, b)) < 0 &&
            take(compare_world_moments(prefix.last_settled_at, span.to, b)) < 0)
          fail("ActorSpatialHistoryUnsettledMotion", actor);
    }
    if (actual_events)
      walk([&](const WorldMoment &at, Integer rank,
               const MovementAnchor &anchor, const SourceRef &evidence,
               const SourceRef &causal) {
        if (rank == 3 || !same_source(evidence, causal))
          return;
        actual_events->push_back({at, anchor, {{evidence, schema(evidence)}}});
      });
    return out;
  });
}
Outcome<ActorStationaryLocationSpan> read_actor_stationary_location_span(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const SourceRef &accepted, const MomentSpan &span,
    const MilitarySpatialHandoffState *military, ExactArithmeticBudget budget,
    const FamilyCatalog *family_catalog, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return read_actor_history_impl(
      c, catalog, m, carriers, registrations, foundation, actors, sources,
      actor, accepted, span, military, budget, nullptr, family_catalog, nullptr,
      nullptr, nullptr, births, military_actions, custody_releases,
      field_exits);
}
Outcome<ActorHistoricalSpatialPoint> read_actor_spatial_location_before_source(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const SourceRef &accepted,
    const SourceRef &observation, const WorldMoment &at,
    const MilitarySpatialHandoffState *military, ExactArithmeticBudget budget,
    const FamilyCatalog *family_catalog, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<ActorHistoricalSpatialPoint>([&] {
    auto actual = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, accepted, {at, at}, military, budget, &observation,
        family_catalog, nullptr, nullptr, nullptr, births, military_actions,
        custody_releases, field_exits));
    return ActorHistoricalSpatialPoint{actor, at, observation,
                                       std::move(actual.actual_anchor),
                                       std::move(actual.original_sources)};
  });
}

Outcome<ActorSpatialLocationView>
read_actor_spatial_location_at_actual_fact_prefix(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const WorldMoment &at,
    const ActorSpatialActualFactPrefix &prefix,
    const MilitarySpatialHandoffState *military, ExactArithmeticBudget budget,
    const FamilyCatalog *family_catalog, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<ActorSpatialLocationView>([&] {
    const auto initial = [&] {
      const SourceRefBuildView fallback =
          actors.actors.find(actor)
              ? SourceRefBuildView{"actor_foundation", "scenario_placement",
                                   actor}
              : SourceRefBuildView{"family", "actor_creation", actor};
      if (const auto *creation = c.state().facts.find(actor);
          creation && creation->domain == "family_birth" &&
          creation->kind == "actor_creation") {
        if (!births || !births->bound_to(c))
          fail("BornActualPlacementHistoryRequired", actor);
        const auto placement =
            take(births->read_birth_placement_for_child(actor));
        const auto &source = placement.location_result.source_ref;
        return PreparedActorSpatialInitialSource(
            {source.domain, source.kind, source.id});
      }
      return PreparedActorSpatialInitialSource(fallback);
    }();
    auto actual = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, initial.source(), {at, at}, military, budget, nullptr,
        family_catalog, nullptr, nullptr, &prefix, births, military_actions,
        custody_releases, field_exits));
    ActorSpatialLocationView out;
    out.actor_id = actor;
    out.actual_anchor = std::move(actual.actual_anchor);
    out.location_snapshot = take(project_movement_location(
        *take(require_actor_ordinary_movement_anchor(out)), budget));
    if (is_military_action_placement_source_kind(
            actual.actual_causal_position_source)) {
      if (!military_actions)
        fail("MilitaryActionPlacementOriginalReadRequired", actor);
      auto placement = take(military_actions->read_military_action_placement(
          {actual.actual_causal_position_source, 2}, actor));
      if (!placement)
        fail("MilitaryActionPlacementActorMismatch", actor);
      out.location_snapshot = placement->original_location_result;
    }
    out.actual_location_sources.push_back(
        {actual.actual_causal_position_source,
         take(actor_spatial_history_source_schema(
             actual.actual_causal_position_source))});
    return out;
  });
}

Outcome<ActorSpatialLocationView> read_actor_spatial_location_at_source_head(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const std::vector<ActualSpatialSourceRef> &frozen,
    const WorldMoment &at, const MilitarySpatialHandoffState *military,
    ExactArithmeticBudget budget, const FamilyCatalog *family_catalog,
    const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<ActorSpatialLocationView>([&] {
    if (frozen.empty())
      fail("ActorSpatialFrozenSourceHeadRequired", actor);
    auto actual = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, frozen.front().source, {at, at}, military, budget, nullptr,
        family_catalog, &frozen, nullptr, nullptr, births, military_actions,
        custody_releases, field_exits));
    ActorSpatialLocationView result;
    result.actor_id = actor;
    result.actual_anchor = std::move(actual.actual_anchor);
    result.location_snapshot = take(project_movement_location(
        *take(require_actor_ordinary_movement_anchor(result)), budget));
    if (is_military_action_placement_source_kind(
            actual.actual_causal_position_source)) {
      if (!military_actions)
        fail("MilitaryActionPlacementOriginalReadRequired", actor);
      auto placement = take(military_actions->read_military_action_placement(
          {actual.actual_causal_position_source, 2}, actor));
      if (!placement)
        fail("MilitaryActionPlacementActorMismatch", actor);
      result.location_snapshot = placement->original_location_result;
    }
    result.actual_location_sources = frozen;
    return result;
  });
}

Outcome<Integer> compare_actor_spatial_source_heads_at_moment(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const std::vector<ActualSpatialSourceRef> &first,
    const std::vector<ActualSpatialSourceRef> &second, const WorldMoment &at,
    const MilitarySpatialHandoffState *military, ExactArithmeticBudget budget,
    const FamilyCatalog *family_catalog, const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<Integer>([&]() -> Integer {
    if (first.empty() || second.empty())
      fail("ActorSpatialFrozenSourceHeadRequired", actor);
    const auto a = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, first.front().source, {at, at}, military, budget, nullptr,
        family_catalog, &first, nullptr, nullptr, births, military_actions,
        custody_releases, field_exits));
    const auto z = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, second.front().source, {at, at}, military, budget, nullptr,
        family_catalog, &second, nullptr, nullptr, births, military_actions,
        custody_releases, field_exits));
    auto ordinal = [&](const SourceRef &ref) -> std::size_t {
      if (ref.domain == "actor_foundation" && ref.kind == "scenario_placement")
        return 0;
      head(c, ref);
      const auto &order = c.state().facts.order();
      const auto found = std::find(order.begin(), order.end(), ref.id);
      if (found == order.end())
        fail("ActorSpatialHistorySourceOrderRequired", ref.id);
      return std::size_t(found - order.begin()) + 1;
    };
    const auto x = ordinal(a.actual_causal_position_source),
               y = ordinal(z.actual_causal_position_source);
    if (x == y && !take(same_actual_movement_position(a.actual_anchor,
                                                      z.actual_anchor, budget)))
      fail("ActorSpatialHeadSameCausePositionMismatch", actor);
    return x < y ? -1 : x > y ? 1 : 0;
  });
}

Outcome<std::vector<ActorSpatialHeadChange>> read_actor_spatial_head_changes(
    const CommonStore &c, const CommonCatalog &catalog,
    const MovementExactState &m, const SpatialCarrierState &carriers,
    const TransportRegistrationState &registrations,
    const AuthoredActorFoundationState &foundation,
    const ActorFoundationCatalog &actors, const SpatialCarrierSources &sources,
    const StableId &actor, const std::vector<ActualSpatialSourceRef> &from_head,
    const WorldMoment &from_at,
    const std::vector<ActualSpatialSourceRef> &to_head,
    const WorldMoment &to_at, const MilitarySpatialHandoffState *military,
    ExactArithmeticBudget budget, const FamilyCatalog *family_catalog,
    const BirthPlacementRead *births,
    const MilitaryActionPlacementRead *military_actions,
    const NamedMilitaryCustodyPlacementRead *custody_releases,
    const FieldBattleExitPlacementRead *field_exits) {
  return guard<std::vector<ActorSpatialHeadChange>>([&] {
    if (from_head.empty() || to_head.empty() ||
        take(compare_world_moments(from_at, to_at, budget)) > 0)
      fail("ActorSpatialHeadIntervalRequired", actor);
    const auto from = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, from_head.front().source, {from_at, from_at}, military, budget,
        nullptr, family_catalog, &from_head, nullptr, nullptr, births,
        military_actions, custody_releases, field_exits));
    std::vector<ActorSpatialHeadChange> all;
    const auto to = take(read_actor_history_impl(
        c, catalog, m, carriers, registrations, foundation, actors, sources,
        actor, to_head.front().source, {to_at, to_at}, military, budget,
        nullptr, family_catalog, &to_head, &all, nullptr, births,
        military_actions, custody_releases, field_exits));
    auto ordinal = [&](const SourceRef &ref) -> std::size_t {
      if (ref.domain == "actor_foundation" && ref.kind == "scenario_placement")
        return 0;
      head(c, ref);
      const auto &ids = c.state().facts.order();
      const auto found = std::find(ids.begin(), ids.end(), ref.id);
      if (found == ids.end())
        fail("ActorSpatialHistorySourceOrderRequired", ref.id);
      return std::size_t(found - ids.begin()) + 1;
    };
    const auto lo = ordinal(from.actual_causal_position_source),
               hi = ordinal(to.actual_causal_position_source);
    if (hi < lo)
      fail("ActorSpatialHeadCausalOrderReversed", actor);
    std::vector<ActorSpatialHeadChange> result;
    std::set<std::size_t> seen;
    for (auto &item : all) {
      const auto n = ordinal(item.immutable_source_head.front().source);
      if (n <= lo || n > hi)
        continue;
      if (take(compare_world_moments(item.occurred_at, from_at, budget)) < 0 ||
          take(compare_world_moments(item.occurred_at, to_at, budget)) > 0)
        fail("ActorSpatialHeadCausalMomentMismatch", actor);
      if (!seen.insert(n).second)
        fail("DuplicateActorSpatialHeadProducer", actor);
      result.push_back(std::move(item));
    }
    std::sort(result.begin(), result.end(), [&](const auto &a, const auto &z) {
      const auto cmp =
          take(compare_world_moments(a.occurred_at, z.occurred_at, budget));
      return cmp ? cmp < 0
                 : ordinal(a.immutable_source_head.front().source) <
                       ordinal(z.immutable_source_head.front().source);
    });
    return result;
  });
}

} // namespace hanmo::resident
