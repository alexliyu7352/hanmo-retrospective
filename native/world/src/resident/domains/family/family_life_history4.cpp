#include "family_life_history4.hpp"
#include <algorithm>
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
namespace {
Status original_boundary(const CommonStore &common, const SourceRef &ref,
                         Integer tick, const WorldMoment &now) {
  const auto *header = common.state().facts.find(ref.id);
  const auto &order = common.state().facts.order();
  if (tick < 0 || tick > now.half_day_tick || ref.id.empty() || !header ||
      header->id != ref.id || header->domain != ref.domain ||
      header->kind != ref.kind ||
      std::find(order.begin(), order.end(), ref.id) == order.end())
    return common_failure("FamilyLifeOriginalHistoryBoundaryRequired", ref.id,
                          ErrorKind::internal);
  return common_success();
}
// 唯一原 append-cut 判据；旧 tick 投影保留，exact 新口只加强实际 M 一致性。
Outcome<bool> visible_at_cut(const CommonStore &common, const StableId &actor,
                             const StableId &death_id,
                             const WorldMoment &recorded, Integer tick,
                             std::size_t limit,
                             const WorldMoment *exact = nullptr) {
  SourceRefBuildView ref{"family_life", "family_death", death_id};
  auto position = read_actual_common_fact_position(common, ref);
  if (!position.ok)
    return {false, false, position.error};
  if (limit > common.state().facts.size())
    return {false, false,
            common_failure("FamilyLifeOriginalHistoryBoundaryRequired", actor,
                           ErrorKind::internal)
                .error};
  int order = recorded.half_day_tick < tick   ? -1
              : recorded.half_day_tick > tick ? 1
                                              : 0;
  if (exact) {
    auto cmp = compare_world_moments(recorded, *exact);
    if (!cmp.ok)
      return {false, false, cmp.error};
    order = cmp.value;
  }
  if (position.value >= limit) {
    if (order < 0)
      return {false, false,
              common_failure("FamilyLifeOriginalHistoryTimeMismatch", actor,
                             ErrorKind::internal)
                  .error};
    return {true, false, {}};
  }
  if (order > 0)
    return {false, false,
            common_failure("FamilyLifeOriginalHistoryTimeMismatch", actor,
                           ErrorKind::internal)
                .error};
  return {true, true, {}};
}
struct DeathArgument {
  CheckpointMemoryCharge memory;
  DeathRef value;
  explicit DeathArgument(const StableId &id) {


    memory = CheckpointMemoryCharge(0);
    value = {checkpoint_fresh_text_copy("family_life"),
             checkpoint_fresh_text_copy("family_death"),
             checkpoint_fresh_string_copy(id)};
  }
};
bool before(const CommonStore &common, const StableId &a, const StableId &b) {
  const auto &order = common.state().facts.order();
  const auto x = std::find(order.begin(), order.end(), a),
             y = std::find(order.begin(), order.end(), b);
  return x != order.end() && y != order.end() && x < y;
}
} // namespace
Status FamilyLifeHistoryRead4::selected() const {
  if (!policy_.enabled || policy_.selected_version != 4)
    return common_failure("FamilyLife4SelectionRequired", {},
                          ErrorKind::unsupported_capability);
  if (catalog_.content_digest != common_.state().metadata.content_digest)
    return common_failure("FamilyLifeHistoryCatalogMismatch", {},
                          ErrorKind::internal);
  return validate_family_death_policy_selection(policy_, catalog_);
}
Outcome<WorldMoment> FamilyLifeHistoryRead4::read_current_moment() const {
  auto valid = selected();
  if (!valid.ok)
    return {false, {}, valid.error};
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return at;
  if (at.value.half_day_tick < common_.tick())
    return {false,
            {},
            common_failure("FamilyLifeHistoryClockBeforeWorld", {},
                           ErrorKind::internal)
                .error};
  return at;
}
Outcome<std::optional<VerifiedDeathView>>
FamilyLifeHistoryRead4::read_actor_death_before_original_fact(
    const StableId &actor, Integer tick, const SourceRef &boundary) const {
  auto at = read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto valid = original_boundary(common_, boundary, tick, at.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto position = read_actual_common_fact_position(common_, boundary);
  if (!position.ok)
    return {false, {}, position.error};
  return death_at_cut(actor, tick, position.value);
}
Outcome<std::optional<VerifiedDeathView>>
FamilyLifeHistoryRead4::death_at_cut(const StableId &actor, Integer tick,
                                     std::size_t limit) const {
  auto actual = read_actor_death(actor);
  if (!actual.ok || !actual.value)
    return actual;
  const auto &death = *actual.value;
  if (death.recorded_at.kind != ActorFoundationAt::Kind::exact_moment)
    return {false,
            {},
            common_failure("FamilyLife4ExactDeathRequired", death.death_ref.id,
                           ErrorKind::internal)
                .error};
  auto visible = visible_at_cut(common_, actor, death.death_ref.id,
                                death.recorded_at.exact_moment, tick, limit);
  if (!visible.ok)
    return {false, {}, visible.error};
  if (!visible.value)
    return {true, std::nullopt, {}};
  return actual;
}
Outcome<std::optional<VerifiedDeathView>>
FamilyLifeHistoryRead4::read_actor_death_at_original_prefix(
    const StableId &actor, Integer tick,
    const CommonActualFactPrefix &prefix) const {
  auto now = read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto cut = read_actual_common_fact_prefix(common_, prefix);
  if (!cut.ok)
    return {false, {}, cut.error};
  if (tick < 0 || tick > now.value.half_day_tick)
    return {false,
            {},
            common_failure("FamilyLifeOriginalHistoryTimeMismatch", actor,
                           ErrorKind::internal)
                .error};
  return death_at_cut(actor, tick, cut.value);
}

Status FamilyLifeHistoryRead4::validate() const {
  auto valid = selected();
  if (!valid.ok)
    return valid;
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  if (at.value.half_day_tick < common_.tick())
    return common_failure("FamilyLifeHistoryClockBeforeWorld", {},
                          ErrorKind::internal);
  return validate_family_life4(common_, deaths_, sources_, at.value);
}
Outcome<const FamilyDeathRecord *>
FamilyLifeHistoryRead4::read_actor_death_record(
    const StableId &actor, const StableId *&actual_key) const {
  actual_key = nullptr;
  auto valid = selected();
  if (!valid.ok)
    return {false, {}, valid.error};
  if (!common_.actor(actor))
    return {false, {}, common_failure("UnknownActor", actor).error};
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  if (at.value.half_day_tick < common_.tick())
    return {false,
            {},
            common_failure("FamilyLifeHistoryClockBeforeWorld", {},
                           ErrorKind::internal)
                .error};
  const StableId *id = nullptr;
  for (const auto &row : deaths_.deaths.rows())
    if (row.second.actor_id == actor) {
      if (id)
        return {
            false,
            {},
            common_failure("ConflictingDeathSource", actor, ErrorKind::internal)
                .error};
      id = &row.first;
    }
  if (!id)
    return {true, nullptr, {}};
  actual_key = id;
  DeathArgument ref(*id);
  return read_verified_death4_record(common_, deaths_, sources_, ref.value,
                                     at.value);
}
Outcome<std::optional<VerifiedDeathView>>
FamilyLifeHistoryRead4::read_actor_death(const StableId &actor) const {
  const StableId *actual_key = nullptr;
  auto record = read_actor_death_record(actor, actual_key);
  if (!record.ok)
    return {false, {}, record.error};
  if (!record.value)
    return {true, std::nullopt, {}};
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  DeathArgument ref(*actual_key);
  auto death =
      read_verified_death4(common_, deaths_, sources_, ref.value, at.value);
  if (!death.ok)
    return {false, {}, death.error};
  return {true, std::move(death.value), {}};
}
Outcome<const FamilyDeathRecord *>
FamilyLifeHistoryRead4::read_actor_death_record_at_original_prefix(
    const StableId &actor, const WorldMoment &at,
    const CommonActualFactPrefix &prefix) const {
  auto now = read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto time = compare_world_moments(at, now.value);
  if (!time.ok)
    return {false, {}, time.error};
  if (at.half_day_tick < 0 || time.value > 0)
    return {false,
            {},
            common_failure("FamilyLifeOriginalHistoryTimeMismatch", actor,
                           ErrorKind::internal)
                .error};
  auto cut = read_actual_common_fact_prefix(common_, prefix);
  if (!cut.ok)
    return {false, {}, cut.error};
  const StableId *actual_key = nullptr;
  auto record = read_actor_death_record(actor, actual_key);
  if (!record.ok)
    return record;
  if (!record.value) {
    // 无原死亡行时不能把今日已死当历史无死；这里只拒缺源，不由今日态推时间。
    if (!common_.actor(actor)->alive)
      return {false,
              {},
              common_failure("FamilyLifeHistoricalAliveSourceRequired", actor,
                             ErrorKind::unsupported_capability)
                  .error};
    return record;
  }
  const auto &death = *record.value;
  if (!death.precise)
    return {false,
            {},
            common_failure("FamilyLife4ExactDeathRequired", actor,
                           ErrorKind::internal)
                .error};
  auto visible =
      visible_at_cut(common_, actor, *actual_key, death.precise->recorded_at,
                     at.half_day_tick, cut.value, &at);
  if (!visible.ok)
    return {false, {}, visible.error};
  return {true, visible.value ? record.value : nullptr, {}};
}

Status FamilyLifeHistoryRead4::require_no_death_before_original_fact(
    const StableId &actor, Integer tick, const SourceRef &boundary) const {
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto valid = original_boundary(common_, boundary, tick, at.value);
  if (!valid.ok)
    return valid;
  auto death = read_actor_death(actor);
  if (!death.ok)
    return {false, {}, death.error};
  if (!death.value)
    return common_.actor(actor)->alive
               ? common_success()
               : common_failure("FamilyLifeHistoricalAliveSourceRequired",
                                actor, ErrorKind::unsupported_capability);
  const auto &actual = *death.value;
  if (actual.effective_at.kind != ActorFoundationAt::Kind::exact_moment)
    return common_failure("FamilyLife4ExactDeathRequired", actual.death_ref.id,
                          ErrorKind::internal);
  if (tick > actual.effective_at.exact_moment.half_day_tick ||
      !before(common_, boundary.id, actual.cause_ref.id))
    return common_failure("FamilyDecisionAfterVerifiedDeath", boundary.id);
  return common_success();
}
Status FamilyLifeHistoryRead4::require_death_before_original_fact(
    const DeathRef &ref, const StableId &actor, Integer tick,
    const SourceRef &boundary) const {
  auto at = clock_.read_current_moment();
  if (!at.ok)
    return {false, {}, at.error};
  auto valid = original_boundary(common_, boundary, tick, at.value);
  if (!valid.ok)
    return valid;
  auto death = read_actor_death(actor);
  if (!death.ok)
    return {false, {}, death.error};
  if (!death.value || ref.domain != death.value->death_ref.domain ||
      ref.kind != death.value->death_ref.kind ||
      ref.id != death.value->death_ref.id)
    return common_failure("FamilyOriginalDeathReferenceMismatch", ref.id,
                          ErrorKind::internal);
  const auto &actual = *death.value;
  if (actual.recorded_at.kind != ActorFoundationAt::Kind::exact_moment ||
      tick < actual.recorded_at.exact_moment.half_day_tick ||
      !before(common_, actual.death_ref.id, boundary.id))
    return common_failure("FamilyObligationBeforeVerifiedDeath", boundary.id,
                          ErrorKind::internal);
  return common_success();
}
} // namespace hanmo::resident
