// 段事实只连接原 Field 会话和原军务回执；此叶不写人物、人口或第二个世界钟。
#include "field_segment_settlement.hpp"
#include "field_binding_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
// 原 Common 顺序是唯一因果界；ID 字面次序不能代替实际追加位置。
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
template <class T, class F> Outcome<T> guarded(F &&body) {
  try {
    return {true, body(), {}};
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
template <class T>
const T *unique(const std::vector<T> &rows, const StableId &id) {
  const T *found = nullptr;
  for (const auto &row : rows)
    if (row.id == id) {
      if (found)
        fail("FieldSegmentDuplicateOriginalPayload", id);
      found = &row;
    }
  return found;
}
bool before_or_same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) <= std::tie(b.tick, b.t);
}
bool same(const FieldBattleTime &a, const FieldBattleTime &b) {
  return a.tick == b.tick && a.t == b.t;
}
std::size_t head_position(const CommonStore &common, const StableId &id,
                          const char *kind) {
  const auto *head = common.state().facts.find(id);
  if (!head || head->id != id || head->domain != "military" ||
      head->kind != kind)
    fail("FieldSegmentOriginalHeaderRequired", id);
  return take(read_actual_common_fact_position(common, *head));
}
// 历史回执保自己的完整名单；不拿后来编队的伤亡分类覆盖过去。
void roster_shape(const FieldBattleSession &session,
                  const FieldSegmentSettlement &settlement,
                  const MilitaryCasualtyFact &receipt) {
  std::set<StableId> units, members, changes;
  for (const auto &row : receipt.units) {
    const auto formation = session.formations.find(row.unit_id);
    if (formation == session.formations.end() ||
        !units.insert(row.unit_id).second ||
        !before_or_same(formation->second.joined_at, settlement.settled_at) ||
        row.morale < 0 || row.morale > 100)
      fail("FieldSegmentRosterSourceMismatch", receipt.id);
    std::set<StableId> partition;
    for (const auto *group :
         {&row.fit, &row.wounded, &row.dead, &row.captured, &row.deserted})
      for (const auto &actor : *group)
        if (actor.empty() || !partition.insert(actor).second ||
            !members.insert(actor).second)
          fail("FieldSegmentRosterPartitionMismatch", receipt.id);
    const std::set<StableId> original(row.member_ids.begin(),
                                      row.member_ids.end());
    if (original.size() != row.member_ids.size() || original != partition)
      fail("FieldSegmentRosterPartitionMismatch", receipt.id);
    std::set<StableId> retained;
    const auto &unit = formation->second;
    for (const auto *group :
         {&unit.fit, &unit.wounded, &unit.dead, &unit.captured, &unit.deserted})
      for (const auto &actor : *group)
        if (!retained.insert(actor).second)
          fail("FieldSegmentRetainedRosterMismatch", row.unit_id);
    if (retained != original)
      fail("FieldSegmentRetainedRosterMismatch", row.unit_id);
    std::set<StableId> captives;
    for (const auto &[actor, keeper] : row.captors) {
      if (keeper.empty() || !captives.insert(actor).second ||
          std::find(row.captured.begin(), row.captured.end(), actor) ==
              row.captured.end())
        fail("FieldSegmentCaptorSourceMismatch", receipt.id);
      // 俘获方必须同在这张真实段回执中，不能借后来才入场的敌方编队倒填。
      bool enemy = false;
      for (const auto &other_row : receipt.units) {
        const auto other = session.formations.find(other_row.unit_id);
        if (other != session.formations.end() &&
            other->second.army_id == keeper &&
            other->second.side != unit.side &&
            before_or_same(other->second.joined_at, settlement.settled_at))
          enemy = true;
      }
      if (!enemy)
        fail("FieldSegmentCaptorSourceMismatch", receipt.id);
    }
    if (captives !=
        std::set<StableId>(row.captured.begin(), row.captured.end()))
      fail("FieldSegmentCaptorSourceMismatch", receipt.id);
  }
  if (units.empty())
    fail("FieldSegmentRosterSourceRequired", receipt.id);
  for (const auto &change : receipt.actor_changes) {
    if (!changes.insert(change.actor_id).second)
      fail("FieldSegmentDuplicateActorChange", receipt.id);
    const FieldFormation *owner = nullptr;
    for (const auto &row : receipt.units) {
      const auto &formation = session.formations.at(row.unit_id);
      const bool enlisted =
          std::find(row.member_ids.begin(), row.member_ids.end(),
                    change.actor_id) != row.member_ids.end();
      const bool officer = formation.commander_actor_id == change.actor_id ||
                           formation.deputy_actor_id == change.actor_id;
      if (!enlisted && !officer)
        continue;
      if (owner || formation.army_id != change.army_id)
        fail("FieldSegmentActorOwnerMismatch", receipt.id);
      owner = &formation;
      if (enlisted) {
        const auto *status = change.status == "dead"       ? &row.dead
                             : change.status == "wounded"  ? &row.wounded
                             : change.status == "captured" ? &row.captured
                             : change.status == "deserted" ? &row.deserted
                                                           : nullptr;
        if (!status || std::find(status->begin(), status->end(),
                                 change.actor_id) == status->end())
          fail("FieldSegmentActorStatusMismatch", receipt.id);
        if (change.status == "captured") {
          if (std::find(row.captors.begin(), row.captors.end(),
                        std::pair<StableId, StableId>{change.actor_id,
                                                      change.captor_army_id}) ==
              row.captors.end())
            fail("FieldSegmentActorCaptorMismatch", receipt.id);
        } else if (!change.captor_army_id.empty()) {
          fail("FieldSegmentActorCaptorMismatch", receipt.id);
        }
      } else {
        if ((change.status != "dead" && change.status != "wounded") ||
            !change.captor_army_id.empty())
          fail("FieldSegmentOfficerStatusMismatch", receipt.id);
        const Integer expected = change.status == "dead" ? 2 : 1;
        bool actual_loss = false;
        for (const auto &loss : formation.commander_casualties)
          if (loss.actor_id == change.actor_id && loss.loss == expected &&
              before_or_same(loss.at, settlement.settled_at))
            actual_loss = true;
        if (!actual_loss)
          fail("FieldSegmentOfficerSourceRequired", receipt.id);
      }
    }
    if (!owner)
      fail("FieldSegmentActorOwnerRequired", receipt.id);
  }
}
} // namespace
Outcome<FieldSegmentSettlementSourceRead> read_field_segment_settlement_source(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const SourceRef &source,
    const FieldSegmentApplicationRead *actual_clock) {
  return guarded<FieldSegmentSettlementSourceRead>([&] {
    if (source.domain != "military" || source.kind != "field_battle_segment" ||
        source.id.empty())
      fail("FieldSegmentSourceRequired", source.id);
    Integer readable_tick = common.tick();
    std::optional<WorldMoment> actual_now;
    if (actual_clock) {
      if (!actual_clock->bound_to(common, military, world))
        fail("CrossCandidateFieldSegmentApplicationClock", source.id);
      actual_now = take(actual_clock->read_current_moment());
      if (actual_now->half_day_tick > common.tick()) {
        const auto closing =
            take(actual_clock->read_field_window_closing_moment());
        if (take(compare_world_moments(closing, *actual_now)) != 0)
          fail("FieldSegmentActualClosingProofRequired", source.id);
      } else if (actual_now->half_day_tick != common.tick()) {
        fail("FieldSegmentApplicationClockMismatch", source.id);
      }
      readable_tick = actual_now->half_day_tick;
    }
    const auto parent_position =
        head_position(common, source.id, "field_battle_segment");
    const FieldBattleSession *session = nullptr;
    const FieldSegmentSettlement *settlement = nullptr;
    for (const auto &[id, value] : world.sessions) {
      if (id != value.battle_id)
        fail("FieldSegmentSessionKeyMismatch", id);
      for (const auto &row : value.settlements)
        if (row.fact_id == source.id) {
          if (settlement)
            fail("FieldSegmentDuplicateOwner", source.id);
          session = &value;
          settlement = &row;
        }
    }
    if (!session || !settlement || settlement->segment < 0 ||
        settlement->segment > session->segment ||
        settlement->recorded_tick < session->created_tick ||
        settlement->recorded_tick > readable_tick ||
        settlement->settled_at.tick < 0 || settlement->settled_at.t < 0 ||
        !before_or_same(settlement->settled_at, session->now) ||
        settlement->casualty_fact_ids.size() != 1 ||
        head_position(common, session->battle_id, "field_battle_session") >=
            parent_position)
      fail("FieldSegmentOriginalOwnerRequired", source.id);
    std::size_t ordinal = 0;
    if (settlement->applied_at.half_day_tick != settlement->recorded_tick)
      fail("FieldSegmentApplicationClockMismatch", source.id);
    take(compare_world_moments(settlement->applied_at, settlement->applied_at));
    if (actual_now &&
        take(compare_world_moments(settlement->applied_at, *actual_now)) > 0)
      fail("FieldSegmentFutureApplicationSource", source.id);
    const FieldSegmentSettlement *previous = nullptr;
    for (const auto &row : session->settlements) {
      if (row.segment != static_cast<Integer>(ordinal++) ||
          row.casualty_fact_ids.size() != 1 ||
          (previous &&
           (previous->battle_fact_id ||
            previous->recorded_tick > row.recorded_tick ||
            take(compare_world_moments(previous->applied_at, row.applied_at)) >
                0 ||
            !before_or_same(previous->settled_at, row.settled_at) ||
            head_position(common, previous->fact_id, "field_battle_segment") >=
                head_position(common, row.fact_id, "field_battle_segment") ||
            head_position(common, previous->casualty_fact_ids.front(),
                          "military_casualties") >=
                head_position(common, row.fact_id, "field_battle_segment"))))
        fail("FieldSegmentSequenceMismatch", session->battle_id);
      previous = &row;
    }
    const auto *casualties =
        unique(military.casualty_facts, settlement->casualty_fact_ids.front());
    if (!casualties || casualties->battle_id != source.id ||
        casualties->tick != settlement->recorded_tick ||
        head_position(common, casualties->id, "military_casualties") <=
            parent_position)
      fail("FieldSegmentCasualtySourceRequired", source.id);
    for (const auto &row : military.casualty_facts)
      if (row.battle_id == source.id && &row != casualties)
        fail("FieldSegmentDuplicateCasualty", source.id);
    roster_shape(*session, *settlement, *casualties);
    // 已经写到真实终局的段不能只删终局回执引用而伪装成非终局段。
    if (session->state == FieldBattleState::Ended && session->result &&
        same(settlement->settled_at, session->result->ended) &&
        !settlement->battle_fact_id)
      fail("FieldSegmentTerminalSourceRequired", source.id);
    const MilitaryBattleFact *terminal = nullptr;
    if (settlement->battle_fact_id) {
      terminal = unique(military.battle_facts, *settlement->battle_fact_id);
      if (!terminal || !session->result ||
          session->state != FieldBattleState::Ended ||
          settlement != &session->settlements.back() ||
          !same(settlement->settled_at, session->result->ended) ||
          terminal->source_id != source.id ||
          terminal->session_id != session->battle_id ||
          terminal->phase != "completed" || !terminal->final ||
          terminal->tick != settlement->recorded_tick ||
          terminal->round != settlement->segment + 1 || !terminal->result ||
          terminal->result->winner_side != session->result->winner_side ||
          terminal->winner_side != session->result->winner_side ||
          terminal->result->reason != session->result->reason ||
          head_position(common, terminal->id, "military_battle") <=
              head_position(common, casualties->id, "military_casualties"))
        fail("FieldSegmentTerminalSourceRequired", source.id);
      std::set<StableId> units, armies;
      for (const auto &row : casualties->units) {
        units.insert(row.unit_id);
        armies.insert(session->formations.at(row.unit_id).army_id);
      }
      if (terminal->start_tick != session->created_tick ||
          units != std::set<StableId>(terminal->unit_ids.begin(),
                                      terminal->unit_ids.end()) ||
          units.size() != terminal->unit_ids.size() ||
          armies != std::set<StableId>(terminal->army_ids.begin(),
                                       terminal->army_ids.end()) ||
          armies.size() != terminal->army_ids.size())
        fail("FieldSegmentTerminalRosterMismatch", source.id);
      const FieldBattleContactRecord *contact = nullptr;
      for (const auto &[id, row] : world.contacts)
        if (row.battle_id == session->battle_id) {
          if (contact || id != row.contact_id)
            fail("FieldSegmentContactSourceMismatch", source.id);
          contact = &row;
        }
      if (!contact || terminal->contact_source_id != contact->contact_id ||
          head_position(common, contact->contact_id, "field_battle_contact") >=
              parent_position)
        fail("FieldSegmentContactSourceRequired", source.id);
    }
    return FieldSegmentSettlementSourceRead{session, settlement, casualties,
                                            terminal};
  });
}
// 从唯一原伤亡回执追到实际段；错误身份、未来来源或非死亡变化均拒绝。
Outcome<FieldSegmentFatalSourceRead> read_field_segment_fatal_source(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const SourceRef &casualty,
    const StableId &actor_id, const WorldMoment &actual_now,
    const FieldSegmentApplicationRead *actual_clock) {
  return guarded<FieldSegmentFatalSourceRead>([&] {
    if (casualty.domain != "military" ||
        casualty.kind != "military_casualties" || casualty.id.empty() ||
        actor_id.empty() || !common.actor(actor_id))
      fail("FieldSegmentFatalSourceRequired", casualty.id);
    const auto *receipt = unique(military.casualty_facts, casualty.id);
    if (!receipt)
      fail("FieldSegmentFatalSourceRequired", casualty.id);


    auto parent_memory = CheckpointMemoryCharge(0);
    const SourceRef parent{checkpoint_fresh_text_copy("military"),
                           checkpoint_fresh_text_copy("field_battle_segment"),
                           checkpoint_fresh_string_copy(receipt->battle_id)};
    const auto source = take(read_field_segment_settlement_source(
        common, military, world, parent, actual_clock));
    if (source.casualties != receipt)
      fail("FieldSegmentFatalSourceRequired", casualty.id);
    if ((!actual_clock && actual_now.half_day_tick != common.tick()) ||
        (actual_clock &&
         take(compare_world_moments(
             actual_now, take(actual_clock->read_current_moment()))) != 0) ||
        take(compare_world_moments(source.settlement->applied_at, actual_now)) >
            0)
      fail("FieldSegmentFutureApplicationSource", casualty.id);
    const MilitaryActorChange *actual = nullptr;
    Integer ordinal = 0;
    for (std::size_t n = 0; n < receipt->actor_changes.size(); ++n) {
      const auto &change = receipt->actor_changes[n];
      if (change.actor_id != actor_id)
        continue;
      if (actual || n > std::size_t(std::numeric_limits<Integer>::max()))
        fail("FieldSegmentFatalActorAmbiguous", casualty.id);
      actual = &change;
      ordinal = static_cast<Integer>(n);
    }
    if (!actual || actual->status != "dead")
      fail("FieldSegmentFatalActorRequired", actor_id);
    return FieldSegmentFatalSourceRead{source, actual, ordinal};
  });
}
Outcome<FieldSegmentCaptivitySourceRead> read_field_segment_captivity_source(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const SourceRef &casualty,
    const StableId &actor_id, const FieldSegmentApplicationRead &clock) {
  return guarded<FieldSegmentCaptivitySourceRead>([&] {
    if (!clock.bound_to(common, military, world) ||
        casualty.domain != "military" ||
        casualty.kind != "military_casualties" || casualty.id.empty() ||
        actor_id.empty() || !common.actor(actor_id))
      fail("FieldSegmentCaptivitySourceRequired", casualty.id);
    const auto now = take(clock.read_current_moment());
    const auto *receipt = unique(military.casualty_facts, casualty.id);
    if (!receipt)
      fail("FieldSegmentCaptivitySourceRequired", casualty.id);


    auto parent_memory = CheckpointMemoryCharge(0);
    const SourceRef parent{checkpoint_fresh_text_copy("military"),
                           checkpoint_fresh_text_copy("field_battle_segment"),
                           checkpoint_fresh_string_copy(receipt->battle_id)};
    const auto source = take(read_field_segment_settlement_source(
        common, military, world, parent, &clock));
    if (source.casualties != receipt ||
        take(compare_world_moments(source.settlement->applied_at, now)) > 0)
      fail("FieldSegmentCaptivitySourceRequired", casualty.id);
    const MilitaryActorChange *actual = nullptr;
    Integer ordinal = 0;
    for (std::size_t n = 0; n < receipt->actor_changes.size(); ++n) {
      const auto &change = receipt->actor_changes[n];
      if (change.actor_id != actor_id)
        continue;
      if (actual || n > std::size_t(std::numeric_limits<Integer>::max()))
        fail("FieldSegmentCaptivityActorAmbiguous", casualty.id);
      actual = &change;
      ordinal = static_cast<Integer>(n);
    }
    // Field1 original captures use the actual opposing Army. Garrison custody
    // belongs to its separately selected original source, never this adapter.
    if (!actual || actual->status != "captured" || actual->garrison_id ||
        actual->captor_army_id.empty() ||
        actual->captor_army_id == actual->army_id)
      fail("FieldSegmentCapturedActorRequired", actor_id);
    auto captor = take(read_field_original_battle_binding(
        common, world, source.session->battle_id, actual->captor_army_id));
    if (captor->army_id != actual->captor_army_id ||
        captor->battle_id != source.session->battle_id)
      fail("FieldSegmentOriginalCaptorRequired", actor_id);
    // Parent reader already proves this actor's exact unit/captor pair and
    // opposing admitted formation at this segment; no current Actor flags,
    // new custody head, body-applied assertion or member release is inferred.
    return FieldSegmentCaptivitySourceRead{source, actual, captor, ordinal};
  });
}
Status validate_field_segment_settlement_sources(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world,
    const FieldSegmentApplicationRead *actual_clock) {
  try {
    if (actual_clock && !actual_clock->bound_to(common, military, world))
      fail("CrossCandidateFieldSegmentApplicationClock", {});
    std::set<StableId> owned;
    for (const auto &[id, session] : world.sessions)
      for (const auto &row : session.settlements) {
        if (!owned.insert(row.fact_id).second)
          fail("FieldSegmentDuplicateOwner", row.fact_id);
        take(read_field_segment_settlement_source(
            common, military, world,
            {"military", "field_battle_segment", row.fact_id}, actual_clock));
      }
    for (const auto &row : common.state().facts.rows())
      if (row.second.kind == "field_battle_segment" &&
          (row.second.domain != "military" || !owned.contains(row.first)))
        fail("FieldSegmentUnownedHeader", row.first);
    // 子回执也反向找唯一父正文；错填会话 ID 不能绕过 Field 的反覆盖。
    for (const auto &row : military.casualty_facts) {
      const auto *head = common.state().facts.find(row.battle_id);
      if (!head || head->kind != "field_battle_segment")
        continue;
      auto source = take(read_field_segment_settlement_source(
          common, military, world,
          {"military", "field_battle_segment", row.battle_id}, actual_clock));
      if (source.casualties != &row)
        fail("FieldSegmentUnownedCasualty", row.id);
    }
    for (const auto &row : military.battle_facts) {
      const auto *head = common.state().facts.find(row.source_id);
      if (!world.sessions.contains(row.session_id) &&
          (!head || head->kind != "field_battle_segment"))
        continue;
      auto source = take(read_field_segment_settlement_source(
          common, military, world,
          {"military", "field_battle_segment", row.source_id}, actual_clock));
      if (source.terminal != &row)
        fail("FieldSegmentUnownedTerminal", row.id);
    }
    return common_success();
  } catch (Failure &error) {
    return {false, {}, std::move(error.error)};
  }
}
Outcome<WorldMoment> read_field_segment_application_moment(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world, const SourceRef &source,
    const FieldSegmentApplicationRead &clock) {
  return guarded<WorldMoment>([&] {
    if (!clock.bound_to(common, military, world))
      fail("CrossCandidateFieldSegmentApplicationClock", source.id);
    const auto actual = take(read_field_segment_settlement_source(
        common, military, world, source, &clock));
    const auto now = take(clock.read_current_moment());
    if (take(compare_world_moments(actual.settlement->applied_at, now)) > 0)
      fail("FieldSegmentFutureApplicationSource", source.id);
    return actual.settlement->applied_at;
  });
}
Status validate_field_segment_application_sources(
    const CommonStore &common, const MilitaryState &military,
    const FieldBattleWorldState &world,
    const FieldSegmentApplicationRead &clock) {
  if (!clock.bound_to(common, military, world))
    return common_failure("CrossCandidateFieldSegmentApplicationClock");
  auto sources = validate_field_segment_settlement_sources(common, military,
                                                           world, &clock);
  if (!sources.ok)
    return sources;
  std::vector<std::pair<std::size_t, const FieldSegmentSettlement *>> ordered;
  for (const auto &[id, session] : world.sessions)
    for (const auto &settlement : session.settlements) {
      auto read = read_field_segment_application_moment(
          common, military, world,
          {"military", "field_battle_segment", settlement.fact_id}, clock);
      if (!read.ok)
        return {false, {}, read.error};
      auto at = read_actual_common_fact_position(
          common,
          SourceRef{"military", "field_battle_segment", settlement.fact_id});
      if (!at.ok)
        return {false, {}, at.error};
      ordered.emplace_back(at.value, &settlement);
    }
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
  for (std::size_t i = 1; i < ordered.size(); ++i) {
    auto comparison = compare_world_moments(ordered[i - 1].second->applied_at,
                                            ordered[i].second->applied_at);
    if (!comparison.ok)
      return {false, {}, comparison.error};
    if (comparison.value > 0)
      return common_failure("FieldSegmentApplicationOrderMismatch",
                            ordered[i].second->fact_id);
  }
  return common_success();
}
} // namespace hanmo::resident
