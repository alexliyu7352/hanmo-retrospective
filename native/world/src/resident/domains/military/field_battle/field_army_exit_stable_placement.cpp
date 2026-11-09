#include "field_exit_placement.hpp"
#include "field_army_exit_ready.hpp"
#include "field_segment_settlement.hpp"
#include "field_ability_sources.hpp"
#include "field_exact_contact_source.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
// 完整ready/cancel采用重算真实新点；旧cancel.actual_anchor继续证明旧contact，不被改写。
Outcome<FieldActorArmyExitStablePlacementRead>
OriginalFieldBattleExitPlacementRead::read_field_army_exit_stable_placement(
    const MomentOwnerSource &source, const StableId &actor) const {
  return movement_detail::guard<FieldActorArmyExitStablePlacementRead>([&] {
    using namespace movement_detail;
    const auto &x = context_;
    const auto *exact = x.exact_contacts;
    if (source.source_schema != 1 || source.source_ref.domain != "travel" ||
        source.source_ref.kind != "group_movement_control" || !x.common.actor(actor))
      fail("FieldArmyExitStablePlacementSourceRequired", actor);
    if (!exact || !x.original_abilities || !x.original_body ||
        &exact->common != &x.common || &exact->military != &x.military ||
        &exact->field != &x.field || &exact->movement != &x.movement ||
        &exact->carriers != &x.carriers || &exact->groups != &x.groups ||
        &exact->travel != &x.travel || &exact->clock != &x.clock ||
        x.original_abilities->exact_contacts != exact ||
        x.original_abilities->conditions != x.original_body ||
        &x.original_body->common != &x.common || x.original_body->military != &x.military ||
        x.original_body->injury_field != &x.field || !x.clock.bound_to(x.common, x.military, x.field))
      fail("FieldArmyExitStablePlacementOriginalOwnerRequired", actor);
    const auto full = take(read_field_army_exit_continuation_source(
        *exact, x.catalog, *x.original_abilities, source.source_ref, budget_));
    if (!full.already_at_destination || full.opening || full.journey || !full.cancellation ||
        full.cancellation != x.groups.controls.find(source.source_ref.id) ||
        !full.ready.plan || !full.ready.plan->ready_source ||
        !full.ready.plan->actual_continuation ||
        !same_source(*full.ready.plan->actual_continuation, source.source_ref) ||
        std::count(full.ready.named_participants.begin(), full.ready.named_participants.end(), actor) != 1)
      fail("FieldArmyExitStablePlacementActualMemberRequired", actor);
    const auto &ready = *full.ready.plan->ready_source;
    const auto *point = std::get_if<OriginalMovementAnchor>(&full.ready.actual_anchor);
    if (!point || !std::holds_alternative<SettlementLocation>(point->location) ||
        !same_source(point->actual_source, ready) ||
        take(read_actual_common_fact_position(x.common, ready)) >=
            take(read_actual_common_fact_position(x.common, source.source_ref)))
      fail("FieldArmyExitStablePlacementActualPointRequired", actor);
    CheckpointVector<ActualSpatialSourceBuildView> heads;
    heads.push_back({{source.source_ref.domain, source.source_ref.kind, source.source_ref.id}, 1});
    heads.push_back({{ready.domain, ready.kind, ready.id}, 1});
    return FieldActorArmyExitStablePlacementRead{
        ActualActorSpatialWriteRead{ActualActorSpatialWriteReadBuildView{
            actor, full.ready.adopted_at, full.ready.actual_anchor, heads}},
        &full.ready.plan->army_id};
  });
}
// 只枚举真实稳定采用；尚未ready或只有候选出口不产生任何历史位置事实。
Outcome<CheckpointVector<ActualActorSpatialWriteRead>>
OriginalFieldBattleExitPlacementRead::read_field_army_exit_stable_placement_history(
    const StableId &actor) const {
  return movement_detail::guard<CheckpointVector<ActualActorSpatialWriteRead>>([&] {
    using namespace movement_detail;
    CheckpointVector<ActualActorSpatialWriteRead> out;
    const auto &x = context_;
    for (const auto &[id, plan] : x.field.army_exit_plans) {
      if (!plan.actual_continuation || plan.actual_continuation->domain != "travel" ||
          plan.actual_continuation->kind != "group_movement_control") continue;
      if (!x.exact_contacts || !x.original_abilities)
        fail("FieldArmyExitStablePlacementOriginalOwnerRequired", actor);
      const auto full = take(read_field_army_exit_continuation_source(
          *x.exact_contacts, x.catalog, *x.original_abilities, *plan.actual_continuation, budget_));
      if (full.ready.plan != &plan) fail("FieldArmyExitStablePlacementOriginalOwnerRequired", actor);
      if (std::find(full.ready.named_participants.begin(), full.ready.named_participants.end(), actor) ==
          full.ready.named_participants.end()) continue;
      auto actual = take(read_field_army_exit_stable_placement({*plan.actual_continuation, 1}, actor));
      out.push_back(std::move(actual.actual));
    }
    return out;
  });
}
} // namespace hanmo::resident
