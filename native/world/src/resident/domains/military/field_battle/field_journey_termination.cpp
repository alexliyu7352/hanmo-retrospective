#include "resident/domains/military/military_model.hpp"
// 只借原接敌/实际入场轨迹和段伤亡，不把战场当前坐标伪作一次旅行完成。
#include "field_contact_origin.hpp"
#include "field_journey_termination.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <tuple>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (found)
        fail("FieldJourneyDuplicateOriginalSource", id);
      found = &row;
    }
  return found;
}
bool no_later(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) <= std::tie(b.tick, b.t);
}
std::size_t position(const CommonStore &common, const SourceRef &source) {
  return take(read_actual_common_fact_position(common, source));
}
// 共同证明先核载荷；采用前与完成后只在真实 Action 存续条件上不同。
std::optional<MilitaryJourneyTermination>
original_exit(const CommonStore &common, const MilitaryState &military,
              const FieldBattleWorldState &world, const StableId &casualty_id,
              const StableId &action_id, const StableId &actor_id) {
  if (action_id.empty() || actor_id.empty() || !common.actor(actor_id))
    fail("FieldJourneyOriginalActorRequired", actor_id);
  const auto *receipt = unique(military.casualty_facts, casualty_id);
  if (!receipt)
    fail("FieldJourneyCasualtySourceRequired", casualty_id);
  const auto source = take(read_field_segment_settlement_source(
      common, military, world,
      {"military", "field_battle_segment", receipt->battle_id}));
  if (source.casualties != receipt)
    fail("FieldJourneyCasualtySourceRequired", casualty_id);
  const MilitaryActorChange *change = nullptr;
  for (const auto &row : receipt->actor_changes)
    if (row.actor_id == actor_id) {
      if (change)
        fail("FieldJourneyActorChangeAmbiguous", casualty_id);
      change = &row;
    }
  if (!change || (change->status != "dead" && change->status != "captured" &&
                  change->status != "deserted"))
    return std::nullopt;
  const auto cut = position(
      common, {"military", "field_battle_segment", receipt->battle_id});
  std::optional<StableId> original_action;
  auto accept = [&](const auto &endpoint) {
    if (endpoint.actor_id != actor_id)
      return;
    if (endpoint.trace.actor_id != actor_id || original_action)
      fail("FieldJourneyMotionSourceAmbiguous", casualty_id);
    // 空行动是真正静止端点，不给它凭空生成一次完成；moving=false
    // 不等于没有原行动。
    original_action = endpoint.trace.action_id;
  };
  for (const auto &[key, contact] : world.contacts) {
    if (contact.battle_id != source.session->battle_id)
      continue;
    if (key != contact.contact_id ||
        position(common, {"military", "field_battle_contact", key}) >= cut)
      fail("FieldJourneyContactSourceRequired", casualty_id);
    auto valid_origin = validate_field_contact_origin_head(common, contact);
    if (!valid_origin.ok)
      throw Failure{valid_origin.error};
    const auto legacy = std::get_if<FieldRoadContact>(&contact.source);
    // Exact group execution owns no per-commander Common travel Action. Its
    // member handoff stays with the original S1 group source, not this legacy
    // action termination adapter.
    if (!legacy)
      continue;
    for (Integer side = 0; side < 2; ++side)
      if (legacy->army_ids[side] == change->army_id)
        accept(side == 0 ? legacy->contact.road->first
                         : legacy->contact.road->second);
  }
  for (const auto &[key, entry] : world.boundary_contacts) {
    if (entry.battle_id != source.session->battle_id ||
        entry.army_id != change->army_id)
      continue;
    // 穿边停止不等于已参战；只有原应用头先于本次段提交才能成为来源。
    if (!entry.entry_application_source || !entry.entered_at)
      continue;
    const auto entered = position(common, *entry.entry_application_source);
    // 后来才入场的同军来源不改写旧段的领队行动。
    if (entered >= cut)
      continue;
    if (key != entry.contact_id || !entry.entered_observed_tick ||
        (entry.status != FieldBoundaryStatus::Entered &&
         entry.status != FieldBoundaryStatus::Released) ||
        !entry.side || *entry.side < 0 || *entry.side > 1 ||
        *entry.entered_observed_tick < source.session->created_tick ||
        *entry.entered_observed_tick > receipt->tick ||
        !no_later(*entry.entered_at, source.settlement->settled_at) ||
        entry.entry_application_source->domain != "military" ||
        entry.entry_application_source->kind != "field_battle_entered")
      fail("FieldJourneyEntrySourceRequired", casualty_id);
    if (position(common, {"military", "field_battle_contact", key}) >= entered)
      fail("FieldJourneyEntrySourceRequired", casualty_id);
    std::visit(
        [&](const auto &endpoint) {
          using T = std::decay_t<decltype(endpoint)>;
          if constexpr (!std::is_same_v<T, FieldExactBoundaryOrigin>) {
            if (endpoint.actor_id == actor_id &&
                std::count(entry.participant_actor_ids.begin(),
                           entry.participant_actor_ids.end(), actor_id) != 1)
              fail("FieldJourneyEntryActorRequired", casualty_id);
            accept(endpoint);
          }
          // Exact group has no commander legacy action. Its actual group
          // member handoff stays with S1, as for initial exact contacts.
        },
        entry.motion);
  }
  if (!original_action || original_action->empty() ||
      *original_action != action_id)
    return std::nullopt;
  return MilitaryJourneyTermination{
      action_id,
      actor_id,
      source.session->battle_id,
      casualty_id,
      change->status == "dead"       ? "MilitaryCommanderKilled"
      : change->status == "captured" ? "MilitaryCommanderCaptured"
                                     : "MilitaryCommanderDeserted",
      receipt->tick};
}
} // namespace
Outcome<std::optional<MilitaryJourneyTermination>>
preview_field_segment_journey_termination(const CommonStore &common,
                                          const MilitaryState &military,
                                          const FieldBattleWorldState &world,
                                          const StableId &casualty_id,
                                          const StableId &action_id,
                                          const StableId &actor_id) {
  try {
    auto source = original_exit(common, military, world, casualty_id, action_id,
                                actor_id);
    if (!source)
      return {true, std::nullopt, {}};
    const auto *actor = common.actor(actor_id);
    const auto *action = common.action(action_id);
    if (!actor->alive || !actor->available || actor->action_id != action_id ||
        !action || action->actor_id != actor_id || action->kind != "travel")
      fail("FieldJourneyOriginalActionRequired", action_id);
    return {true, std::move(source), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<std::optional<MilitaryJourneyTermination>>
query_field_segment_journey_termination(const CommonStore &common,
                                        const MilitaryState &military,
                                        const FieldBattleWorldState &world,
                                        const StableId &casualty_id,
                                        const StableId &action_id,
                                        const StableId &actor_id) {
  try {
    auto source = original_exit(common, military, world, casualty_id, action_id,
                                actor_id);
    if (!source)
      return {true, std::nullopt, {}};
    const auto *actor = common.actor(actor_id);
    // 原俘获/逃散者后来发生其它结果不抹掉这次真实退出；只有死亡本身不可逆。
    if (common.action(action_id) ||
        (source->reason == "MilitaryCommanderKilled" &&
         (actor->alive || actor->available || actor->hp != 0 ||
          !actor->action_id.empty())))
      fail("FieldJourneyOriginalOutcomeRequired", action_id);
    return {true, std::move(source), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
} // namespace hanmo::resident
