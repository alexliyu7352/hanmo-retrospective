#include "native_actual_read_sources.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "resident/domains/actor_foundation/condition_application_fact.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "world_bundle.hpp"
namespace hanmo::resident {
namespace {
Status occurred(const WorldMoment &source, const WorldMoment &now,
                const StableId &id) {
  auto order = compare_world_moments(source, now);
  if (!order.ok)
    return {false, {}, order.error};
  return order.value > 0 ? common_failure("ReadStampActualSourceInFuture", id)
                         : common_success();
}
Status exact_at(const ActorFoundationAt &source, const WorldMoment &now,
                const StableId &id) {
  if (source.kind != ActorFoundationAt::Kind::exact_moment)
    return common_failure("ReadStampExactSourceMomentRequired", id);
  return occurred(source.exact_moment, now, id);
}
} // namespace
Status validate_native_actual_read_source(const WorldBundle &world,
                                          const StableId &id,
                                          const WorldMoment &now) {
  if (id.empty())
    return common_failure("ReadStampActualSourceMissing", id);
  const auto &common = world.common.state();
  const auto *actual_header = common.facts.find(id);
  const bool condition_header =
      actual_header &&
      owns_condition_application_fact_kind(actual_header->kind);
  bool common_anchor = actual_header != nullptr;
  if (const auto *action = common.actions.find(id)) {
    if (action->id != id || action->start_tick > now.half_day_tick ||
        !world.common.actor(action->actor_id))
      return common_failure("ReadStampActualActionMismatch", id);
    common_anchor = true;
  }
  const CommitResult *result = nullptr;
  for (const auto &row : common.results.rows())
    if (row.second && row.second->result_id == id) {
      if (result)
        return common_failure("ReadStampDuplicateActualResult", id);
      result = row.second.get();
    }
  if (result) {
    if (common_anchor || !result->ok || result->tick > now.half_day_tick ||
        result->revision > world.common.revision())
      return common_failure("ReadStampActualResultMismatch", id);
    const HistoryRecord *history = nullptr;
    for (const auto &row : common.history)
      if (row.result_id == id) {
        if (history)
          return common_failure("ReadStampDuplicateActualResultHistory", id);
        history = &row;
      }
    if (!history || history->kind != result->kind ||
        history->tick != result->tick || !world.common.actor(history->actor_id))
      return common_failure("ReadStampActualResultHistoryRequired", id);
    common_anchor = true;
  }
  if (!world.catalog || !world.catalog->exact_time ||
      !world.catalog->actor_foundation || !world.actor_foundation) {
    if (condition_header)
      return common_failure("ReadStampConditionApplicationOwnerRequired", id);
    return common_anchor ? common_success()
                         : common_failure("ReadStampActualSourceMissing", id);
  }
  const auto &foundation = *world.actor_foundation;
  const auto *seed =
      foundation.actor_start_context.initial_condition_seeds.find(id);
  const auto *receipt =
      foundation.character_condition_extensions.applied_receipts.find(id);
  const auto *boundary = world.growth.fixed_service_boundaries.find(id);
  // The selected application head and its original receipt are one source,
  // not two owners. Only the original borrowed-header validator may establish
  // that alias; an action or any other colliding record remains ambiguous.
  if (condition_header) {
    auto actual = read_condition_application_fact_header(
        world.common, foundation, *world.catalog->actor_foundation,
        *actual_header);
    if (!actual.ok)
      return {false, {}, actual.error};
    if (actual.value.receipt != receipt)
      return common_failure("ReadStampConditionReceiptIdentityMismatch", id);
    if (!common.actions.find(id))
      common_anchor = false;
  } else if (receipt && !actual_header) {
    auto actual = validate_condition_application_fact_for_receipt(
        world.common, foundation, *world.catalog->actor_foundation, *receipt);
    if (!actual.ok)
      return actual;
  }
  const unsigned owners = unsigned(common_anchor) + unsigned(seed != nullptr) +
                          unsigned(receipt != nullptr) +
                          unsigned(boundary != nullptr);
  if (owners != 1)
    return common_failure(owners ? "ReadStampAmbiguousActualSource"
                                 : "ReadStampActualSourceMissing",
                          id);
  if (seed) {
    auto schema =
        read_initial_condition_seed_schema(*world.catalog->actor_foundation);
    if (!schema.ok)
      return {false, {}, schema.error};
    const auto *origin =
        foundation.character_origin_records.origins.find(seed->actor_id);
    if (seed->seed_key != id || !origin || origin->actor_id != seed->actor_id ||
        origin->initial_condition_seed_ref.seed_key != id ||
        !world.common.actor(seed->actor_id))
      return common_failure("ReadStampInitialConditionIdentityMismatch", id);
    return exact_at(origin->created_at, now, id);
  }
  if (receipt) {
    const auto *head =
        foundation.character_condition_extensions.actor_condition_heads.find(
            receipt->actor_id);
    if (receipt->result_id != id || !head ||
        head->actor_id != receipt->actor_id ||
        receipt->condition_revision_before < 0 ||
        receipt->condition_revision_after <=
            receipt->condition_revision_before ||
        head->condition_revision < receipt->condition_revision_after ||
        !world.common.actor(receipt->actor_id))
      return common_failure("ReadStampConditionReceiptIdentityMismatch", id);
    return exact_at(receipt->effective_at, now, id);
  }
  if (boundary) {
    if (!exact_complete_selected(*world.catalog->exact_time) ||
        !world.catalog->growth ||
        !world.catalog->growth->complete_exact_period_sources ||
        boundary->boundary_id != id || boundary->source_schema != 1 ||
        boundary->action_ref.empty() || boundary->action_kind.empty())
      return common_failure("ReadStampGrowthBoundaryIdentityMismatch", id);
    if (boundary->owner_ref.kind == 0) {
      if (boundary->owner_ref.action_id != boundary->action_ref ||
          !world.growth.facts.find(boundary->action_ref))
        return common_failure("ReadStampGrowthBoundaryOwnerMissing", id);
    } else if (boundary->owner_ref.kind == 1) {
      if (boundary->owner_ref.life_activity_id != boundary->action_ref ||
          !world.growth.life_activities.find(boundary->action_ref))
        return common_failure("ReadStampGrowthBoundaryOwnerMissing", id);
    } else
      return common_failure("ReadStampGrowthBoundaryOwnerInvalid", id);
    if (const auto *action = world.common.action(boundary->action_ref))
      if (action->kind != boundary->action_kind)
        return common_failure("ReadStampGrowthBoundaryActionMismatch", id);
    return occurred(boundary->occurred_at, now, id);
  }
  return common_success();
}
} // namespace hanmo::resident
