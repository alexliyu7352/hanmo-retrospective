#include "field_army_exit_ordinary_loss.hpp"
#include "field_army_exit_rally.hpp"
#include "field_ability_sources.hpp"
#include "field_army_group_origin.hpp"
#include "field_binding_source.hpp"
#include "field_army_exit_placement.hpp"
#include "field_member_loss_group_source.hpp"
#include "field_exit_placement.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
#include "field_exact_contact_source.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/military_roster_casualties.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <set>
#include <tuple>
namespace hanmo::resident {
using namespace movement_detail;
namespace {
// 读取真正军事实体头序；同名字符串不构成父源。
std::size_t head(const CommonStore &c, const StableId &id, const char *kind) {
  const auto *h = c.state().facts.find(id);
  if (!h || h->id != id || h->domain != "military" || h->kind != kind)
    fail("FieldArmyExitLossActualHeadRequired", id);
  return take(read_actual_common_fact_position(c, *h));
}
// 原向量成员分类，不把普通兵ID强造Common Actor。
bool has(const std::vector<StableId> &v, const StableId &id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}
// 只借唯一实际step；终局后真实几何另由完整Field reader验证。
FieldArmyExitOrdinaryCasualtyRead parent(const CommonStore &c,
    const MilitaryState &m, const FieldBattleWorldState &w, const StableId &id,
    const FieldSegmentApplicationRead &clock) {
  if (!clock.bound_to(c, m, w)) fail("FieldArmyExitLossSameWorldRequired", id);
  FieldArmyExitOrdinaryCasualtyRead out;
  const auto step_head = head(c, id, "field_army_exit_step");
  for (const auto &[key, p] : w.army_exit_plans)
    for (const auto &s : p.steps) if (s.fact_id == id) {
      if (out.step || key != p.fact_id) fail("FieldArmyExitLossStepAmbiguous", id);
      out.plan = &p; out.step = &s;
    }
  if (!out.step || !out.plan->terminal_segment || !out.plan->ordinary_deadline)
    fail("FieldArmyExitLossActualStepRequired", id);
  const auto terminal = take(read_field_segment_settlement_source(
      c, m, w, *out.plan->terminal_segment, &clock));
  if (!terminal.terminal || terminal.session->battle_id != out.plan->battle_id ||
      head(c, out.plan->fact_id, "field_army_exit_choice") >=
          head(c, terminal.settlement->fact_id, "field_battle_segment") ||
      head(c, terminal.terminal->id, "military_battle") >= step_head ||
      take(compare_world_moments(out.step->applied_at, take(clock.read_current_moment()))) > 0 ||
      take(compare_world_moments(terminal.settlement->applied_at, out.step->applied_at)) > 0 ||
      take(compare_fractions(out.step->applied_at.sub_half_day, CanonicalFraction{})) != 0 ||
      std::tie(out.step->from.tick, out.step->from.t) >= std::tie(out.step->to.tick, out.step->to.t) ||
      std::tie(out.step->to.tick, out.step->to.t) <
          std::tie(out.plan->ordinary_deadline->tick, out.plan->ordinary_deadline->t))
    fail("FieldArmyExitLossActualDeadlineRequired", id);
  const auto &ended = terminal.session->result->ended;
  const Integer windows = ended.t == 0 ? 1 : 2;
  if (ended.tick > std::numeric_limits<Integer>::max() - windows ||
      out.plan->ordinary_deadline->tick != ended.tick + windows || out.plan->ordinary_deadline->t ||
      out.step->to.tick != out.plan->ordinary_deadline->tick || out.step->to.t ||
      out.step->applied_at.half_day_tick != out.step->to.tick)
    fail("FieldArmyExitLossActualDeadlineRequired", id);
  auto previous_head = head(c, terminal.terminal->id, "military_battle");
  FieldBattleTime previous_at = terminal.settlement->settled_at;
  const WorldMoment *previous_actual = &terminal.settlement->applied_at;
  for (const auto &step : out.plan->steps) {
    const auto position = head(c, step.fact_id, "field_army_exit_step");
    if (position <= previous_head || step.from.tick != previous_at.tick || step.from.t != previous_at.t ||
        std::tie(step.from.tick, step.from.t) >= std::tie(step.to.tick, step.to.t) ||
        take(compare_world_moments(*previous_actual, step.applied_at)) > 0)
      fail("FieldArmyExitLossStepOrderRequired", id);
    previous_head = position; previous_at = step.to; previous_actual = &step.applied_at;
  }
  out.session = terminal.session;
  return out;
}
// 从冻结终局名册只移走本步完整普通兵名单；军官/不可逆旧伤亡不允许改变。
std::vector<MilitaryRosterReceipt> expected_rosters(const CommonStore &c,
    const MilitaryState &m, const FieldBattleWorldState &w,
    const FieldArmyExitOrdinaryCasualtyRead &o, const FieldSegmentApplicationRead &clock) {
  const auto terminal = take(read_field_segment_settlement_source(
      c, m, w, *o.plan->terminal_segment, &clock));
  std::vector<MilitaryRosterReceipt> out;
  std::set<StableId> units, members;
  for (const auto &motion : o.step->formations) {
    if (motion.deserted_member_ids.empty()) continue;
    if (motion.reached || !units.insert(motion.formation_id).second)
      fail("FieldArmyExitLossOrdinarySourceRequired", o.step->fact_id);
    const auto f = o.session->formations.find(motion.formation_id);
    if (f == o.session->formations.end() || f->second.army_id != o.plan->army_id)
      fail("FieldArmyExitLossOriginalFormationRequired", motion.formation_id);
    const MilitaryRosterReceipt *original = nullptr;
    for (const auto &r : terminal.casualties->units) if (r.unit_id == motion.formation_id) {
      if (original) fail("FieldArmyExitLossOriginalRosterAmbiguous", r.unit_id);
      original = &r;
    }
    if (!original) fail("FieldArmyExitLossOriginalRosterRequired", motion.formation_id);
    const FieldArmyExitRallyFormation *rally = nullptr;
    for (const auto &r : o.plan->rally_formations) if (r.formation_id == motion.formation_id) {
      if (rally) fail("FieldArmyExitLossOriginalRallyAmbiguous", r.formation_id);
      rally = &r;
    }
    if (!rally || rally->original_exit || motion.deserted_member_ids != rally->ordinary_member_ids)
      fail("FieldArmyExitLossWholeOrdinaryRequired", motion.formation_id);
    std::vector<StableId> ordinary;
    for (const auto *group : {&original->fit, &original->wounded})
      for (const auto &actor : *group)
        if (actor != f->second.commander_actor_id && f->second.deputy_actor_id != actor)
          ordinary.push_back(actor);
    std::sort(ordinary.begin(), ordinary.end());
    if (ordinary != motion.deserted_member_ids || ordinary.empty())
      fail("FieldArmyExitLossWholeOrdinaryRequired", motion.formation_id);
    // 一个formation的普通兵只能在一个实际截止步离开，不允许两个回执重复移除。
    for (const auto &step : o.plan->steps) if (&step != o.step)
      for (const auto &other : step.formations)
        if (other.formation_id == motion.formation_id && !other.deserted_member_ids.empty())
          fail("FieldArmyExitLossRepeatedDeparture", motion.formation_id);
    MilitaryRosterReceipt next = *original;
    for (const auto &actor : ordinary) {
      if (!members.insert(actor).second || has(next.dead, actor) || has(next.captured, actor) ||
          has(next.deserted, actor)) fail("FieldArmyExitLossOriginalPartitionRequired", actor);
      next.fit.erase(std::remove(next.fit.begin(), next.fit.end(), actor), next.fit.end());
      next.wounded.erase(std::remove(next.wounded.begin(), next.wounded.end(), actor), next.wounded.end());
      next.deserted.push_back(actor);
    }
    out.push_back(std::move(next));
  }
  if (out.empty()) fail("FieldArmyExitLossOrdinaryDepartureRequired", o.step->fact_id);
  return out;
}
// 完整互斥名册逐字比较，不借今日名册替换历史父源。
bool same_roster(const MilitaryRosterReceipt &a, const MilitaryRosterReceipt &b) {
  return a.unit_id == b.unit_id && a.fit == b.fit && a.wounded == b.wounded &&
      a.dead == b.dead && a.captured == b.captured && a.deserted == b.deserted &&
      a.member_ids == b.member_ids && a.morale == b.morale && a.captors == b.captors;
}
}
// 此底层有限原件读不进入完整几何，因此availability/原名册可以安全调用它。
Outcome<FieldArmyExitOrdinaryCasualtyRead> read_field_army_exit_ordinary_casualty_source(
    const CommonStore &c, const MilitaryState &m, const FieldBattleWorldState &w,
    const SourceRef &source, const FieldSegmentApplicationRead &clock) {
  return guard<FieldArmyExitOrdinaryCasualtyRead>([&] {
    if (source.domain != "military" || source.kind != "military_casualties")
      fail("FieldArmyExitLossActualCasualtyRequired", source.id);
    const MilitaryCasualtyFact *receipt = nullptr;
    for (const auto &r : m.casualty_facts) if (r.id == source.id) {
      if (receipt) fail("FieldArmyExitLossDuplicateCasualty", source.id);
      receipt = &r;
    }
    if (!receipt) fail("FieldArmyExitLossActualCasualtyRequired", source.id);
    auto out = parent(c, m, w, receipt->battle_id, clock);
    if (receipt->tick != out.step->applied_at.half_day_tick ||
        head(c, receipt->id, "military_casualties") <= head(c, receipt->battle_id, "field_army_exit_step"))
      fail("FieldArmyExitLossCasualtyOrderRequired", receipt->id);
    for (const auto &r : m.casualty_facts)
      if (r.battle_id == receipt->battle_id && &r != receipt)
        fail("FieldArmyExitLossDuplicateCasualty", receipt->battle_id);
    const auto expected = expected_rosters(c, m, w, out, clock);
    if (receipt->units.size() != expected.size()) fail("FieldArmyExitLossRosterMismatch", receipt->id);
    std::set<StableId> expected_actors, actual_actors;
    for (std::size_t i = 0; i < expected.size(); ++i) {
      if (!same_roster(receipt->units[i], expected[i])) fail("FieldArmyExitLossRosterMismatch", receipt->id);
      for (const auto &motion : out.step->formations) if (motion.formation_id == expected[i].unit_id)
        for (const auto &actor : motion.deserted_member_ids) if (c.actor(actor)) expected_actors.insert(actor);
    }
    for (const auto &change : receipt->actor_changes)
      if (change.status != "deserted" || change.army_id != out.plan->army_id ||
          !change.captor_army_id.empty() || change.garrison_id || !c.actor(change.actor_id) ||
          !actual_actors.insert(change.actor_id).second)
        fail("FieldArmyExitLossNamedOutcomeMismatch", receipt->id);
    if (expected_actors != actual_actors) fail("FieldArmyExitLossNamedCoverageMismatch", receipt->id);
    out.casualties = receipt;
    return out;
  });
}
// 热writer先使用完整原step几何，再将本次真实名单交唯一旧军人分类writer。
static Outcome<SourceRef> apply_field_army_exit_ordinary_casualties_in_candidate(
    CommonStore &c, MilitaryState &m, const FieldExactContactReadContext &x,
    const FieldBattleCatalog &catalog, const FieldBattleAbilityReadContext &a,
    const SourceRef &step, CheckpointMemoryCharge &owner) {
  return guard<SourceRef>([&] {
    if (&c != &x.common || &m != &x.military || a.exact_contacts != &x)
      fail("FieldArmyExitLossSameWorldRequired", step.id);
    const auto original = take(read_field_army_exit_step_source(x, catalog, a, step));
    if (!same_moment(original.step->applied_at, take(x.clock.read_current_moment())) ||
        !same_moment(original.step->applied_at, take(x.clock.read_field_window_closing_moment())))
      fail("FieldArmyExitLossActualClosingRequired", step.id);
    bool has_departure = false;
    for (const auto &motion : original.step->formations)
      for (const auto &actor : motion.deserted_member_ids) {
        take(read_field_army_exit_ordinary_departure_source(x, catalog, a, step, actor));
        has_departure = true;
      }
    if (!has_departure) fail("FieldArmyExitLossOrdinaryDepartureRequired", step.id);
    for (const auto &old : m.casualty_facts) if (old.battle_id == step.id) {
      const SourceRef source{"military", "military_casualties", old.id};
      take(read_field_army_exit_ordinary_casualty_source(c, m, x.field, source, x.clock));
      return source;
    }
    const auto p = parent(c, m, x.field, step.id, x.clock);
    const auto expected = expected_rosters(c, m, x.field, p, x.clock);
    std::vector<OriginalMilitaryRosterFinalView> finals;
    for (const auto &r : expected) {
      const MilitaryUnitRecord *u = nullptr;
      for (const auto &v : m.military_units) if (v.id == r.unit_id) {
        if (u) fail("FieldArmyExitLossOriginalUnitAmbiguous", r.unit_id);
        u = &v;
      }
      if (!u || u->army_id != p.plan->army_id || u->member_ids != r.member_ids)
        fail("FieldArmyExitLossOriginalUnitRequired", r.unit_id);
      // 不允许当前单位已经发生另外分类后仍借旧terminal覆盖它。
      const auto terminal = take(read_field_segment_settlement_source(c, m, x.field, *p.plan->terminal_segment, &x.clock));
      const MilitaryRosterReceipt *before = nullptr;
      for (const auto &row : terminal.casualties->units) if (row.unit_id == r.unit_id) before = &row;
      if (!before || u->fit != before->fit || u->wounded != before->wounded ||
          u->dead != before->dead || u->captured != before->captured ||
          u->deserted != before->deserted || u->captors != before->captors || u->morale != before->morale)
        fail("FieldArmyExitLossCurrentRosterSourceChanged", r.unit_id);
      finals.push_back({u->id,u->army_id,u->commander_actor_id,u->troop_type_id,
          r.member_ids,r.fit,r.wounded,r.dead,r.captured,r.deserted,r.morale});
    }
    owner.grow(0); // 名册仍真实复制且保原 job 检查，不走逐字段计量。
    const auto receipt = take(apply_original_military_roster_casualties(
        c, m, step.id, original.step->applied_at.half_day_tick,
        std::vector<StableId>{p.plan->army_id}, finals, {}, {}, true));
    const SourceRef source{"military", "military_casualties", receipt.id};
    take(read_field_army_exit_ordinary_casualty_source(c, m, x.field, source, x.clock));
    return source;
  });
}
// 每个真实ordinary step必须恰有一回执，子回执也必须反向属于真实step。
Status validate_field_army_exit_ordinary_casualty_sources(const CommonStore &c,
    const MilitaryState &m, const FieldBattleWorldState &w,
    const FieldSegmentApplicationRead &clock) {
  return status([&] {
    if (!clock.bound_to(c,m,w)) fail("FieldArmyExitLossSameWorldRequired");
    for (const auto &[id,p] : w.army_exit_plans) for (const auto &step : p.steps) {
      bool needed = false;
      for (const auto &motion : step.formations) needed = needed || !motion.deserted_member_ids.empty();
      std::size_t count = 0;
      for (const auto &r : m.casualty_facts) if (r.battle_id == step.fact_id) {
        ++count;
        take(read_field_army_exit_ordinary_casualty_source(c,m,w,{"military","military_casualties",r.id},clock));
      }
      if (count != (needed ? 1u : 0u)) fail("FieldArmyExitLossCoverageMismatch", step.fact_id);
    }
    for (const auto &r : m.casualty_facts) {
      const auto *h = c.state().facts.find(r.battle_id);
      if (h && h->kind == "field_army_exit_step")
        take(read_field_army_exit_ordinary_casualty_source(c,m,w,{"military","military_casualties",r.id},clock));
    }
  });
}
namespace {
// 捕获writer保持原错误/真实单写边界，不与原几何guard的异常类型混用。
struct LossFailure { RuleError error; };
template<class T> T loss_take(Outcome<T> value) {
  if (!value.ok) throw LossFailure{std::move(value.error)};
  return std::move(value.value);
}
void loss_take(Status value) { if (!value.ok) throw LossFailure{std::move(value.error)}; }
void loss_require(bool yes, const char *code) {
  if (!yes) throw LossFailure{common_failure(code).error};
}
SourceRef loss_fresh(const SourceRef &s) {
  return {checkpoint_fresh_string_copy(s.domain),checkpoint_fresh_string_copy(s.kind),checkpoint_fresh_string_copy(s.id)};
}
}
namespace {
// 不可由外部构造的同批先验；指针借本候选不可变step/原entry，anchor取实际motion末点。
struct PreparedOrdinaryMember {
  const StableId *actor = nullptr;
  const FieldArmyExitStep *step = nullptr;
  FieldOriginalArmyGroupOrigin origin;
  MovementAnchor anchor;
};
// 尚未绑定S1，明确只保真实capture身份，不冒充BoundSpatialLocationWrite。
struct DeferredOrdinaryApplication {
  BoundActorAvailabilityWrite availability;
  StableId actor;
  SourceRef handoff;
  std::shared_ptr<const ActorLocationCaptureIdentity> capture;
  std::shared_ptr<const ActorLocationWriteIdentity> write;
  Integer sequence = 0;
};
// 私有prepared只在本批栈内活；其后不再full-read，等全S4完成才走原S1冷binder。
DeferredOrdinaryApplication apply_prepared_ordinary_member(
    CommonStore &common, AuthoredActorFoundationState &foundation,
    MovementExactState &movement, GroupMovementExecutionState &groups,
    const FieldGroupHandoffReadContext &x, const PreparedOrdinaryMember &prepared,
    const SourceRef &casualty, const ReadStamp &stamp,
    const ReadStampAuthority &stamps, const DomainReadVersionSource &versions,
    CheckpointMemoryCharge &owner, ExactArithmeticBudget budget) {
    const auto &actor = *prepared.actor;
    loss_require(&common == &x.common && &movement == &x.movement &&
                &groups == &x.groups && x.original_body &&
                &x.original_body->common == &common &&
                &x.original_body->state == &foundation &&
                common.actor_location_capture_identity() &&
                common.actor_condition_capture_identity(),
            "FieldDesertionSameCandidateCaptureRequired");
    const auto &body = *x.original_body;
    const auto low = loss_take(read_field_army_exit_ordinary_casualty_source(
        common,x.military,x.field,casualty,x.clock));
    loss_require(low.step == prepared.step, "FieldArmyExitLossPreparedStepChanged");
    FieldMemberLossGroupOrigin origin;
    origin.ordinary_loss = low; origin.session = low.session;
    origin.applied_at = &low.step->applied_at;
    origin.journey = prepared.origin.journey; origin.opening = prepared.origin.opening;
    origin.closed = prepared.origin.closed; origin.original_contact = prepared.origin.original_contact;
    for (std::size_t i=0;i<low.casualties->actor_changes.size();++i)
      if (low.casualties->actor_changes[i].actor_id == actor) {
        loss_require(!origin.actor_change && i <= std::size_t(std::numeric_limits<Integer>::max()),
                     "FieldArmyExitLossOriginalOrdinalRequired");
        origin.actor_change=&low.casualties->actor_changes[i]; origin.casualty_ordinal=Integer(i);
      }
    loss_require(origin.actor_change, "FieldArmyExitLossOriginalNamedMemberRequired");
    loss_require(origin.ordinary_loss && origin.applied_at &&
                origin.actor_change->status == "deserted" &&
                origin.actor_change->captor_army_id.empty() &&
                !origin.actor_change->garrison_id &&
                origin.journey->state == GroupJourneyLifecycle::paused,
            "FieldDesertionOriginalPausedMemberRequired");
    loss_require(loss_take(compare_world_moments(loss_take(x.clock.read_current_moment()),
                                       *origin.applied_at,
                                       budget)) == 0,
            "FieldDesertionActualMomentRequired");
    loss_require(x.exact_contacts && x.original_abilities &&
                x.original_abilities->exact_contacts == x.exact_contacts &&
                x.original_abilities->conditions == x.original_body,
            "FieldArmyExitLossOriginalAbilitiesRequired");
    const auto &anchor = prepared.anchor;
    for (const auto &entry : groups.member_loss_handoffs.rows())
      loss_require(entry.second.journey_id != origin.journey->journey_id ||
                  entry.second.actor_id != actor,
              "FieldDesertionMemberAlreadyLost");
    for (const auto &entry : groups.contact_member_releases.rows())
      loss_require(entry.second.journey_id != origin.journey->journey_id ||
                  std::find(entry.second.named_participants.begin(),
                            entry.second.named_participants.end(),
                            actor) == entry.second.named_participants.end(),
              "FieldDesertionMemberAlreadyReleased");
    const SpatialWriterBinding *previous = nullptr;
    const StableId *original_key = nullptr;
    for (const auto &entry : movement.spatial_bindings.rows())
      if (entry.second.subject_kind == "actor" &&
          entry.second.subject_id == actor) {
        loss_require(!previous && entry.first.size() == 6 + actor.size() &&
                    entry.first.compare(0, 6, "actor:") == 0 &&
                    entry.first.compare(6, actor.size(), actor) == 0,
                "FieldDesertionOriginalBindingAmbiguous");
        previous = &entry.second;
        original_key = &entry.first;
      }
    const auto *carried =
        previous ? std::get_if<CarriedMovementWriter>(&previous->writer)
                 : nullptr;
    const auto *army =
        carried ? std::get_if<ArmyCarrierRef>(&carried->carrier_ref) : nullptr;
    loss_require(army && army->army_id == origin.actor_change->army_id &&
                previous->binding_source.domain == "travel" &&
                previous->binding_source.kind == "group_movement_opening" &&
                previous->binding_source.id == origin.opening->id,
            "FieldDesertionOriginalCarriedMemberRequired");
    const auto location_capture = common.actor_location_capture_identity();
    const auto location_offset =
        common.actor_location_execution_drafts().size();
    const auto condition_offset =
        common.actor_condition_execution_drafts().size();
    loss_require(
        location_offset < std::size_t(std::numeric_limits<Integer>::max()) &&
            condition_offset < std::size_t(std::numeric_limits<Integer>::max()),
        "FieldDesertionCaptureSequenceExhausted");
    auto permit = loss_take(prepare_field_military_availability(
        common, foundation, body, casualty, origin.casualty_ordinal, stamp,
        stamps, versions));
    loss_require(bool(permit), "FieldDesertionActualAvailabilityPermitRequired");
    owner.grow(0); // 真源校验和单写边界保持，只退役复制计量。
    const auto binding_key = checkpoint_fresh_string_copy(*original_key);
    ActorMutation mutation;
    mutation.actor_id = checkpoint_fresh_string_copy(actor);
    mutation.available = false;
    mutation.location = loss_take(project_movement_location(anchor, budget));
    loss_take(common.apply_actor(mutation));
    auto availability = loss_take(
        bind_field_military_availability(common, foundation, body, *permit));
    const auto &locations = common.actor_location_execution_drafts();
    loss_require(availability.transition &&
                availability.capture_sequence ==
                    Integer(condition_offset + 1) &&
                common.actor_condition_execution_drafts().size() ==
                    condition_offset + 1 &&
                locations.size() == location_offset + 1 &&
                locations.back().sequence == Integer(location_offset + 1) &&
                locations.back().actor_id == actor && locations.back().before &&
                locations.back().write_identity &&
                locations.back().kind ==
                    ActorLocationExecutionDraft::Kind::mutation,
            "FieldDesertionSingleActualWriteRequired");
    const auto write_identity = locations.back().write_identity;
    const SourceRef capacity{
        checkpoint_fresh_text_copy("actor_foundation"),
        checkpoint_fresh_text_copy("capacity_transition"),
        checkpoint_fresh_string_copy(availability.transition->transition_id)};
    const auto id = loss_take(common.next_id("group_member_loss_handoff"));
    GroupMemberLossHandoffSource row;
    row.id = checkpoint_fresh_string_copy(id);
    row.journey_id = checkpoint_fresh_string_copy(origin.journey->journey_id);
    row.actor_id = checkpoint_fresh_string_copy(actor);
    row.original_contact = loss_fresh(*origin.original_contact);
    row.actual_casualty = loss_fresh(casualty);
    row.casualty_ordinal = origin.casualty_ordinal;
    row.occurred_at = *origin.applied_at;
    row.actual_body = GroupMemberDesertionApplication{loss_fresh(capacity)};
    const SourceRef handoff{
        checkpoint_fresh_text_copy("travel"),
        checkpoint_fresh_text_copy("group_member_loss_handoff"),
        checkpoint_fresh_string_copy(id)};
    SpatialWriterBinding next;
    next.subject_kind = checkpoint_fresh_text_copy("actor");
    next.subject_id = checkpoint_fresh_string_copy(actor);
    next.writer = StableMovementWriter{anchor};
    next.binding_source = loss_fresh(handoff);
    movement.spatial_bindings.set(binding_key, std::move(next));
    groups.member_loss_handoffs.set(checkpoint_fresh_string_copy(id),
                                    std::move(row));
    loss_take(common.append_fact(
        {checkpoint_fresh_string_copy(id),
         checkpoint_fresh_text_copy("group_member_loss_handoff"),
         checkpoint_fresh_text_copy("travel")}));
    return {{checkpoint_fresh_string_copy(availability.transition->transition_id),
             availability.capture_identity, availability.capture_sequence},
            checkpoint_fresh_string_copy(actor), loss_fresh(handoff),
            location_capture, write_identity, Integer(location_offset + 1)};
}
} // namespace
// 真具名ordinary后果必须各有唯一原S4/S1采用；匿名兵只进入原名册，不造角色。
Status validate_field_army_exit_ordinary_loss_applications(const FieldGroupHandoffReadContext &x) {
  return status([&] {
    take(validate_field_army_exit_ordinary_casualty_sources(x.common,x.military,x.field,x.clock));
    OriginalFieldBattleExitPlacementRead original(x);
    for (const auto &receipt : x.military.casualty_facts) {
      const auto *parent = x.common.state().facts.find(receipt.battle_id);
      if (!parent || parent->kind != "field_army_exit_step") continue;
      // 匿名兵批也必须经过完整几何；只有下层history/availability不回调本口。
      if (!x.exact_contacts || !x.original_abilities || !x.original_body ||
          &x.exact_contacts->common != &x.common || &x.exact_contacts->military != &x.military ||
          &x.exact_contacts->field != &x.field || &x.exact_contacts->movement != &x.movement ||
          &x.exact_contacts->carriers != &x.carriers || &x.exact_contacts->groups != &x.groups ||
          &x.exact_contacts->clock != &x.clock || &x.exact_contacts->travel != &x.travel ||
          &x.original_body->common != &x.common || x.original_body->military != &x.military ||
          x.original_body->injury_field != &x.field ||
          x.original_abilities->exact_contacts != x.exact_contacts ||
          x.original_abilities->conditions != x.original_body)
        fail("FieldArmyExitLossOriginalAbilitiesRequired",receipt.id);
      const SourceRef source{"military","military_casualties",receipt.id};
      const auto low = take(read_field_army_exit_ordinary_casualty_source(
          x.common,x.military,x.field,source,x.clock));
      const auto full = take(read_field_army_exit_step_source(
          *x.exact_contacts,x.catalog,*x.original_abilities,
          {"military","field_army_exit_step",receipt.battle_id}));
      if (full.step != low.step || full.plan != low.plan || full.session != low.session)
        fail("FieldArmyExitLossOriginalStepOwnerMismatch",receipt.id);
      for (const auto &change : receipt.actor_changes) {
        take(read_field_member_loss_group_origin(x,source,change.actor_id));
        const GroupMemberLossHandoffSource *adopted = nullptr;
        for (const auto &entry : x.groups.member_loss_handoffs.rows())
          if (entry.second.actor_id == change.actor_id && same_source(entry.second.actual_casualty,source)) {
            if (adopted || entry.first != entry.second.id)
              fail("FieldArmyExitLossDuplicateAdoption",change.actor_id);
            adopted = &entry.second;
          }
        if (!adopted || !std::holds_alternative<GroupMemberDesertionApplication>(adopted->actual_body))
          fail("FieldArmyExitLossActualAdoptionRequired",change.actor_id);
        take(original.validate_actual_group_member_loss(*adopted));
        take(original.read_field_member_loss_placement(
            MomentOwnerSource{{"travel","group_member_loss_handoff",adopted->id},1},change.actor_id));
      }
    }
  });
}
// 单候选完整批次消除“先添casualty再逐人full body”循环，最终完整反覆盖仍必过。
Outcome<FieldArmyExitOrdinaryLossApplication>
apply_field_army_exit_ordinary_losses_with_capture_in_candidate(
    CommonStore &common, MilitaryState &military, AuthoredActorFoundationState &foundation,
    MovementExactState &movement, GroupMovementExecutionState &groups,
    const FieldGroupHandoffReadContext &x, const SourceRef &step,
    const CurrentReadStampRead &current, ReadStampAuthority &stamps,
    const DomainReadVersionSource &versions, CheckpointMemoryCharge &owner,
    ExactArithmeticBudget budget) {
  try {
    loss_require(&common==&x.common && &military==&x.military && &movement==&x.movement &&
        &groups==&x.groups && x.exact_contacts && x.original_abilities && x.original_body &&
        &x.original_body->state==&foundation && &x.original_body->common==&common &&
        x.original_abilities->exact_contacts==x.exact_contacts &&
        x.original_abilities->conditions==x.original_body &&
        common.actor_condition_capture_identity() && common.actor_location_capture_identity(),
        "FieldArmyExitLossSameCandidateCaptureRequired");
    const auto condition_capture=common.actor_condition_capture_identity();
    const auto location_capture=common.actor_location_capture_identity();
    const auto savepoint=loss_take(common.actor_condition_savepoint_boundary());
    const auto original=loss_take(read_field_army_exit_step_source(
        *x.exact_contacts,x.catalog,*x.original_abilities,step));
    bool has_ordinary = false;
    for (const auto &motion : original.step->formations)
      has_ordinary = has_ordinary || !motion.deserted_member_ids.empty();
    if (!has_ordinary) {
      // 完整step已验；无业务后果时不造伤亡头，也不提供虚假的capture证据。
      for (const auto &receipt : military.casualty_facts)
        loss_require(receipt.battle_id != step.id, "FieldArmyExitLossUnexpectedCasualty");
      return {true,FieldArmyExitOrdinaryLossApplication{}, {}};
    }
    // 已真实采用的整批只验证原件，不重做任何Common mutation。
    for(const auto &receipt:military.casualty_facts) if(receipt.battle_id==step.id) {
      loss_take(validate_field_army_exit_ordinary_loss_applications(x));
      FieldArmyExitOrdinaryLossApplication prior;
      prior.actual_casualty = SourceRef{"military","military_casualties",receipt.id};
      return {true,std::move(prior),{}};
    }
    std::vector<PreparedOrdinaryMember> prepared;
    for(const auto &motion:original.step->formations) for(const auto &actor:motion.deserted_member_ids) {
      const auto departure=loss_take(read_field_army_exit_ordinary_departure_source(
          *x.exact_contacts,x.catalog,*x.original_abilities,step,actor));
      if(!common.actor(actor)) continue; // 真匿名兵只有Military名册，不造S4/S1人物。
      loss_require(departure.member && departure.source.step==original.step,
                   "FieldArmyExitLossOriginalNamedMemberRequired");
      const auto *binding=loss_take(read_field_original_battle_binding(
          common,x.field,original.plan->battle_id,original.plan->army_id));
      auto origin=loss_take(read_field_original_army_group_origin(x,*binding,budget));
      loss_require(origin.journey && origin.opening && origin.closed && origin.original_contact &&
          origin.journey->state==GroupJourneyLifecycle::paused &&
          std::count(origin.opening->named_participants.begin(),origin.opening->named_participants.end(),actor)==1,
          "FieldArmyExitLossOriginalPausedMemberRequired");
      auto anchor=loss_take(read_field_army_exit_ordinary_anchor(
          *x.exact_contacts,x.catalog,*x.original_abilities,step,actor,budget));
      prepared.push_back({&actor,original.step,origin,std::move(anchor)});
    }
    const auto casualty=loss_take(apply_field_army_exit_ordinary_casualties_in_candidate(
        common,military,*x.exact_contacts,x.catalog,*x.original_abilities,step,owner));
    std::vector<DeferredOrdinaryApplication> pending;
    for(const auto &member:prepared) {
      loss_take(common.validate_actor_condition_savepoint_boundary(savepoint));
      loss_require(common.actor_condition_capture_identity()==condition_capture &&
          common.actor_location_capture_identity()==location_capture,
          "FieldArmyExitLossPreparedCaptureChanged");
      ReadStampWriteBoundary boundary(stamps);
      auto stamp=loss_take(current.capture({{ResourceKey::Kind::actor,*member.actor}},
          {{"actor_foundation","condition_head",*member.actor}},{casualty.id}));
      loss_require(bool(stamp),"FieldArmyExitLossActualReadStampRequired");
      pending.push_back(apply_prepared_ordinary_member(common,foundation,movement,groups,x,
          member,casualty,*stamp,stamps,versions,owner,budget));
    }
    // 现在每个新actor的capacity/body都已落真账，再让原位置binder完整反读来源。
    FieldArmyExitOrdinaryLossApplication out; out.actual_casualty=casualty;
    OriginalFieldBattleExitPlacementRead placement(x,budget);
    for(auto &applied:pending) {
      auto location=loss_take(bind_actual_field_member_loss_location_write(common,movement,
          placement,applied.actor,MomentOwnerSource{applied.handoff,1},applied.capture,
          applied.write,applied.sequence,budget));
      out.availability.push_back(std::move(applied.availability));
      out.locations.push_back(std::move(location));
    }
    loss_take(validate_field_army_exit_ordinary_loss_applications(x));
    return {true,std::move(out),{}};
  } catch(LossFailure &failure) { return {false,{},std::move(failure.error)}; }
    catch(const std::bad_alloc &) { return {false,{},common_failure("FieldArmyExitLossResourceExhausted").error}; }
}
} // namespace hanmo::resident
