// 原战斗叶只读身份/参与者/真实落账M；不把来源读取冒作人物写者或BW06 driver。
#include "injury_sources_v1.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/field_battle/field_segment_settlement.hpp"
#include "resident/domains/personal/personal_battle_lifecycle.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <charconv>
#include <string_view>
namespace hanmo::resident {
namespace {
struct Failure {
  RuleError error;
};
[[noreturn]] void fail(const char *code, const StableId &id = {}) {
  throw Failure{common_failure(code, id).error};
}
template <class T> T take(Outcome<T> value) {
  if (!value.ok)
    throw Failure{std::move(value.error)};
  return std::move(value.value);
}
template <class F> Outcome<CharacterBattleInjuryReadV1> guarded(F fn) {
  try {
    return {true, fn(), {}};
  } catch (Failure &e) {
    return {false, {}, std::move(e.error)};
  }
}
Integer allocated_serial(const CommonStore &common, const StableId &id,
                         std::string_view prefix) {
  const std::string_view value(id);
  if (value.substr(0, prefix.size()) != prefix)
    return -1;
  const auto digits = value.substr(prefix.size());
  if (digits.empty() || digits.front() == '0')
    return -1;
  Integer number = 0;
  const auto result =
      std::from_chars(digits.data(), digits.data() + digits.size(), number);
  return result.ec == std::errc{} &&
                 result.ptr == digits.data() + digits.size() && number > 0 &&
                 number < common.state().next_instance_id
             ? number
             : -1;
}
FieldSegmentSettlementSourceRead
segment_source(const CommonStore &common, const MilitaryState &military,
               const FieldBattleWorldState &field, const StableId &id,
               const FieldSegmentApplicationRead *actual_clock) {


  auto memory = CheckpointMemoryCharge(0);
  // This temporary is destroyed before its real first-copy reservation.
  const SourceRef original{"military", "field_battle_segment", id};
  return take(read_field_segment_settlement_source(common, military, field,
                                                   original, actual_clock));
}
void past(const WorldMoment &at, const WorldMoment &now, const StableId &id) {
  take(compare_world_moments(at, at));
  if (take(compare_world_moments(at, now)) > 0)
    fail("CharacterInjuryFutureBattleSource", id);
}
} // namespace
Outcome<CharacterBattleInjuryReadV1> read_personal_battle_injury_source_v1(
    const CommonStore &common, const PersonalState &personal,
    const PersonalCatalog &catalog, Integer selected_lifecycle,
    const CharacterBattleReceiptSourceV1 &source, const StableId &actor,
    const WorldMoment &now) {
  return guarded([&] {
    const auto &ref = source.original_receipt;
    if (source.source_schema != 1 || ref.domain != "personal" ||
        ref.kind != "personal_battle" || !catalog.enabled ||
        selected_lifecycle != 1 || !common.actor(actor))
      fail("CharacterInjuryPersonalOriginalSourceRequired", ref.id);
    take(validate_personal_battle_lifecycle_sources(common, personal,
                                                    selected_lifecycle, now));
    const auto *battle = personal.battles.find(ref.id);
    const auto *fact = personal.facts.find(ref.id);
    const auto *header = common.state().facts.find(ref.id);
    if (!battle || !fact || !header || header->id != ref.id ||
        header->domain != ref.domain || header->kind != ref.kind ||
        allocated_serial(common, ref.id, "battle:") < 1 ||
        allocated_serial(common, fact->result_id, "result:") <=
            allocated_serial(common, ref.id, "battle:") ||
        battle->id != ref.id || fact->battle_id != ref.id ||
        battle->result_id != fact->result_id || !battle->applied ||
        (battle->phase != "result" && battle->phase != "closed") ||
        battle->completed_tick != fact->tick || fact->tick < 0 ||
        fact->tick > common.tick() || !battle->lifecycle ||
        !battle->lifecycle->result_applied_at)
      fail("CharacterInjuryPersonalReceiptMismatch", ref.id);
    for (const auto &other : personal.facts.rows())
      if (other.first != ref.id && other.second.result_id == fact->result_id)
        fail("CharacterInjuryRepeatedPersonalResult", fact->result_id);
    const auto *person = common.state().actors.find(actor);
    if (!person || (person->created_tick && *person->created_tick > fact->tick))
      fail("CharacterInjuryPersonalActorBeforeOrigin", ref.id);
    const auto *unit = battle->units.find(actor);
    const auto *objective = catalog.objectives.find(battle->objective_id);
    if (!unit || unit->hp < 0 || !objective ||
        objective->injury_on_defeat < 0 || objective->injury_on_defeat > 3 ||
        std::count(fact->participants.begin(), fact->participants.end(),
                   actor) != 1 ||
        std::count(fact->defeated.begin(), fact->defeated.end(), actor) !=
            (unit->hp == 0 ? 1 : 0))
      fail("CharacterInjuryPersonalParticipantResultRequired", ref.id);
    past(*battle->lifecycle->result_applied_at, now, ref.id);
    // Original Personal writes hp/fatigue for every participant. An unwounded
    // participant still needs that genuine Condition coverage, but incoming0
    // must preserve injury/points and must never invoke the positive rehit
    // rule.
    const Integer incoming = unit->hp == 0 ? objective->injury_on_defeat : 0;
    return CharacterBattleInjuryReadV1{&*battle->lifecycle->result_applied_at,
                                       incoming, fact, nullptr};
  });
}
Outcome<CharacterBattleInjuryReadV1> read_field_battle_injury_source_v1(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &field,
    const CharacterBattleReceiptSourceV1 &source, const StableId &actor,
    const WorldMoment &now, const FieldSegmentApplicationRead *actual_clock) {
  return guarded([&] {
    const auto &ref = source.original_receipt;
    if (source.source_schema != 1 || ref.domain != "military" ||
        ref.kind != "military_casualties" || field.schema != 1 ||
        !common.actor(actor) ||
        (!actual_clock && now.half_day_tick != common.tick()))
      fail("CharacterInjuryFieldOriginalSourceRequired", ref.id);
    if (actual_clock) {
      if (!actual_clock->bound_to(common, military, field))
        fail("CharacterInjuryFieldClockBindingRequired", ref.id);
      const auto actual = take(actual_clock->read_current_moment());
      if (take(compare_world_moments(actual, now)) != 0)
        fail("CharacterInjuryFieldActualMomentMismatch", ref.id);
    }
    const MilitaryCasualtyFact *casualty = nullptr;
    for (const auto &fact : military.casualty_facts)
      if (fact.id == ref.id) {
        if (casualty)
          fail("CharacterInjuryDuplicateCasualtyReceipt", ref.id);
        casualty = &fact;
      }
    if (!casualty)
      fail("CharacterInjuryFieldCasualtyRequired", ref.id);
    const auto original = segment_source(common, military, field,
                                         casualty->battle_id, actual_clock);
    if (original.casualties != casualty || !original.session ||
        !original.settlement)
      fail("CharacterInjuryFieldSegmentOwnerMismatch", ref.id);
    past(original.settlement->applied_at, now, ref.id);
    const MilitaryActorChange *participant = nullptr;
    for (const auto &change : casualty->actor_changes)
      if (change.actor_id == actor) {
        if (participant)
          fail("CharacterInjuryRepeatedFieldParticipant", ref.id);
        participant = &change;
      }
    if (!participant || participant->status != "wounded")
      fail("CharacterInjuryFieldWoundRequired", ref.id);
    // 10-27 定案：每段真伤亡回执＋参与者唯一，M 取本段 applied_at；
    // 不等待终局，不汇总别段，不将 dead 作为伤势加点的许可。
    return CharacterBattleInjuryReadV1{&original.settlement->applied_at, 1,
                                       nullptr, casualty};
  });
}
} // namespace hanmo::resident
