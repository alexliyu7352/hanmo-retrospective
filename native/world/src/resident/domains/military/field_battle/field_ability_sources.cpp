#include "field_event_legacy_kinds.hpp"
// 第一版六项被动与四种战法的实际可用来源；不运行旧 BattleRecord 或旧回合。
#include "field_ability_sources.hpp"
#include "field_command_source.hpp"
#include "field_commander_change_source.hpp"
#include "field_exact_contact_source.hpp"
#include "field_reinforcement_queue.hpp"
#include "field_segment_settlement.hpp"
#include "field_world.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/domains/actor_foundation/effective_attributes_v1.hpp"
#include "resident/domains/growth/growth_field_history.hpp"
#include "resident/domains/growth/growth_field_origin.hpp"
#include "resident/domains/military/battle_model.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/military_tactic_learning_source.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/original_world_read_boundary.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "resident/model/dynamic_contract.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
Integer field_tactic_cooldown_s(FieldTacticKind);
Integer field_tactic_duration_s(FieldTacticKind);
bool tactic_before(const FieldBattleTime &, const FieldBattleTime &);
// 合同以外的旧兵种战法不在第一版可用列表里，不删除原定义。
std::optional<FieldTacticKind> supported(const StableId &kind) {
  if (kind == "charge")
    return FieldTacticKind::Charge;
  if (kind == "volley")
    return FieldTacticKind::Volley;
  if (kind == "rally")
    return FieldTacticKind::Rally;
  if (kind == "guard")
    return FieldTacticKind::Guard;
  return {};
}
// §13.3 表中兵种限制只限制本项发动，不能替换作者的原兵种身份。
bool category_permits(FieldTacticKind kind, FieldTroopCategory category) {
  if (kind == FieldTacticKind::Charge)
    return category == FieldTroopCategory::Cavalry ||
           category == FieldTroopCategory::Infantry;
  if (kind == FieldTacticKind::Volley)
    return category == FieldTroopCategory::Archer;
  if (kind == FieldTacticKind::Guard)
    return category == FieldTroopCategory::Infantry;
  return true;
}
} // namespace
static Outcome<FieldOfficerStatistics>
read_field_officer_statistics_at_boundary(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const StableId &actor, Integer observed_tick,
    const FactRecord *actual_before,
    const GrowthFieldBoundaryKind *additional_boundary,
    const WorldMoment *actual_consumer_at = nullptr) {
  const auto fail = [&](const char *code) -> Outcome<FieldOfficerStatistics> {
    return {false, {}, common_failure(code, actor).error};
  };
  if (&context.common != &context.growth_history.common ||
      (actual_before &&
       common.state().facts.find(actual_before->id) != actual_before))
    return fail("FieldAbilityGrowthSourceMismatch");
  if (actual_before) {
    if (!actual_consumer_at ||
        actual_consumer_at->half_day_tick != observed_tick)
      return fail("FieldAbilityActualMomentMismatch");
    auto canonical =
        compare_world_moments(*actual_consumer_at, *actual_consumer_at);
    if (!canonical.ok)
      return {false, {}, canonical.error};
  }
  // 当前整 World 正在 contact 栈，不代表每个历史快照都属于今天这一 phase。
  const DynamicStepInput *source_phase = context.phase;
  const bool npc_source =
      additional_boundary &&
      (*additional_boundary == GrowthFieldBoundaryKind::npc_started ||
       *additional_boundary == GrowthFieldBoundaryKind::npc_council_adopted);
  const bool exact_source =
      npc_source ||
      (actual_before && actual_before->kind == "field_battle_entered");
  if (actual_before) {
    if (exact_source) {
      if (!context.actual_clock || !actual_consumer_at)
        return fail("FieldAbilityActualWorldClockRequired");
      auto now = context.actual_clock->read_current_moment();
      if (!now.ok)
        return {false, {}, now.error};
      auto current = compare_world_moments(*actual_consumer_at, now.value);
      if (!current.ok)
        return {false, {}, current.error};
      if (current.value > 0)
        return fail("FieldAbilityFutureSourceMoment");
      // Only the actual current application may borrow this phase. Historical
      // same-tick snapshots retain their own cut and never borrow today's XP.
      if (!source_phase || current.value != 0) {
        source_phase = nullptr;
      } else {
        auto lower = compare_world_moments(
            *actual_consumer_at, WorldMoment{source_phase->from_tick, {}});
        auto upper = compare_world_moments(
            *actual_consumer_at, WorldMoment{source_phase->to_tick, {}});
        if (!lower.ok)
          return {false, {}, lower.error};
        if (!upper.ok)
          return {false, {}, upper.error};
        if (lower.value < 0 || upper.value > 0)
          return fail("FieldAbilitySourcePhaseMomentMismatch");
      }
    } else if (!source_phase || actual_before->kind != "field_battle_session" ||
               observed_tick != source_phase->to_tick) {
      source_phase = nullptr;
    }
  }
  FieldOfficerStatistics result;
  for (const auto &entry : {std::pair{"infantry", &result.infantry},
                            std::pair{"cavalry", &result.cavalry},
                            std::pair{"archery", &result.archery},
                            std::pair{"tactics", &result.tactics}}) {
    Outcome<GrowthFieldSkillObservation> read;
    if (actual_before) {
      GrowthFieldBoundaryKind kind;
      if (actual_before->kind == "field_battle_session")
        kind = GrowthFieldBoundaryKind::npc_contact_session;
      else if (actual_before->kind == "field_battle_started")
        kind = additional_boundary && *additional_boundary ==
                                          GrowthFieldBoundaryKind::npc_started
                   ? GrowthFieldBoundaryKind::npc_started
                   : GrowthFieldBoundaryKind::player_started;
      else if (actual_before->kind == "field_battle_entered")
        kind = GrowthFieldBoundaryKind::actor_entered_at_contact;
      else if (additional_boundary &&
               ((actual_before->kind == "military_council" &&
                 (*additional_boundary ==
                      GrowthFieldBoundaryKind::council_adopted ||
                  *additional_boundary ==
                      GrowthFieldBoundaryKind::npc_council_adopted)) ||
                (actual_before->kind == "field_battle_stratagem" &&
                 *additional_boundary ==
                     GrowthFieldBoundaryKind::stratagem_adopted) ||
                (actual_before->kind == "field_commander_changed" &&
                 *additional_boundary ==
                     GrowthFieldBoundaryKind::commander_changed)))
        kind = *additional_boundary;
      else
        return fail("FieldAbilityStartSourceRequired");
      read = read_growth_field_skill_before_fact(
          common, context.growth_history, context.growth, actor, entry.first,
          observed_tick, *actual_before, kind, source_phase, context.phase,
          actual_consumer_at, context.actual_clock);
    } else {
      if (!context.original_read_boundary || !context.actual_clock ||
          !context.original_read_boundary->bound_to(common,
                                                    *context.actual_clock))
        return fail("FieldAbilityCurrentReadBoundaryRequired");
      auto observed = context.original_read_boundary->read_observed_moment();
      if (!observed.ok)
        return {false, {}, observed.error};
      if (observed.value.half_day_tick != observed_tick)
        return fail("FieldAbilityGrowthSourceMismatch");
      read = read_growth_field_skill_current_preview(
          common, context.growth_history, context.growth, actor, entry.first,
          *context.original_read_boundary, *context.actual_clock);
    }
    if (!read.ok)
      return {false, {}, read.error};
    // 合同直接消费原技艺字段，不把 0–1200 原值误换成 0–4 等级。
    *entry.second = read.value.experience;
  }
  if (!context.conditions || !context.permanent_attributes ||
      !context.attribute_absence || &context.conditions->common != &common)
    return fail("FieldEffectiveAttributesOriginalSourcesRequired");
  if ((actual_before && !actual_consumer_at) ||
      (!actual_before && !context.actual_clock))
    return fail("FieldEffectiveAttributesActualBoundaryRequired");
  auto attributes =
      actual_before
          ? read_effective_character_attributes_before_fact_v1(
                *context.conditions, *context.permanent_attributes,
                *context.attribute_absence, actor,
                {actual_before->domain, actual_before->kind, actual_before->id},
                *actual_consumer_at)
          : read_current_effective_character_attributes_v1(
                *context.conditions, *context.permanent_attributes,
                *context.attribute_absence, *context.actual_clock, actor);
  if (!attributes.ok)
    return {false, {}, attributes.error};
  result.attributes = std::move(attributes.value);
  return {true, std::move(result), {}};
}
Outcome<FieldOfficerStatistics>
read_field_officer_statistics(const CommonStore &common,
                              const FieldBattleAbilityReadContext &context,
                              const StableId &actor, Integer observed_tick,
                              const FactRecord *actual_before) {
  return read_field_officer_statistics_at_boundary(
      common, context, actor, observed_tick, actual_before, nullptr);
}
Outcome<FieldOfficerStatistics>
read_field_officer_statistics_before_council_adoption(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleWorldState &world, const FieldCouncilFact &adoption,
    const StableId &actor) {
  const auto fail = [&](const char *code) -> Outcome<FieldOfficerStatistics> {
    return {false, {}, common_failure(code, adoption.fact_id).error};
  };
  const auto record = world.council_facts.find(adoption.fact_id);
  const auto session = world.sessions.find(adoption.battle_id);
  const auto *head = common.state().facts.find(adoption.fact_id);
  if (record == world.council_facts.end() || &record->second != &adoption ||
      session == world.sessions.end() || !head ||
      head->kind != "military_council" || head->domain != "military" ||
      adoption.kind != FieldCouncilFactKind::Adopted || adoption.side < 0 ||
      adoption.side > 1 || !adoption.council.closed ||
      !adoption.council.adopted_proposal_id ||
      adoption.recorded_tick < session->second.created_tick)
    return fail("FieldCouncilAdoptionSourceRequired");
  const auto &value = session->second;
  const auto chosen = std::find_if(
      adoption.council.proposals.begin(), adoption.council.proposals.end(),
      [&](const auto &proposal) {
        return proposal.proposal_id == *adoption.council.adopted_proposal_id;
      });
  if (chosen == adoption.council.proposals.end())
    return fail("FieldCouncilAdoptionSourceRequired");
  bool participant = false;
  for (Integer side = 0; side < 2; ++side) {
    const auto &council =
        side == adoption.side ? adoption.council : value.councils[side];
    for (const auto &person : council.participants)
      if (person.actor_id == actor) {
        const auto formation = value.formations.find(person.formation_id);
        if (participant || formation == value.formations.end() ||
            formation->second.side != side ||
            formation->second.commander_actor_id != actor)
          return fail("FieldCouncilAdoptionSourceRequired");
        participant = true;
      }
  }
  if (!participant)
    return fail("FieldCouncilAdoptionSourceRequired");
  const auto *entry = common.state().facts.find(value.battle_id);
  if (!entry || entry->kind != "field_battle_session" ||
      entry->domain != "military")
    return fail("FieldCouncilAdoptionSourceRequired");
  auto entry_position = read_actual_common_fact_position(common, *entry);
  auto adopted_position = read_actual_common_fact_position(common, *head);
  if (!entry_position.ok || !adopted_position.ok)
    return {false,
            {},
            !entry_position.ok ? entry_position.error : adopted_position.error};
  if (entry_position.value >= adopted_position.value)
    return fail("FieldCouncilAdoptionSourceRequired");
  if (!adoption.council.player_actor_id) {
    // 无玩家一侧只在真实 contact_install 内定案。沿本条实际 Adopted
    // 原头与原安装 phase 读取，不借旧 Session 头替代采用前缀。
    if (adoption.command_result ||
        adoption.recorded_tick != value.created_tick ||
        !std::holds_alternative<FieldAiController>(
            value.sides[adoption.side].controller))
      return fail("FieldCouncilAdoptionSourceRequired");
    const auto npc_boundary = GrowthFieldBoundaryKind::npc_council_adopted;
    return read_field_officer_statistics_at_boundary(
        common, context, actor, adoption.recorded_tick, head, &npc_boundary,
        adoption.adopted_actual_at ? &*adoption.adopted_actual_at : nullptr);
  }
  // 玩家主将定案或玩家部将听令是零时命令。原 Adopt 载荷先验，
  // Speak/PlayerOrder 等同名 Common 头不得借此入口充实际采用来源。
  const auto boundary = GrowthFieldBoundaryKind::council_adopted;
  return read_field_officer_statistics_at_boundary(
      common, context, actor, adoption.recorded_tick, head, &boundary,
      adoption.adopted_actual_at ? &*adoption.adopted_actual_at : nullptr);
}
Outcome<FieldOfficerStatistics> read_field_officer_statistics_before_stratagem(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleWorldState &world, const FieldBattleSession &session,
    const FieldStratagemRecord &stratagem, const StableId &actor) {
  const auto fail = [&](const char *code) -> Outcome<FieldOfficerStatistics> {
    return {false, {}, common_failure(code, stratagem.stratagem_id).error};
  };
  const auto actual_session = world.sessions.find(session.battle_id);
  const auto actual_row = session.stratagems.find(stratagem.stratagem_id);
  if (actual_session == world.sessions.end() ||
      &actual_session->second != &session ||
      actual_row == session.stratagems.end() ||
      &actual_row->second != &stratagem ||
      stratagem.strategist_actor_id.empty() ||
      (actor != stratagem.strategist_actor_id &&
       stratagem.target_commander_actor_id != actor) ||
      stratagem.observed_tick < session.created_tick)
    return fail("FieldStratagemAdoptionSourceRequired");
  if (stratagem.side < 0 || stratagem.side > 1 ||
      session.sides[stratagem.side].strategist_actor_id !=
          stratagem.strategist_actor_id)
    return fail("FieldStratagemAdoptionSourceRequired");
  bool strategist = false;
  for (const auto &participant : session.councils[stratagem.side].participants)
    if (participant.role == FieldCouncilRole::Strategist &&
        participant.actor_id == stratagem.strategist_actor_id)
      strategist = true;
  if (!strategist)
    return fail("FieldStratagemAdoptionSourceRequired");
  if (stratagem.kind == FieldStratagemKind::Confuse) {
    const auto target =
        stratagem.target_formation_id
            ? session.formations.find(*stratagem.target_formation_id)
            : session.formations.end();
    if (target == session.formations.end() ||
        target->second.side == stratagem.side)
      return fail("FieldStratagemAdoptionSourceRequired");
    const auto &formation = target->second;
    const StableId *commander = &formation.commander_actor_id;
    if (formation.commander_lost_at &&
        std::tie(formation.commander_lost_at->tick,
                 formation.commander_lost_at->t) <=
            std::tie(stratagem.issued.tick, stratagem.issued.t)) {
      if (!formation.deputy_actor_id)
        return fail("FieldStratagemAdoptionSourceRequired");
      commander = &*formation.deputy_actor_id;
    }
    if (stratagem.target_commander_actor_id != *commander)
      return fail("FieldStratagemAdoptionSourceRequired");
    for (const auto &loss : formation.commander_casualties)
      if (loss.actor_id == *commander && loss.loss == 2 &&
          std::tie(loss.at.tick, loss.at.t) <=
              std::tie(stratagem.issued.tick, stratagem.issued.t))
        return fail("FieldStratagemAdoptionSourceRequired");
  }
  const auto &source = stratagem.adopted_source;
  if (source.domain != "military")
    return fail("FieldStratagemAdoptionSourceRequired");
  if (source.kind == "military_council") {
    const auto adopted = world.council_facts.find(source.id);
    if (adopted == world.council_facts.end() ||
        adopted->second.battle_id != session.battle_id ||
        adopted->second.side != stratagem.side ||
        adopted->second.recorded_tick != stratagem.observed_tick ||
        stratagem.kind == FieldStratagemKind::Confuse)
      return fail("FieldStratagemAdoptionSourceRequired");
    if (!adopted->second.adopted_actual_at)
      return fail("FieldStratagemAdoptionSourceRequired");
    auto same_moment = compare_world_moments(
        stratagem.applied_at, *adopted->second.adopted_actual_at);
    if (!same_moment.ok)
      return {false, {}, same_moment.error};
    if (same_moment.value != 0)
      return fail("FieldStratagemAdoptionSourceRequired");
    return read_field_officer_statistics_before_council_adoption(
        common, context, world, adopted->second, actor);
  }
  if (source.kind != "field_battle_stratagem" ||
      source.id != stratagem.stratagem_id ||
      stratagem.kind != FieldStratagemKind::Confuse ||
      !stratagem.target_commander_actor_id)
    return fail("FieldStratagemAdoptionSourceRequired");
  const auto *head = common.state().facts.find(source.id);
  if (!head || head->kind != source.kind || head->domain != source.domain ||
      stratagem.observed_tick > common.tick())
    return fail("FieldStratagemAdoptionSourceRequired");
  // 采用头已由此真正会话记录反向绑定。目标后来换将不换掉 actor；
  // 原 Growth 边界核同 tick 的真实先后，不拿今天属性或技能兜底。
  const auto boundary = GrowthFieldBoundaryKind::stratagem_adopted;
  return read_field_officer_statistics_at_boundary(
      common, context, actor, stratagem.observed_tick, head, &boundary,
      &stratagem.applied_at);
}
Outcome<StableId>
read_field_effective_commander(const CommonStore &common,
                               const FieldFormation &formation) {
  // §0/§7 的已指定副手暂带；接替者只能使用自己的原属性与习得账。
  // available 是原死/俘/逃散可用性，不检查 action_id 空闲，避免把在军中当缺席。
  const auto *effective_id = &formation.commander_actor_id;
  if (formation.commander_lost_at) {
    if (!formation.deputy_actor_id || formation.deputy_actor_id->empty())
      return {false,
              {},
              common_failure("FieldTacticNoCommander", formation.formation_id)
                  .error};
    const auto &deputy = *formation.deputy_actor_id;
    const bool serving =
        std::find(formation.fit.begin(), formation.fit.end(), deputy) !=
            formation.fit.end() ||
        std::find(formation.wounded.begin(), formation.wounded.end(), deputy) !=
            formation.wounded.end();
    if (!serving || deputy == formation.commander_actor_id)
      return {false,
              {},
              common_failure("FieldTacticNoCommander", formation.formation_id)
                  .error};
    effective_id = &*formation.deputy_actor_id;
  }
  // 局部已阵亡而本段尚未写回 Common 的接替者也已失去发令资格。
  // 不等半日落账后才停止其能力，不把原将的习得移给副手。
  for (const auto &loss : formation.commander_casualties)
    if (loss.actor_id == *effective_id && loss.loss == 2)
      return {false,
              {},
              common_failure("FieldTacticNoCommander", formation.formation_id)
                  .error};
  const auto *actor = common.state().actors.find(*effective_id);
  if (!actor || !actor->alive || !actor->available)
    return {
        false,
        {},
        common_failure("FieldTacticNoCommander", formation.formation_id).error};
  return {true, *effective_id, {}};
}
static Outcome<FieldFormationAbilitySource> read_field_actor_abilities(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const MilitaryBattleCatalog &military,
    const MilitaryTacticLearningCatalog &learning_catalog,
    const MilitaryTacticLearningState &learning,
    const FieldFormation &formation, const StableId &side_commander,
    Integer observed_tick, const DynamicStepInput *phase,
    const StableId &actual_before_fact, const StableId &actual_actor,
    const GrowthFieldBoundaryKind *additional_boundary = nullptr,
    const WorldMoment *actual_consumer_at = nullptr) {
  if (phase != context.phase)
    return {false,
            {},
            common_failure("FieldAbilityGrowthSourceMismatch",
                           formation.formation_id)
                .error};
  const auto *actor = common.state().actors.find(actual_actor);
  if (!actor)
    return {
        false,
        {},
        common_failure("FieldAbilityActorSourceRequired", actual_actor).error};
  const TroopDefinition *troop = nullptr;
  for (const auto &row : military.troop_types)
    if (row.id == formation.troop_type_id) {
      if (troop)
        return {false,
                {},
                common_failure("FieldTroopSourceMismatch", row.id).error};
      troop = &row;
    }
  if (!troop || troop->tactic_capacity < 0)
    return {false,
            {},
            common_failure("FieldTroopSourceRequired", formation.troop_type_id)
                .error};
  const DynamicStepInput *learning_phase = phase;
  if (actual_before_fact.empty() && observed_tick == common.tick())
    learning_phase = nullptr;
  auto valid = validate_military_tactic_learning_structure(
      common, context.common, learning_catalog, learning, learning_phase);
  if (!valid.ok)
    return {false, {}, valid.error};
  const FactRecord *actual_header = nullptr;
  if (!actual_before_fact.empty()) {
    actual_header = common.state().facts.find(actual_before_fact);
    if (!actual_header)
      return {
          false,
          {},
          common_failure("FieldAbilityStartSourceRequired", actual_before_fact)
              .error};
  }
  auto statistics = read_field_officer_statistics_at_boundary(
      common, context, actor->id, observed_tick, actual_header,
      additional_boundary, actual_consumer_at);
  if (!statistics.ok)
    return {false, {}, statistics.error};
  std::array<FieldTrait, 6> traits;
  std::size_t trait_count = 0;
  if (statistics.value.cavalry >= 60)
    traits[trait_count++] = FieldTrait::CavalryMaster;
  if (statistics.value.infantry >= 60)
    traits[trait_count++] = FieldTrait::InfantryMaster;
  if (statistics.value.archery >= 60)
    traits[trait_count++] = FieldTrait::ArcheryMaster;
  if (statistics.value.attributes.leadership >= 75)
    traits[trait_count++] = FieldTrait::Inspiring;
  if (actor->id == side_commander && statistics.value.tactics >= 60)
    traits[trait_count++] = FieldTrait::StaffWork;
  if (statistics.value.attributes.martial >= 80)
    traits[trait_count++] = FieldTrait::Valiant;
  struct Learned {
    std::size_t index;
    FieldTacticKind kind;
    MilitaryTacticLearningObservation actual;
  };
  CheckpointVector<Learned> selected;
  CheckpointVector<const StableId *> seen;
  if (learning_catalog.enabled)
    for (std::size_t n = 0; n < troop->tactics.size(); ++n) {
      const auto &tactic = troop->tactics[n];
      const auto kind = supported(tactic.kind);
      if (!kind || !category_permits(*kind, formation.category))
        continue;
      bool repeated = false;
      for (const auto *id : seen)
        repeated = repeated || *id == tactic.id;
      if (tactic.id.empty() || repeated || tactic.point_cost < 0 ||
          tactic.power_bp < 0 || tactic.morale_effect < 0)
        return {false,
                {},
                common_failure("FieldTacticSourceMismatch", tactic.id).error};
      seen.push_back(&tactic.id);
      if (tactic.learned_tactic_id.empty())
        continue;
      auto learned = read_military_learned_tactic(
          common, context.common, learning_catalog, learning, actor->id,
          tactic.learned_tactic_id, observed_tick, actual_before_fact,
          learning_phase);
      if (!learned.ok)
        return {false, {}, learned.error};
      if (!learned.value)
        continue;
      selected.push_back({n, *kind, std::move(*learned.value)});
    }
  FieldFormationAbilitySource out;


  for (const auto &row : selected) {
    const auto &tactic = troop->tactics[row.index];


  }
  (out.copied_payload).grow(0);
  out.formation_id = formation.formation_id;
  out.commander_actor_id = actor->id;
  out.tactic_capacity = troop->tactic_capacity;
  out.traits.assign(traits.begin(), traits.begin() + trait_count);
  out.tactics.reserve(selected.size());
  for (const auto &row : selected) {
    const auto &tactic = troop->tactics[row.index];
    out.tactics.push_back(
        {tactic.id, tactic.learned_tactic_id, row.actual.acquisition_fact_id,
         row.kind, tactic.point_cost, tactic.power_bp, tactic.morale_effect});
  }
  std::sort(
      out.tactics.begin(), out.tactics.end(),
      [](const auto &a, const auto &b) { return a.tactic_id < b.tactic_id; });
  return {true, std::move(out), {}};
}
Outcome<FieldFormationAbilitySource> read_field_formation_abilities(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const MilitaryBattleCatalog &military,
    const MilitaryTacticLearningCatalog &learning_catalog,
    const MilitaryTacticLearningState &learning,
    const FieldFormation &formation, const StableId &side_commander,
    Integer observed_tick, const DynamicStepInput *phase,
    const StableId &actual_before_fact, const WorldMoment *actual_consumer_at) {
  auto effective = read_field_effective_commander(common, formation);
  if (!effective.ok)
    return {false, {}, effective.error};
  return read_field_actor_abilities(
      common, context, military, learning_catalog, learning, formation,
      side_commander, observed_tick, phase, actual_before_fact, effective.value,
      nullptr, actual_consumer_at);
}

} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
Outcome<FieldFormationAbilitySource> initial_ability_source(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleSession &session, const FieldFormation &formation,
    const SourceRef *actual_source = nullptr,
    std::optional<Integer> observed_at = {},
    const StableId *historical_actor = nullptr,
    const WorldMoment *actual_consumer_at = nullptr, bool npc_start = false) {
  if (formation.side < 0 || formation.side > 1)
    return {false,
            {},
            common_failure("FieldAbilityStartRequired", formation.formation_id)
                .error};
  const MilitaryTacticLearningCatalog unselected;
  const auto &learning = context.military.battle_catalog.tactic_learning;
  const MilitaryTacticLearningState unselected_state;
  if (learning && learning->enabled && !context.learning)
    return {
        false,
        {},
        common_failure("FieldAbilityLearningStateRequired", session.battle_id)
            .error};
  if (historical_actor && (!actual_source || !observed_at))
    return {false,
            {},
            common_failure("FieldAbilityParticipationSourceRequired",
                           formation.formation_id)
                .error};
  auto effective = historical_actor
                       ? Outcome<StableId>{true, *historical_actor, {}}
                       : read_field_effective_commander(common, formation);
  if (!effective.ok)
    return {false, {}, effective.error};
  const auto *source = actual_source            ? actual_source
                       : session.started_source ? &*session.started_source
                                                : nullptr;
  const auto initial_boundary = npc_start
                                    ? GrowthFieldBoundaryKind::npc_started
                                    : GrowthFieldBoundaryKind::player_started;
  Integer preview_tick = common.tick();
  if (!source && !observed_at) {
    auto actual = read_field_actual_application_moment(common, context);
    if (!actual.ok)
      return {false, {}, actual.error};
    preview_tick = actual.value.half_day_tick;
  }
  return read_field_actor_abilities(
      common, context, context.military.battle_catalog,
      learning ? *learning : unselected,
      context.learning ? *context.learning : unselected_state, formation,
      session.sides[formation.side].commander_actor_id,
      observed_at              ? *observed_at
      : session.started_source ? session.started_observed_tick
                               : preview_tick,
      context.phase, source ? source->id : StableId{}, effective.value,
      source && source->kind == "field_battle_started" ? &initial_boundary
                                                       : nullptr,
      actual_consumer_at          ? actual_consumer_at
      : session.started_actual_at ? &*session.started_actual_at
                                  : nullptr);
}
bool same_tactic_source(const FieldAvailableTactic &a,
                        const FieldAvailableTactic &b) {
  return a.tactic_id == b.tactic_id &&
         a.learned_tactic_id == b.learned_tactic_id &&
         a.acquisition_fact_id == b.acquisition_fact_id && a.kind == b.kind &&
         a.point_cost == b.point_cost && a.power_bp == b.power_bp &&
         a.morale_effect == b.morale_effect;
}
} // namespace
Status validate_field_battle_start_ability_sources(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleSession &session) {
  for (const auto &[id, formation] : session.formations) {
    auto source = initial_ability_source(common, context, session, formation);
    if (!source.ok)
      return {false, {}, source.error};
  }
  return common_success();
}
Status initialize_field_battle_abilities(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    FieldBattleSession &session, const SourceRef &start_source,
    Integer observed_tick, const std::optional<StableId> &started_by,
    CheckpointMemoryCharge &owner) {
  auto actual_moment = read_field_actual_application_moment(common, context);
  if (!actual_moment.ok)
    return {false, {}, actual_moment.error};
  const Integer actual_tick = actual_moment.value.half_day_tick;
  if (observed_tick != actual_tick || start_source.domain != "military" ||
      start_source.kind != "field_battle_started" || start_source.id.empty() ||
      session.started_source || (started_by && !common.actor(*started_by)))
    return common_failure("FieldAbilityStartSourceRequired", session.battle_id);
  if ((session.state != FieldBattleState::Deploying &&
       session.state != FieldBattleState::Running) ||
      (session.started_at && (session.now.tick != session.started_at->tick ||
                              session.now.t != session.started_at->t)))
    return common_failure("FieldAbilityStartRequired", session.battle_id);

  CheckpointVector<std::pair<FieldFormation *, FieldFormationAbilityState>>
      prepared;
  prepared.reserve(session.formations.size());
  for (auto &[id, formation] : session.formations) {
    if (formation.ability)
      return common_failure("FieldAbilityAlreadyInitialized", id);
    auto source = initial_ability_source(common, context, session, formation,
                                         &start_source, observed_tick, nullptr,
                                         &actual_moment.value, !started_by);
    if (!source.ok)
      return {false, {}, source.error};


    (owner).grow(0);
    FieldFormationAbilityState ability;
    ability.commander_actor_id = std::move(source.value.commander_actor_id);
    ability.source_ref = start_source;
    ability.observed_tick = observed_tick;
    ability.traits = std::move(source.value.traits);
    ability.capacity = ability.points = source.value.tactic_capacity;
    ability.last_point_recovery = session.now;
    ability.tactics.reserve(source.value.tactics.size());
    for (auto &tactic : source.value.tactics)
      ability.tactics.push_back({std::move(tactic), {}});
    prepared.emplace_back(&formation, std::move(ability));
  }
  for (auto &[formation, ability] : prepared)
    formation->ability = std::move(ability);
  session.state = FieldBattleState::Running;
  session.started_at = session.now;


  (owner).grow(0);
  session.started_source = start_source;
  session.started_observed_tick = observed_tick;
  session.started_actual_at = actual_moment.value;
  session.started_by_actor_id = started_by;
  return common_success();
}
Status initialize_field_reinforcement_ability_in_candidate(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleWorldState &world, FieldBattleSession &session,
    FieldFormation &formation, CheckpointMemoryCharge &owner) {
  if (formation.ability || session.state != FieldBattleState::Running ||
      !context.actual_clock || !context.exact_contacts ||
      !context.exact_contacts->clock.bound_to(
          common, context.exact_contacts->military, world))
    return common_failure("FieldReinforcementAbilityOriginalOwnerRequired",
                          formation.formation_id);
  auto admission = read_field_reinforcement_admission_identity(
      common, world, session, formation);
  if (!admission.ok)
    return {false, {}, admission.error};
  auto now = read_field_actual_application_moment(common, context);
  if (!now.ok)
    return {false, {}, now.error};
  auto compared = compare_world_moments(now.value, admission.value->applied_at);
  if (!compared.ok)
    return {false, {}, compared.error};
  if (compared.value != 0 || session.now.tick != admission.value->at.tick ||
      session.now.t != admission.value->at.t)
    return common_failure("FieldReinforcementAbilityActualBoundaryRequired",
                          formation.formation_id);
  auto source = initial_ability_source(
      common, context, session, formation, &admission.value->application_source,
      admission.value->applied_at.half_day_tick, nullptr,
      &admission.value->applied_at);
  if (!source.ok)
    return {false, {}, source.error};


  (owner).grow(0);
  FieldFormationAbilityState ability;
  ability.commander_actor_id = std::move(source.value.commander_actor_id);
  ability.source_ref = admission.value->application_source;
  ability.observed_tick = admission.value->applied_at.half_day_tick;
  ability.traits = std::move(source.value.traits);
  ability.capacity = ability.points = source.value.tactic_capacity;
  ability.last_point_recovery = admission.value->at;
  ability.tactics.reserve(source.value.tactics.size());
  for (auto &tactic : source.value.tactics)
    ability.tactics.push_back({std::move(tactic), {}});
  formation.ability = std::move(ability);
  return common_success();
}
Outcome<FieldAbilitySourceBoundary> read_field_ability_source_boundary(
    const CommonStore &common, const FieldBattleWorldState &world,
    const FieldBattleSession &session, const FieldFormation &formation,
    const CurrentMomentRead *actual_clock) {
  // source_ref 指向真实参与应用头；穿界停止与真正进场不能合并为一次来源。
  const auto fail = [&]() -> Outcome<FieldAbilitySourceBoundary> {
    return {false,
            {},
            common_failure("FieldAbilityParticipationSourceRequired",
                           formation.formation_id)
                .error};
  };
  const auto session_row = world.sessions.find(session.battle_id);
  const auto formation_row = session.formations.find(formation.formation_id);
  if (session_row == world.sessions.end() || &session_row->second != &session ||
      formation_row == session.formations.end() ||
      &formation_row->second != &formation || !formation.ability ||
      !session.started_at || !session.started_source)
    return fail();
  const auto &ability = *formation.ability;
  const auto &source = ability.source_ref;
  const auto *head = common.state().facts.find(source.id);
  if (!head || source.domain != "military" || head->domain != source.domain ||
      head->kind != source.kind ||
      ability.observed_tick < session.created_tick ||
      (!actual_clock && ability.observed_tick > common.tick()))
    return fail();
  const auto valid_moment = [&](const WorldMoment &at) -> Status {
    if (at.half_day_tick != ability.observed_tick)
      return common_failure("FieldAbilityActualMomentMismatch", source.id);
    auto canonical = compare_world_moments(at, at);
    if (!canonical.ok)
      return {false, {}, canonical.error};
    if (!actual_clock)
      return common_success();
    auto now = actual_clock->read_current_moment();
    if (!now.ok)
      return {false, {}, now.error};
    auto order = compare_world_moments(at, now.value);
    if (!order.ok)
      return {false, {}, order.error};
    return order.value <= 0
               ? common_success()
               : common_failure("FieldAbilityFutureSourceMoment", source.id);
  };
  const auto &order = common.state().facts.order();
  const auto at = [&](const StableId &id) {
    return std::find(order.begin(), order.end(), id) - order.begin();
  };
  if (source.kind == "field_battle_session" ||
      source.kind == "field_battle_started") {
    if (!session.started_actual_at || source.id != session.started_source->id ||
        source.kind != session.started_source->kind ||
        ability.observed_tick != session.started_observed_tick ||
        formation.joined_at.tick != session.started_at->tick ||
        formation.joined_at.t != session.started_at->t ||
        at(source.id) < at(session.battle_id))
      return fail();
    auto moment = valid_moment(*session.started_actual_at);
    if (!moment.ok)
      return {false, {}, moment.error};
    return {true,
            {&source, ability.observed_tick, formation.joined_at,
             &*session.started_actual_at},
            {}};
  }
  if (source.kind != "field_battle_entered")
    return fail();
  auto admission = read_field_reinforcement_admission_identity(
      common, world, session, formation);
  if (!admission.ok)
    return {false, {}, admission.error};
  const auto &entry = *admission.value;
  if (entry.application_source.domain != source.domain ||
      entry.application_source.kind != source.kind ||
      entry.application_source.id != source.id ||
      entry.applied_at.half_day_tick != ability.observed_tick)
    return fail();
  auto moment = valid_moment(entry.applied_at);
  if (!moment.ok)
    return {false, {}, moment.error};
  return {
      true,
      {&source, ability.observed_tick, formation.joined_at, &entry.applied_at},
      {}};
}
Status validate_field_battle_initial_abilities(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleWorldState &world, const FieldBattleSession &session,
    const FieldBattleParameters &parameters) {
  if (!session.started_at || tactic_before(session.now, *session.started_at))
    return common_failure("FieldAbilityStartSourceRequired", session.battle_id);
  const auto *head = session.started_source
                         ? common.state().facts.find(session.started_source->id)
                         : nullptr;
  if (!head || session.started_source->domain != "military" ||
      head->domain != session.started_source->domain ||
      head->kind != session.started_source->kind ||
      head->kind != "field_battle_started" || !session.started_actual_at ||
      session.started_observed_tick < session.created_tick ||
      (!context.actual_clock && session.started_observed_tick > common.tick()))
    return common_failure("FieldAbilityStartSourceRequired", session.battle_id);
  for (const auto &[id, formation] : session.formations) {
    if (!formation.ability)
      return common_failure("FieldAbilityStateRequired", id);
    auto boundary = read_field_ability_source_boundary(
        common, world, session, formation, context.actual_clock);
    if (!boundary.ok)
      return {false, {}, boundary.error};
    auto source = initial_ability_source(
        common, context, session, formation, boundary.value.source,
        boundary.value.observed_tick, &formation.commander_actor_id,
        boundary.value.actual_at, !session.started_by_actor_id);
    if (!source.ok)
      return {false, {}, source.error};
    const auto &actual = *formation.ability;
    const FieldFormationAbilitySource *current_source = &source.value;
    Outcome<FieldFormationAbilitySource> replacement;
    const FieldCommanderChange *change = nullptr;
    if (!actual.commander_changes.empty()) {
      if (actual.commander_changes.size() != 1)
        return common_failure("FieldCommanderChangeRepeated", id);
      change = &actual.commander_changes.front();
      auto proof =
          read_field_commander_change_source(common, world, change->source_ref);
      if (!proof.ok)
        return {false, {}, proof.error};
      if (proof.value.formation != &formation ||
          tactic_before(session.now, change->at) ||
          tactic_before(change->at, formation.joined_at))
        return common_failure("FieldCommanderChangeSourceMismatch", id);
      replacement = read_field_commander_replacement_abilities(
          common, context, world, change->source_ref);
      if (!replacement.ok)
        return {false, {}, replacement.error};
      current_source = &replacement.value;
    }
    if (actual.source_ref.domain != "military" ||
        actual.source_ref.kind != boundary.value.source->kind ||
        actual.source_ref.id != boundary.value.source->id ||
        actual.observed_tick != boundary.value.observed_tick ||
        actual.commander_actor_id != current_source->commander_actor_id ||
        actual.traits != source.value.traits ||
        actual.capacity != source.value.tactic_capacity || actual.points < 0 ||
        actual.points > actual.capacity ||
        actual.tactics.size() != current_source->tactics.size())
      return common_failure("FieldAbilitySourceMismatch", id);
    Integer points = actual.capacity;
    FieldBattleTime recovered = formation.joined_at,
                    previous = formation.joined_at;
    const auto accrue = [&](const FieldBattleTime &to) -> Status {
      if (tactic_before(to, recovered))
        return common_failure("FieldAbilityRecoverySourceRequired", id);
      const auto elapsed =
          (__int128_t(to.tick) - recovered.tick) * parameters.window_s + to.t -
          recovered.t;
      const auto units = elapsed / (30 * 60);
      if (units * (30 * 60) > std::numeric_limits<Integer>::max())
        return common_failure("FieldBattleTimeOverflow", id);
      auto advanced = add_field_battle_seconds(
          recovered, static_cast<Integer>(units * (30 * 60)), parameters);
      if (!advanced.ok)
        return {false, {}, advanced.error};
      recovered = advanced.value;
      points = static_cast<Integer>(
          std::min<__int128_t>(actual.capacity, __int128_t(points) + units));
      return common_success();
    };
    std::map<StableId, FieldBattleTime> ready_times;
    std::set<StableId> activation_ids;
    for (const auto &activation : actual.active) {
      const auto &activation_source =
          change && !tactic_before(activation.started, change->at)
              ? *current_source
              : source.value;
      const auto definition =
          std::find_if(activation_source.tactics.begin(),
                       activation_source.tactics.end(), [&](const auto &row) {
                         return row.tactic_id == activation.tactic_id;
                       });
      if (activation.activation_id.empty() ||
          !activation_ids.insert(activation.activation_id).second ||
          activation.issuer_actor_id.empty() ||
          activation.commander_actor_id !=
              activation_source.commander_actor_id ||
          definition == activation_source.tactics.end() ||
          !same_tactic_source(activation.accepted_source, *definition) ||
          activation.kind != definition->kind ||
          tactic_before(activation.started, previous) ||
          tactic_before(session.now, activation.started) ||
          (activation.first_contact_consumed &&
           activation.kind != FieldTacticKind::Charge) ||
          activation.remaining_pulses < 0 || activation.remaining_pulses > 3 ||
          (activation.kind != FieldTacticKind::Volley &&
           activation.remaining_pulses != 0))
        return common_failure("FieldTacticApplicationSourceMismatch",
                              activation.activation_id);
      auto accrued = accrue(activation.started);
      if (!accrued.ok)
        return accrued;
      const auto cooldown = ready_times.find(activation.tactic_id);
      if (points < definition->point_cost ||
          (cooldown != ready_times.end() &&
           tactic_before(activation.started, cooldown->second)))
        return common_failure("FieldTacticBalanceMismatch",
                              activation.activation_id);
      points -= definition->point_cost;
      auto ready = add_field_battle_seconds(
          activation.started, field_tactic_cooldown_s(activation.kind),
          parameters);
      auto end = add_field_battle_seconds(
          activation.started, field_tactic_duration_s(activation.kind),
          parameters);
      if (!ready.ok || !end.ok)
        return {false, {}, !ready.ok ? ready.error : end.error};
      ready_times[activation.tactic_id] = ready.value;
      const bool lasting = field_tactic_duration_s(activation.kind) != 0;
      if (bool(activation.ends) != lasting ||
          (activation.ends && (activation.ends->tick != end.value.tick ||
                               activation.ends->t != end.value.t)))
        return common_failure("FieldTacticDurationMismatch",
                              activation.activation_id);
      if (activation.ai_issued) {
        // Authority is the real accepted AI command, not today's controller.
        const FieldCouncilFact *actual = nullptr;
        for (const auto &[key, fact] : world.council_facts) {
          if (fact.kind != FieldCouncilFactKind::AiOrder ||
              fact.battle_id != session.battle_id ||
              fact.formation_id != formation.formation_id ||
              (activation.order_id
                   ? fact.order_id != activation.order_id
                   : fact.activation_id != activation.activation_id))
            continue;
          if (actual || key != fact.fact_id ||
              fact.player_actor_id != activation.issuer_actor_id ||
              fact.player_command_kind != "field_battle_tactic")
            return common_failure("FieldTacticAiSourceMismatch",
                                  activation.activation_id);
          auto source = read_field_ai_command_source(common, world, key,
                                                     context.original_life);
          if (!source.ok)
            return {false, {}, source.error};
          if (source.value != &fact)
            return common_failure("FieldTacticAiSourceMismatch",
                                  activation.activation_id);
          actual = &fact;
        }
        if (!actual)
          return common_failure("FieldTacticAiSourceRequired",
                                activation.activation_id);
      }
      previous = activation.started;
    }
    auto accrued = accrue(session.now);
    if (!accrued.ok)
      return accrued;
    if (actual.points != points ||
        actual.last_point_recovery.tick != recovered.tick ||
        actual.last_point_recovery.t != recovered.t)
      return common_failure("FieldTacticBalanceMismatch", id);
    for (std::size_t i = 0; i < actual.tactics.size(); ++i) {
      const auto &tactic = actual.tactics[i];
      const auto ready = ready_times.find(tactic.source.tactic_id);
      if (!same_tactic_source(tactic.source, current_source->tactics[i]) ||
          bool(tactic.ready_at) != (ready != ready_times.end()) ||
          (tactic.ready_at && (tactic.ready_at->tick != ready->second.tick ||
                               tactic.ready_at->t != ready->second.t)))
        return common_failure("FieldAbilitySourceMismatch", id);
    }
    std::set<StableId> active_statuses;
    for (const auto &status : formation.statuses) {
      if (status.kind != FieldStatusKind::Charging &&
          status.kind != FieldStatusKind::Guarding)
        continue;
      const auto activation = std::find_if(
          actual.active.begin(), actual.active.end(), [&](const auto &entry) {
            return entry.activation_id == status.source_id;
          });
      if (activation == actual.active.end() || !activation->ends ||
          !status.until || status.until->tick != activation->ends->tick ||
          status.until->t != activation->ends->t ||
          !tactic_before(session.now, *status.until) ||
          (status.kind == FieldStatusKind::Charging
               ? activation->kind != FieldTacticKind::Charge
               : activation->kind != FieldTacticKind::Guard) ||
          !active_statuses.insert(status.source_id).second)
        return common_failure("FieldTacticStatusMismatch", status.source_id);
    }
    for (const auto &activation : actual.active)
      if (activation.ends && tactic_before(session.now, *activation.ends) &&
          !active_statuses.contains(activation.activation_id))
        return common_failure("FieldTacticStatusMissing",
                              activation.activation_id);
  }
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
namespace {
// §13.3 第一版固定表；持续/冷却不是旧 hex 回合数，发动不消费随机流。
Integer field_tactic_cooldown_s(FieldTacticKind kind) {
  return kind == FieldTacticKind::Volley ? 15 * 60 : 20 * 60;
}
Integer field_tactic_duration_s(FieldTacticKind kind) {
  return kind == FieldTacticKind::Charge  ? 3 * 60
         : kind == FieldTacticKind::Guard ? 10 * 60
                                          : 0;
}
bool tactic_before(const FieldBattleTime &a, const FieldBattleTime &b) {
  return std::tie(a.tick, a.t) < std::tie(b.tick, b.t);
}
// 仅原已冻结表中的唯一真实战法可发动；重复键不随取第一条掩盖。
const FieldTacticReadiness *field_tactic_ready(const FieldFormation &formation,
                                               const StableId &id) {
  if (!formation.ability)
    return nullptr;
  const FieldTacticReadiness *found = nullptr;
  for (const auto &tactic : formation.ability->tactics)
    if (tactic.source.tactic_id == id) {
      if (found)
        return nullptr;
      found = &tactic;
    }
  return found;
}
// 所有普通深成员在首次复制前按原字段计划收费，租约借真实 World owner。
void reserve_field_activation_fields(
    const FieldBattleSession &session, const FieldFormation &formation,
    const FieldAvailableTactic &source, const StableId &activation_id,
    const StableId &issuer, const StableId &commander, const StableId *order,
    bool adds_status, CheckpointMemoryCharge &owner) {

  const auto &old = formation.ability->active;


  (owner).grow(0);
}
} // namespace
Status
validate_field_tactic_activation(const CommonStore &common,
                                 const FieldFormation &formation,
                                 const FieldBattleTime &at, const StableId &id,
                                 const FieldBattleParameters &parameters) {
  if (formation.state == FieldFormationState::Withdrawn ||
      formation.state == FieldFormationState::Destroyed || !formation.ability)
    return common_failure("FieldTacticNoCommander", formation.formation_id);
  auto commander = read_field_effective_commander(common, formation);
  if (!commander.ok)
    return {false, {}, commander.error};
  // 合法副手可用自身能力；尚未切到该真实来源时不沿逝者快照发动。
  if (formation.ability->commander_actor_id != commander.value)
    return common_failure("FieldAbilityCommanderSourceRequired",
                          formation.formation_id);
  const auto *tactic = field_tactic_ready(formation, id);
  if (!tactic || !category_permits(tactic->source.kind, formation.category))
    return common_failure("FieldTacticUnknown", id);
  if (formation.ability->points < tactic->source.point_cost)
    return common_failure("FieldTacticNoPoints", id);
  if (tactic->ready_at && tactic_before(at, *tactic->ready_at))
    return common_failure("FieldTacticCooling", id);
  if (formation.state == FieldFormationState::Routing &&
      tactic->source.kind != FieldTacticKind::Rally)
    return common_failure("FieldFormationRouting", formation.formation_id);
  auto ready = add_field_battle_seconds(
      at, field_tactic_cooldown_s(tactic->source.kind), parameters);
  if (!ready.ok)
    return {false, {}, ready.error};
  return common_success();
}
Outcome<StableId> activate_field_tactic_in_candidate(
    CommonStore &common, FieldBattleSession &session, FieldFormation &formation,
    const StableId &issuer, const StableId &id, const StableId *order_id,
    bool ai_issued, const FieldBattleParameters &parameters,
    CheckpointMemoryCharge &owner) {
  auto fail = [&](const char *code) -> Outcome<StableId> {
    return {false, {}, common_failure(code, formation.formation_id).error};
  };
  auto stored = session.formations.find(formation.formation_id);
  if (session.state != FieldBattleState::Running ||
      stored == session.formations.end() || &stored->second != &formation ||
      !common.actor(issuer))
    return fail("FieldTacticSourceRequired");
  auto valid = validate_field_tactic_activation(common, formation, session.now,
                                                id, parameters);
  if (!valid.ok)
    return {false, {}, valid.error};
  if (order_id) {
    const auto order = session.orders.find(*order_id);
    const auto *intent =
        order == session.orders.end()
            ? nullptr
            : std::get_if<FieldTacticIntent>(&order->second.payload);
    if (!intent || intent->tactic_id != id ||
        order->second.target_formation_id != formation.formation_id ||
        order->second.issuer_actor_id != issuer ||
        order->second.ai_issued != ai_issued ||
        order->second.status != FieldOrderStatus::Adopted)
      return fail("FieldTacticOrderSourceRequired");
  } else {
    const auto *direct = std::get_if<FieldDirectControl>(&formation.control);
    if (!direct || direct->controller_actor_id != issuer ||
        (ai_issued &&
         (!std::holds_alternative<FieldAiController>(
              session.sides[formation.side].controller) ||
          session.sides[formation.side].commander_actor_id != issuer)) ||
        (!ai_issued && common.state().metadata.player_actor_id != issuer))
      return fail("FieldOrderNoAuthority");
  }
  auto &ability = *formation.ability;
  auto *ready =
      const_cast<FieldTacticReadiness *>(field_tactic_ready(formation, id));
  const auto cooldown = add_field_battle_seconds(
      session.now, field_tactic_cooldown_s(ready->source.kind), parameters);
  auto ends = add_field_battle_seconds(
      session.now, field_tactic_duration_s(ready->source.kind), parameters);
  if (!cooldown.ok || !ends.ok)
    return {false, {}, !cooldown.ok ? cooldown.error : ends.error};
  auto generated = common.next_id("field_tactic");
  if (!generated.ok)
    return {false, {}, generated.error};
  const bool status = ready->source.kind == FieldTacticKind::Charge ||
                      ready->source.kind == FieldTacticKind::Guard;
  reserve_field_activation_fields(
      session, formation, ready->source, generated.value, issuer,
      ability.commander_actor_id, order_id, status, owner);
  FieldTacticActivation activation;
  activation.activation_id = checkpoint_fresh_string_copy(generated.value);
  activation.tactic_id = checkpoint_fresh_string_copy(id);
  activation.issuer_actor_id = checkpoint_fresh_string_copy(issuer);
  activation.commander_actor_id =
      checkpoint_fresh_string_copy(ability.commander_actor_id);
  activation.accepted_source = ready->source;
  activation.ai_issued = ai_issued;
  activation.kind = ready->source.kind;
  activation.started = session.now;
  if (field_tactic_duration_s(activation.kind))
    activation.ends = ends.value;
  if (activation.kind == FieldTacticKind::Volley)
    activation.remaining_pulses = 3;
  if (order_id)
    activation.order_id = checkpoint_fresh_string_copy(*order_id);
  std::vector<FieldTacticActivation> activations;
  activations.reserve(ability.active.size() + 1);
  for (const auto &old : ability.active)
    activations.push_back(old);
  activations.push_back(std::move(activation));
  if (status) {
    std::vector<FieldFormationStatus> statuses;
    statuses.reserve(formation.statuses.size() + 1);
    for (const auto &old : formation.statuses)
      statuses.push_back(old);
    statuses.push_back({ready->source.kind == FieldTacticKind::Charge
                            ? FieldStatusKind::Charging
                            : FieldStatusKind::Guarding,
                        checkpoint_fresh_string_copy(generated.value),
                        ends.value});
    formation.statuses = std::move(statuses);
  }
  std::vector<FieldBattleEvent> events;
  events.reserve(session.events.size() + 1);
  for (const auto &old : session.events)
    events.push_back(old);
  session.events = std::move(events);
  ability.active = std::move(activations);
  ability.points -= ready->source.point_cost;
  ready->ready_at = cooldown.value;
  if (ready->source.kind == FieldTacticKind::Rally)
    formation.morale = static_cast<Integer>(std::min<__int128_t>(
        formation.morale_start,
        __int128_t(formation.morale) + ready->source.morale_effect));
  FieldBattleEvent event;
  event.kind = field_event_kind::Tactic;
  event.formation_id = checkpoint_fresh_string_copy(formation.formation_id);
  event.source_id = checkpoint_fresh_string_copy(generated.value);
  event.side = formation.side;
  event.tactic = ready->source.kind;
  event.success = true;
  valid = append_field_battle_event(session, std::move(event));
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(generated.value), {}};
}
Status recover_field_tactic_points_in_candidate(
    FieldBattleSession &session, const FieldBattleParameters &parameters) {
  const bool final_boundary = session.state == FieldBattleState::Ended &&
                              session.result &&
                              session.result->ended.tick == session.now.tick &&
                              session.result->ended.t == session.now.t;
  if ((session.state != FieldBattleState::Running && !final_boundary) ||
      parameters.window_s <= 0)
    return common_failure("FieldBattleNotRunning", session.battle_id);
  for (const auto &[id, formation] : session.formations) {
    if (!formation.ability || formation.ability->capacity < 0 ||
        formation.ability->points < 0 ||
        formation.ability->points > formation.ability->capacity ||
        tactic_before(session.now, formation.ability->last_point_recovery))
      return common_failure("FieldAbilityRecoverySourceRequired", id);
  }
  for (auto &[id, formation] : session.formations) {
    auto &ability = *formation.ability;
    formation.statuses.erase(
        std::remove_if(formation.statuses.begin(), formation.statuses.end(),
                       [&](const auto &status) {
                         return (status.kind == FieldStatusKind::Charging ||
                                 status.kind == FieldStatusKind::Guarding) &&
                                status.until &&
                                !tactic_before(session.now, *status.until);
                       }),
        formation.statuses.end());
    const auto elapsed =
        (__int128_t(session.now.tick) - ability.last_point_recovery.tick) *
            parameters.window_s +
        session.now.t - ability.last_point_recovery.t;
    const auto units = elapsed / (30 * 60);
    if (units == 0)
      continue;
    if (units * (30 * 60) > std::numeric_limits<Integer>::max())
      return common_failure("FieldBattleTimeOverflow", id);
    auto at = add_field_battle_seconds(ability.last_point_recovery,
                                       static_cast<Integer>(units * (30 * 60)),
                                       parameters);
    if (!at.ok)
      return {false, {}, at.error};
    ability.points = static_cast<Integer>(std::min<__int128_t>(
        ability.capacity, __int128_t(ability.points) + units));
    ability.last_point_recovery = at.value;
  }
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<FieldFormationAbilitySource> read_field_commander_replacement_abilities(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    const FieldBattleWorldState &world, const SourceRef &source) {
  auto actual = read_field_commander_change_source(common, world, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  const auto &formation = *actual.value.formation;
  const auto &change = *actual.value.change;
  const auto &session = *actual.value.session;
  if (formation.side < 0 || formation.side > 1)
    return {
        false,
        {},
        common_failure("FieldCommanderChangeSourceMismatch", source.id).error};
  const MilitaryTacticLearningCatalog unselected;
  const MilitaryTacticLearningState unselected_state;
  const auto &learning = context.military.battle_catalog.tactic_learning;
  if (learning && learning->enabled && !context.learning)
    return {
        false,
        {},
        common_failure("FieldAbilityLearningStateRequired", source.id).error};
  const auto boundary = GrowthFieldBoundaryKind::commander_changed;
  return read_field_actor_abilities(
      common, context, context.military.battle_catalog,
      learning ? *learning : unselected,
      context.learning ? *context.learning : unselected_state, formation,
      session.sides[formation.side].commander_actor_id, change.observed_tick,
      context.phase, source.id, change.commander_actor_id, &boundary,
      &change.applied_at);
}
Status apply_field_commander_ability_replacement_in_candidate(
    const CommonStore &common, const FieldBattleAbilityReadContext &context,
    FieldBattleWorldState &world, const SourceRef &source,
    const FieldBattleParameters &parameters, CheckpointMemoryCharge &owner) {
  auto actual = read_field_commander_change_source(common, world, source);
  if (!actual.ok)
    return {false, {}, actual.error};
  auto fresh = read_field_commander_replacement_abilities(common, context,
                                                          world, source);
  if (!fresh.ok)
    return {false, {}, fresh.error};
  auto &formation = world.sessions.at(actual.value.session->battle_id)
                        .formations.at(actual.value.formation->formation_id);
  auto &ability = *formation.ability;
  const auto &change = *actual.value.change;
  if ((ability.commander_actor_id != change.previous_actor_id &&
       ability.commander_actor_id != change.commander_actor_id) ||
      ability.capacity != fresh.value.tactic_capacity || ability.points < 0 ||
      ability.points > ability.capacity)
    return common_failure("FieldCommanderAbilityReplacementRequired",
                          source.id);


  (owner).grow(0);
  std::vector<FieldTacticReadiness> ready;
  ready.reserve(fresh.value.tactics.size());
  for (auto &tactic : fresh.value.tactics) {
    std::optional<FieldBattleTime> cooldown;
    for (const auto &activation : ability.active) {
      if (activation.tactic_id != tactic.tactic_id)
        continue;
      auto until = add_field_battle_seconds(
          activation.started, field_tactic_cooldown_s(activation.kind),
          parameters);
      if (!until.ok)
        return {false, {}, until.error};
      if (!cooldown || tactic_before(*cooldown, until.value))
        cooldown = until.value;
    }
    ready.push_back({std::move(tactic), cooldown});
  }
  if (ability.commander_actor_id == change.commander_actor_id) {
    if (ability.tactics.size() != ready.size())
      return common_failure("FieldCommanderAbilityReplacementMismatch",
                            source.id);
    for (std::size_t i = 0; i < ready.size(); ++i) {
      const auto &a = ability.tactics[i];
      const auto &b = ready[i];
      if (!same_tactic_source(a.source, b.source) ||
          bool(a.ready_at) != bool(b.ready_at) ||
          (a.ready_at && (a.ready_at->tick != b.ready_at->tick ||
                          a.ready_at->t != b.ready_at->t)))
        return common_failure("FieldCommanderAbilityReplacementMismatch",
                              source.id);
    }
    return common_success();
  }
  // No points refill, passive recalculation, old activation deletion or actor
  // relabeling. Only this actor's actual acquired tactics become available.
  ability.commander_actor_id = change.commander_actor_id;
  ability.tactics = std::move(ready);
  return common_success();
}
} // namespace hanmo::resident

namespace hanmo::resident {
Outcome<WorldMoment> read_field_actual_application_moment(
    const CommonStore &common, const FieldBattleAbilityReadContext &context) {
  if (!context.actual_clock)
    return {false, {}, common_failure("FieldActualWorldClockRequired").error};
  auto actual = context.actual_clock->read_current_moment();
  if (!actual.ok)
    return actual;
  auto valid = compare_world_moments(actual.value, actual.value);
  if (!valid.ok)
    return {false, {}, valid.error};
  // The central S0 contact-install stack exposes its actual phase moment.
  // A borrowed phase outside that stack must not turn Common.tick into M.
  if (context.phase) {
    if (common.tick() < 0 || common.tick() >= 9007199254740991LL ||
        context.phase->from_tick != common.tick() ||
        context.phase->to_tick != common.tick() + 1)
      return {false, {}, common_failure("FieldActualWorldClockMismatch").error};
    auto lower = compare_world_moments(
        actual.value, WorldMoment{context.phase->from_tick, {}});
    auto upper = compare_world_moments(actual.value,
                                       WorldMoment{context.phase->to_tick, {}});
    if (!lower.ok)
      return {false, {}, lower.error};
    if (!upper.ok)
      return {false, {}, upper.error};
    if (lower.value < 0 || upper.value > 0)
      return {false, {}, common_failure("FieldActualWorldClockMismatch").error};
  } else if (actual.value.half_day_tick != common.tick()) {
    return {false, {}, common_failure("FieldActualWorldClockMismatch").error};
  }
  return actual;
}
} // namespace hanmo::resident
