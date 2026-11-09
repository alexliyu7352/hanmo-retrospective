// 个人战真实入场、退出和一次性人物后果；共享钱包、背包、事实编号始终通过共同存储。
#include "personal_rules_internal.hpp"
#include "personal_simultaneous_outcomes.hpp"
#include "personal_battle_lifecycle.hpp"
#include "resident/domains/actor_foundation/personal_injury_write_v1.hpp"
#include "resident/domains/actor_foundation/personal_initial_attributes_v1.hpp"
#include "resident/domains/interaction/occupancy/actor_social_occupancy.hpp"
#include <set>
namespace hanmo::resident {
namespace {
using namespace personal_detail;
// 旧十项与显式六项扩展都读本人累计XP，未知名称不会退化到人物武力或补默认值。
Integer xp(const ActorSkills &s, const StableId &id) {
  if (id == "infantry")
    return s.infantry;
  if (id == "cavalry")
    return s.cavalry;
  if (id == "archery")
    return s.archery;
  if (id == "martial")
    return s.martial;
  if (id == "tactics")
    return s.tactics;
  if (id == "debate")
    return s.debate;
  if (id == "negotiation")
    return s.negotiation;
  if (id == "rites")
    return s.rites;
  if (id == "commerce")
    return s.commerce;
  if (id == "transport")
    return s.transport;
  if (s.extended) {
    if (id == "naval")
      return s.extended->naval;
    if (id == "espionage")
      return s.extended->espionage;
    if (id == "fortification")
      return s.extended->fortification;
    if (id == "agriculture")
      return s.extended->agriculture;
    if (id == "medicine")
      return s.extended->medicine;
    if (id == "forging")
      return s.extended->forging;
  }
  return -1;
}
// 原选择引用仍须当下真实自有且未预留；旧装备引用不产生替身物品。
StableId equipped(const CommonStore &common, const PersonalCatalog &c,
                  const StableId &id) {
  auto a = common.state().actors.find(id);
  if (!a || !c.weapons.find(a->combat.weapon_id))
    return {};
  auto count = common.count(a->inventory_id, a->combat.weapon_id,
                            {{Owner::Kind::actor, id, {}}});
  return count.ok && count.value > 0 ? a->combat.weapon_id : StableId{};
}
// 无甲保字段缺省，库存引用损坏与未知护甲定义必须拒绝。
Outcome<std::pair<StableId, Integer>>
armor(const CommonStore &common, const PersonalCatalog &c, const StableId &id) {
  auto a = common.state().actors.find(id);
  if (!a)
    return fail<std::pair<StableId, Integer>>("UnknownActor");
  auto chosen = a->combat.armor_id.value_or("");
  if (chosen.empty())
    return {true, {"", 0}, {}};
  auto defense = c.armor_defenses.find(chosen);
  if (!defense)
    return fail<std::pair<StableId, Integer>>("NotAnArmor");
  if (!common.inventory(a->inventory_id))
    return fail<std::pair<StableId, Integer>>("MissingPersonalInventory");
  auto count =
      common.count(a->inventory_id, chosen, {{Owner::Kind::actor, id, {}}});
  if (!count.ok)
    return {false, {}, count.error};
  return {true,
          count.value > 0 ? std::make_pair(chosen, *defense)
                          : std::make_pair(StableId{}, Integer{0}),
          {}};
}
// 显式目录选择而非指针选择；旧profile不承担新读口义务。
Status initial_attribute_binding(const CommonStore &common,
                                 const PersonalState &state,
                                 const PersonalCatalog &catalog,
                                 const PersonalAccess &access) {
  if (access.selected_injury_sources == 0)
    return access.initial_attributes
               ? common_failure("UnexpectedPersonalInitialAttributeSource")
               : common_success();
  if (access.selected_injury_sources != 1 ||
      access.selected_battle_lifecycle != 1 || !access.initial_attributes ||
      !access.initial_attributes->bound_to(common, state, catalog))
    return common_failure("PersonalInitialAttributeSourcesRequired", {},
                          ErrorKind::unsupported_capability);
  return access.initial_attributes->validate_context();
}
// 入场前把命中与护甲来源全部读齐，保证失败早于费用和编号写入。
Status ready_snapshots(const CommonStore &common, const PersonalState &state,
                       const PersonalCatalog &c, const PersonalAccess &access,
                       const PersonalObjective &o,
                       const OrderedTable<Integer> &sides) {
  auto binding = initial_attribute_binding(common, state, c, access);
  if (!binding.ok)
    return binding;
  if (o.hit_model == "skill_v1" && !common.state().rng.find("duel"))
    return common_failure("InvalidDuelRandomState");
  for (auto &row : sides.rows()) {
    auto a = common.state().actors.find(row.first);
    if (!a)
      return common_failure("UnknownCombatant");
    if (o.hit_model == "skill_v1" &&
        (personal_skill_level(a->skills.martial) < 0 ||
         personal_skill_level(a->skills.archery) < 0))
      return common_failure("InvalidHitSkillExperience");
    if (access.selected_injury_sources == 1) {
      auto actual = access.initial_attributes->current(row.first);
      if (!actual.ok)
        return {false, {}, actual.error};
      if (actual.value.hp != a->hp || actual.value.fatigue != a->fatigue ||
          actual.value.injury != a->injury)
        return common_failure("PersonalInitialCurrentBodyMismatch", row.first);
    }
    auto equipped_armor = armor(common, c, row.first);
    if (!equipped_armor.ok)
      return {false, {}, equipped_armor.error};
  }
  return common_success();
}
// 状态相等比较只用实际字段值，Dictionary键序不参与AI合法性。
bool equal_plan(const PersonalPlan &a, const PersonalPlan &b) {
  return a.destination == b.destination && a.heading == b.heading &&
         a.action == b.action && a.target_id == b.target_id &&
         a.skill_id == b.skill_id;
}
// 与原状态校验一致，事实来源只比较当前拥有的业务值。
bool equal_source(const PersonalSource &a, const PersonalSource &b) {
  if (a.kind != b.kind || a.encounter_id != b.encounter_id ||
      bool(a.context) != bool(b.context))
    return false;
  if (!a.context)
    return true;
  auto &x = *a.context;
  auto &y = *b.context;
  return x.place_id == y.place_id &&
         x.provider_actor_id == y.provider_actor_id && x.role_id == y.role_id &&
         x.kind == y.kind && x.encounter_id == y.encounter_id;
}
} // namespace
// 零到四级门槛沿旧唯一累计经验规则，不存第二份等级账。
Integer personal_skill_level(Integer value) {
  if (value < 0 || value > 1200)
    return -1;
  return value >= 1200  ? 4
         : value >= 650 ? 3
         : value >= 300 ? 2
         : value >= 100 ? 1
                        : 0;
}
// 显式前置失败保留原码与所需等级，旧技能没有新门槛。
Status personal_skill_eligible(const ActorRecord &actor,
                               const PersonalSkill &skill) {
  if (!skill.required_skill_level)
    return common_success();
  auto &required = *skill.required_skill_level;
  auto actual = personal_skill_level(xp(actor.skills, required.skill_id));
  const bool legacy =
      required.skill_id == "infantry" || required.skill_id == "cavalry" ||
      required.skill_id == "archery" || required.skill_id == "martial" ||
      required.skill_id == "tactics" || required.skill_id == "debate" ||
      required.skill_id == "negotiation" || required.skill_id == "rites" ||
      required.skill_id == "commerce" || required.skill_id == "transport";
  const bool extended =
      actor.skills.extended &&
      (required.skill_id == "naval" || required.skill_id == "espionage" ||
       required.skill_id == "fortification" ||
       required.skill_id == "agriculture" || required.skill_id == "medicine" ||
       required.skill_id == "forging");
  if (required.level < 1 || required.level > 4 || (!legacy && !extended))
    return common_failure("InvalidSkillRequirement");
  if (actual < 0)
    return common_failure("InvalidSkillExperience");
  if (actual < required.level) {
    auto failure = common_failure("SkillLevelRequired");
    failure.error.skill_id = required.skill_id;
    failure.error.required_level = required.level;
    failure.error.actual_level = actual;
    return failure;
  }
  return common_success();
}
// 合格集合保人物学习原序和熟练度，未满足新前置的记录只从本场快照过滤。
OrderedTable<Integer> personal_learned_snapshot(const ActorRecord &actor,
                                                const PersonalCatalog &c) {
  auto learned = actor.combat.learned;
  auto keys = learned.order();
  for (auto &id : keys) {
    auto s = c.skills.find(id);
    if (s && !personal_skill_eligible(actor, *s).ok)
      learned.erase(id);
  }
  return learned;
}
// 未关闭会话占用所有原参与者，退出和倒地不会提前释放世界占用。
bool personal_occupied(const PersonalState &s, const StableId &id) {
  for (auto &row : s.battles.rows())
    if (row.second.phase != "closed" && row.second.units.find(id))
      return true;
  return false;
}
bool personal_pauses_world(const PersonalState &s) {
  for (auto &row : s.battles.rows())
    if (row.second.phase != "closed")
      return true;
  return false;
}
// 四种命令保持个人战早门首错，真实来源不得由请求伪造参战者或终局。
Outcome<PersonalQuote>
preview_personal(const CommonStore &common, const PersonalState &state,
                 const PersonalCatalog &c, const InteractionState &interaction,
                 const InteractionCatalog &ic, const InteractionAccess &ia,
                 const PersonalAccess &access, const StableId &actor,
                 const PersonalCommand &cmd) {
  if (cmd.kind != "duel_start" && cmd.kind != "encounter_fight" &&
      cmd.kind != "battle_plan" && cmd.kind != "battle_close")
    return fail<PersonalQuote>("UnsupportedAction");
  if (!c.enabled)
    return fail<PersonalQuote>("PersonalCombatUnavailable");
  auto attribute_binding = initial_attribute_binding(common, state, c, access);
  if (!attribute_binding.ok)
    return {false, {}, attribute_binding.error};
  if (access.selected_injury_sources == 1) {
    auto original = validate_personal_initial_attributes_sources_v1(
        common, state, c, *access.initial_attributes);
    if (!original.ok)
      return {false, {}, original.error};
  }
  auto a = common.state().actors.find(actor);
  if (!a || !a->alive)
    return fail<PersonalQuote>("ActorUnavailable");
  PersonalQuote q;
  q.kind = cmd.kind;
  if (cmd.kind == "battle_plan" || cmd.kind == "battle_close") {
    auto b = state.battles.find(cmd.battle_id);
    if (!b)
      return fail<PersonalQuote>("UnknownBattle");
    if (b->controller_id != actor)
      return fail<PersonalQuote>("NotBattleController");
    q.battle_id = b->id;
    if (cmd.kind == "battle_close") {
      if (b->phase != "result" || !b->applied)
        return fail<PersonalQuote>("NoBattleResult");
      q.result = std::make_shared<const PersonalResultProjection>(
          personal_result_projection(*b, c));
      return {true, q, {}};
    }
    if (b->phase != "planning")
      return fail<PersonalQuote>("BattleAlreadyResolved");
    if (cmd.input_error)
      return fail<PersonalQuote>(*cmd.input_error);
    if (!cmd.plan)
      return fail<PersonalQuote>("InvalidPlan");
    auto valid = personal_validate_plan(*b, c, actor, *cmd.plan);
    if (!valid.ok)
      return {false, {}, valid.error};
    q.plan = valid.value;
    q.tactical = std::make_shared<const PersonalPlanProjection>(
        personal_plan_projection(*b, c, actor, valid.value));
    return {true, q, {}};
  }
  if (personal_pauses_world(state) ||
      (access.pauses_world && access.pauses_world()))
    return fail<PersonalQuote>("BattlePending");
  q.controller_id = actor;
  if (cmd.kind == "duel_start") {
    InteractionContext context = cmd.context.value_or(InteractionContext{});
    if (!cmd.context)
      context.input_error = "MissingInteractionContext";
    auto contact = validate_interaction(common, interaction, ic, ia, actor,
                                        context, "duel_start");
    if (!contact.ok)
      return {false, {}, contact.error};
    auto terms = c.sparring.find(contact.value.role_id);
    if (!terms)
      return fail<PersonalQuote>("NoSparringAgreement");
    if (a->injury >= 3)
      return fail<PersonalQuote>("SevereInjuryCannotSpar");
    q.duration = 1;
    q.fee = terms->fee;
    q.wallet_id = terms->wallet_id;
    q.battlefield_id = terms->battlefield_id;
    q.objective_id = terms->objective_id;
    q.source.kind = "role";
    q.source.context = contact.value;
    q.sides.insert(actor, 0);
    q.sides.insert(contact.value.provider_actor_id, 1);
    if (equipped(common, c, actor).empty())
      return fail<PersonalQuote>("WeaponRequired");
    auto spendable = common.spendable(a->wallet_id);
    if (!spendable.ok)
      return {false, {}, spendable.error};
    if (spendable.value < q.fee)
      return fail<PersonalQuote>("InsufficientFunds");
  } else {
    auto meeting = preview_interaction(common, interaction, ic, ia, actor,
                                       EncounterLeave{cmd.encounter_id});
    if (!meeting.ok)
      return {false, {}, meeting.error};
    const EncounterRecord *encounter = nullptr;
    for (auto &e : interaction.encounters)
      if (e.id == meeting.value.encounter_id)
        encounter = &e;
    if (!encounter)
      return fail<PersonalQuote>("UnknownEncounter");
    if (encounter->leaders[0] != actor && encounter->leaders[1] != actor)
      return fail<PersonalQuote>("NotPartyLeader");
    auto other = encounter->leaders[0] == actor ? encounter->leaders[1]
                                                : encounter->leaders[0];
    auto other_actor = common.state().actors.find(other);
    auto prototype = other_actor && other_actor->prototype_id
                         ? *other_actor->prototype_id
                         : other;
    auto conflict = c.road_conflicts.find(prototype);
    if (!conflict)
      return fail<PersonalQuote>("NoHostileEncounter");
    q.fee = conflict->fee;
    q.wallet_id = conflict->wallet_id;
    q.battlefield_id = conflict->battlefield_id;
    q.objective_id = conflict->objective_id;
    q.source.kind = "encounter";
    q.source.encounter_id = encounter->id;
    if (auto people = common.members(actor))
      for (auto &id : *people)
        q.sides.set(id, 0);
    if (auto people = common.members(other))
      for (auto &id : *people)
        q.sides.set(id, 1);
    if (access.escort_objective) {
      auto protected_goal = access.escort_objective(actor, q.sides);
      if (protected_goal) {
        q.protected_id = protected_goal->first;
        q.objective_id = protected_goal->second;
      }
    }
  }
  auto arena = c.arenas.find(q.battlefield_id);
  auto objective = c.objectives.find(q.objective_id);
  if (!arena || !objective)
    return fail<PersonalQuote>("UnknownBattleDefinition");
  if (cmd.kind == "duel_start" && objective->kind != "sparring")
    return fail<PersonalQuote>("WrongSparringObjective");
  if (objective->kind == "escort" && q.protected_id.empty())
    return fail<PersonalQuote>("ProtectedPersonMissing");
  Integer capacity[2] = {0, 0};
  for (auto &spawn : arena->spawns)
    if (spawn.side >= 0 && spawn.side <= 1)
      ++capacity[spawn.side];
  for (auto &row : q.sides.rows()) {
    auto person = common.state().actors.find(row.first);
    if (!person || !person->alive || !person->available ||
        personal_occupied(state, row.first) ||
        (access.occupied && access.occupied(row.first)))
      return fail<PersonalQuote>("ParticipantBusy");
    --capacity[row.second];
  }
  if (capacity[0] < 0 || capacity[1] < 0)
    return fail<PersonalQuote>("BattlefieldCapacity");
  auto ready = ready_snapshots(common, state, c, access, *objective, q.sides);
  if (!ready.ok)
    return {false, {}, ready.error};
  return {true, q, {}};
}
// 费用、人物后果、事实和接触恢复在同一个外层候选内落地，失败不发布部分世界。
Outcome<PersonalReceipt>
apply_personal(CommonStore &common, PersonalState &state,
               const PersonalCatalog &c, InteractionState &interaction,
               const InteractionCatalog &ic, const InteractionAccess &ia,
               const PersonalAccess &access, const StableId &actor,
               const PersonalQuote &q) {
  auto lifecycle_binding =
      validate_personal_battle_lifecycle_write_binding(common, state, access);
  if (!lifecycle_binding.ok)
    return {false, {}, lifecycle_binding.error};
  auto attribute_binding = initial_attribute_binding(common, state, c, access);
  if (!attribute_binding.ok)
    return {false, {}, attribute_binding.error};
  if (access.selected_injury_sources == 1) {
    auto original = validate_personal_initial_attributes_sources_v1(
        common, state, c, *access.initial_attributes);
    if (!original.ok)
      return {false, {}, original.error};
  }
  if (q.kind == "duel_start" || q.kind == "encounter_fight") {
    auto controller = common.state().actors.find(q.controller_id);
    if (!controller)
      return fail<PersonalReceipt>("ActorUnavailable");
    if (q.source.kind == "role" && controller->injury >= 3)
      return fail<PersonalReceipt>("SevereInjuryCannotSpar");
    auto objective = c.objectives.find(q.objective_id);
    auto arena = c.arenas.find(q.battlefield_id);
    if (!objective || !arena)
      return fail<PersonalReceipt>("UnknownBattleDefinition");
    auto ready = ready_snapshots(common, state, c, access, *objective, q.sides);
    if (!ready.ok)
      return {false, {}, ready.error};
    if (q.fee > 0) {
      auto spendable = common.spendable(controller->wallet_id);
      if (!spendable.ok)
        return {false, {}, spendable.error};
      if (spendable.value < q.fee)
        return fail<PersonalReceipt>("InsufficientFunds");
      ResourceRecipe recipe;
      recipe.operations.push_back(
          WalletDelta{controller->wallet_id, -q.fee, true});
      recipe.operations.push_back(WalletDelta{q.wallet_id, q.fee, false});
      auto paid = common.apply_resources(recipe);
      if (!paid.ok)
        return {false, {}, paid.error};
    }
    auto id = common.next_id("battle");
    if (!id.ok)
      return {false, {}, id.error};
    PersonalBattle b;
    b.id = id.value;
    b.controller_id = q.controller_id;
    b.battlefield_id = q.battlefield_id;
    b.objective_id = q.objective_id;
    b.source = q.source;
    b.created_tick = common.tick();
    auto people = ids(q.sides);
    people.erase(std::remove(people.begin(), people.end(), q.controller_id),
                 people.end());
    people.insert(people.begin(), q.controller_id);
    std::set<std::size_t> used;
    for (auto &person : people) {
      auto &a = *common.state().actors.find(person);
      auto side = *q.sides.find(person);
      const PersonalSpawn *spawn = nullptr;
      for (std::size_t index = 0; index < arena->spawns.size(); ++index)
        if (!used.count(index) && arena->spawns[index].side == side) {
          spawn = &arena->spawns[index];
          used.insert(index);
          break;
        }
      if (!spawn)
        return fail<PersonalReceipt>("BattlefieldCapacity");
      PersonalUnit u;
      u.actor_id = person;
      u.side = side;
      u.position = spawn->position;
      u.heading = spawn->heading;
      u.hp = a.hp;
      u.initial_hp = a.hp;
      u.stamina = objective->initial_stamina.value_or(50);
      u.weapon_id = equipped(common, c, person);
      Integer efficiencies[] = {10000, 9000, 7500, 6000};
      Integer fatigue = a.fatigue >= 90   ? 6000
                        : a.fatigue >= 70 ? 7500
                        : a.fatigue >= 40 ? 9000
                                          : 10000;
      if (access.selected_injury_sources == 1) {
        auto actual = access.initial_attributes->current(person);
        if (!actual.ok)
          return {false, {}, actual.error};
        if (actual.value.hp != a.hp || actual.value.fatigue != a.fatigue ||
            actual.value.injury != a.injury)
          return fail<PersonalReceipt>("PersonalInitialCurrentBodyMismatch");
        u.martial = personal_initial_martial_v1(actual.value);
      } else {
        u.martial = std::max<Integer>(
            1, a.attributes.martial *
                   std::min(fatigue,
                            efficiencies[std::clamp<Integer>(a.injury, 0, 3)]) /
                   10000);
      }
      u.learned = personal_learned_snapshot(a, c);
      u.heart_id =
          u.learned.find(a.combat.heart_id) ? a.combat.heart_id : StableId{};
      u.protected_actor = person == q.protected_id;
      if (objective->hit_model == "skill_v1")
        u.skill_levels =
            PersonalHitLevels{personal_skill_level(a.skills.martial),
                              personal_skill_level(a.skills.archery),
                              {}};
      auto selected = armor(common, c, person);
      if (!selected.ok)
        return {false, {}, selected.error};
      if (!selected.value.first.empty()) {
        u.armor_id = selected.value.first;
        u.armor_defense = selected.value.second;
      }
      b.units.insert(person, u);
      auto closed = erase_actor_interaction_with_source_in_candidate(
          common, interaction, ia, person,
          ActorSocialOccupancyCause::personal_battle_started, actor);
      if (!closed.ok)
        return {false, {}, closed.error};
    }
    auto ai = personal_make_ai(b, c);
    if (!ai.ok)
      return {false, {}, ai.error};
    b.ai_plans = std::move(ai.value);
    auto lifecycle = prepare_personal_battle_lifecycle_start_in_candidate(
        common, state, access);
    if (!lifecycle.ok)
      return {false, {}, lifecycle.error};
    b.lifecycle = std::move(lifecycle.value);
    state.battles.insert(b.id, b);
    auto lifecycle_started =
        publish_personal_battle_lifecycle_start_in_candidate(common, state,
                                                             access, b.id);
    if (!lifecycle_started.ok)
      return {false, {}, lifecycle_started.error};
    if (access.selected_injury_sources == 1) {
      auto original = validate_personal_initial_attributes_sources_v1(
          common, state, c, *access.initial_attributes);
      if (!original.ok)
        return {false, {}, original.error};
    }
    if (q.kind == "encounter_fight" && b.source.kind == "encounter" &&
        access.on_battle_started) {
      auto recorded = access.on_battle_started(*state.battles.find(b.id));
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    return {true, PersonalReceipt{b.id, {}, {}}, {}};
  }
  auto b = state.battles.find(q.battle_id);
  if (!b)
    return fail<PersonalReceipt>("UnknownBattle");
  if (b->controller_id != actor)
    return fail<PersonalReceipt>("NotBattleController");
  if (q.kind == "battle_close") {
    auto closed =
        close_personal_battle_lifecycle_in_candidate(common, state, access, *b);
    if (!closed.ok)
      return {false, {}, closed.error};
    return {true, PersonalReceipt{b->id, {}, b->outcome}, {}};
  }
  if (q.kind != "battle_plan" || !q.plan)
    return fail<PersonalReceipt>("UnsupportedAction");
  auto resolved = personal_resolve(*b, c, *q.plan, common);
  if (!resolved.ok)
    return {false, {}, resolved.error};
  auto lifecycle_preserved = validate_personal_battle_resolved_lifecycle(
      *b, resolved.value, access.selected_battle_lifecycle);
  if (!lifecycle_preserved.ok)
    return {false, {}, lifecycle_preserved.error};
  *b = std::move(resolved.value);
  if (b->phase == "result") {
    if (b->applied || common.state().facts.find(b->id))
      return fail<PersonalReceipt>("BattleResultAlreadyApplied");
    auto &objective = *c.objectives.find(b->objective_id);
    const bool exact_body = access.selected_injury_sources == 1;
    if ((access.selected_injury_sources != 0 && !exact_body) ||
        (exact_body &&
         (access.selected_battle_lifecycle != 1 || !access.terminal_condition ||
          !access.terminal_condition->bound_to(common, state, c))) ||
        (!exact_body && access.terminal_condition))
      return fail<PersonalReceipt>(
          "PersonalTerminalConditionSelectionMismatch");
    std::vector<StableId> defeated;
    for (auto &row : b->units.rows()) {
      auto &u = row.second;
      auto a = common.state().actors.find(row.first);
      if (!a)
        return fail<PersonalReceipt>("UnknownCombatant");
      ActorMutation change;
      change.actor_id = row.first;
      change.hp = std::max<Integer>(1, u.hp);
      change.fatigue = std::min<Integer>(100, a->fatigue + 6);
      if (u.hp == 0) {
        defeated.push_back(row.first);
        change.injury =
            std::min<Integer>(3, a->injury + objective.injury_on_defeat);
      }
      if (!exact_body) {
        auto applied = common.apply_actor(change);
        if (!applied.ok)
          return {false, {}, applied.error};
      }
    }
    auto result = common.next_id("result");
    if (!result.ok)
      return {false, {}, result.error};
    b->result_id = result.value;
    b->applied = true;
    b->completed_tick = common.tick();
    PersonalFact fact;
    fact.battle_id = b->id;
    fact.result_id = b->result_id;
    fact.tick = common.tick();
    fact.outcome = b->outcome;
    fact.participants = ids(b->units);
    std::sort(defeated.begin(), defeated.end());
    fact.defeated = defeated;
    fact.source = b->source;
    std::vector<StableId> withdrawn;
    for (auto &id : fact.participants)
      if (b->units.find(id)->withdrawn_round)
        withdrawn.push_back(id);
    if (!withdrawn.empty())
      fact.withdrawn = withdrawn;
    state.facts.insert(b->id, fact);
    auto result_moment = record_personal_battle_result_moment_in_candidate(
        common, state, access, *b);
    if (!result_moment.ok)
      return {false, {}, result_moment.error};
    auto registered =
        common.append_fact({b->id, "personal_battle", "personal"});
    if (!registered.ok)
      return {false, {}, registered.error};
    // The source1 writer consumes this actual terminal receipt/header.
    // Legacy body mutation above is skipped, never written twice.
    if (exact_body)
      for (const auto &row : b->units.rows()) {
        auto body =
            access.terminal_condition->apply_actual_terminal(b->id, row.first);
        if (!body.ok)
          return {false, {}, body.error};
      }
    if (b->source.kind == "encounter" && access.on_battle_result) {
      auto recorded = access.on_battle_result(*b);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    if (access.record_escort) {
      auto recorded = access.record_escort(*b);
      if (!recorded.ok)
        return {false, {}, recorded.error};
    }
    if (b->source.kind == "encounter") {
      InteractionQuote leave;
      leave.encounter_id = b->source.encounter_id;
      auto left =
          apply_interaction(common, interaction, ic, ia, b->controller_id,
                            EncounterLeave{b->source.encounter_id}, leave);
      if (!left.ok)
        return {false, {}, left.error};
    }
  }
  return {true, PersonalReceipt{b->id, b->round, b->outcome}, {}};
}
// 导入与提交后验证冻结战场、当前来源以及终局事实，不通过重打修补坏记录。
Status validate_personal(const CommonStore &common, const PersonalState &state,
                         const PersonalCatalog &c,
                         const InteractionState &interaction) {
  auto outcome_policy = validate_personal_simultaneous_outcomes(c);
  if (!outcome_policy.ok)
    return outcome_policy;
  std::set<StableId> occupied;
  for (auto &entry : state.battles.rows()) {
    auto &b = entry.second;
    if (b.id != entry.first || b.controller_id.empty())
      return common_failure("InvalidBattleSession");
    if (b.phase != "planning" && b.phase != "result" && b.phase != "closed")
      return common_failure("InvalidBattlePhase");
    auto arena = c.arenas.find(b.battlefield_id);
    auto objective = c.objectives.find(b.objective_id);
    if (!arena || !objective || !b.units.find(b.controller_id))
      return common_failure("InvalidBattleDefinition");
    if (b.round < 1 || b.round > 101 || b.created_tick < 0 ||
        b.created_tick > common.tick())
      return common_failure("InvalidBattleTime");
    if (b.source.kind == "role") {
      if (!b.source.context ||
          !common.state().roles.find(b.source.context->role_id))
        return common_failure("InvalidBattleSource");
    } else if (b.source.kind == "encounter") {
      if (b.phase == "planning") {
        auto e = std::find_if(interaction.encounters.begin(),
                              interaction.encounters.end(), [&](auto &e) {
                                return e.id == b.source.encounter_id &&
                                       e.status == "pending";
                              });
        if (e == interaction.encounters.end())
          return common_failure("BattleEncounterMissing");
      }
    } else
      return common_failure("InvalidBattleSource");
    bool protected_found = false;
    for (auto &row : b.units.rows()) {
      auto &u = row.second;
      auto a = common.state().actors.find(row.first);
      if (!a || u.actor_id != row.first || u.side < 0 || u.side > 1)
        return common_failure("InvalidBattleActor");
      if (u.radius != 18 || !personal_point_valid(*arena, u.position, 18))
        return common_failure("InvalidBattlePosition");
      if (u.hp < 0 || u.hp > 100 || u.initial_hp < 1 || u.initial_hp > 100 ||
          u.stamina < 0 || u.stamina > 100 || u.heading < 0 || u.heading > 359)
        return common_failure("InvalidBattleVitals");
      if (u.recovery_until < 0 || u.recovery_until > 105)
        return common_failure("InvalidBattleUnit");
      if (u.martial < 1 || u.martial > 110 ||
          (!u.weapon_id.empty() && !c.weapons.find(u.weapon_id)))
        return common_failure("InvalidBattleEquipment");
      for (auto &learned : u.learned.rows())
        if (!c.skills.find(learned.first) || learned.second < 0 ||
            learned.second > 100)
          return common_failure("InvalidBattleSkill");
      if (!u.heart_id.empty() &&
          (!u.learned.find(u.heart_id) || !c.skills.find(u.heart_id) ||
           c.skills.find(u.heart_id)->kind != "heart"))
        return common_failure("InvalidBattleHeart");
      for (auto &status : u.statuses.rows())
        if ((status.first != "slow" && status.first != "exposed") ||
            status.second < 1 || status.second > 105)
          return common_failure("InvalidBattleStatus");
      auto withdrawn = withdrawal_valid(b, c, row.first);
      if (!withdrawn.ok)
        return withdrawn;
      auto armor_snapshot = armor_valid(u, c);
      if (!armor_snapshot.ok)
        return armor_snapshot;
      if (b.phase == "planning") {
        auto current = armor(common, c, row.first);
        if (!current.ok)
          return {false, {}, current.error};
        if (current.value.first != u.armor_id.value_or("") ||
            current.value.second != u.armor_defense.value_or(0))
          return common_failure("ArmorSourceMismatch");
      }
      if (objective->hit_model == "skill_v1") {
        if (!u.skill_levels || u.skill_levels->martial < 0 ||
            u.skill_levels->martial > 4 || u.skill_levels->archery < 0 ||
            u.skill_levels->archery > 4)
          return common_failure("InvalidPersonalHitSnapshot");
        if (b.phase == "planning" &&
            (u.skill_levels->martial !=
                 personal_skill_level(a->skills.martial) ||
             u.skill_levels->archery !=
                 personal_skill_level(a->skills.archery)))
          return common_failure("PersonalHitSourceMismatch");
      } else if (u.skill_levels)
        return common_failure("UnexpectedPersonalHitSnapshot");
      if (b.phase == "planning")
        for (auto &learned : u.learned.rows()) {
          auto s = c.skills.find(learned.first);
          if (s && s->required_skill_level &&
              (!a->combat.learned.find(learned.first) ||
               *a->combat.learned.find(learned.first) != learned.second ||
               !personal_skill_eligible(*a, *s).ok))
            return common_failure("InvalidQualifiedBattleSkill", learned.first);
        }
      if (b.phase != "closed" && !occupied.insert(row.first).second)
        return common_failure("DoubleBattleOccupation");
      if (b.phase == "planning" && a->hp != u.initial_hp)
        return common_failure("BattleWorldVitalsChanged");
      if (objective->escort_model == "exit_v1" && u.protected_actor) {
        if (row.first == b.controller_id ||
            u.side != b.units.find(b.controller_id)->side)
          return common_failure("InvalidEscortProtectedSide");
        protected_found = true;
      }
    }
    if (objective->escort_model == "exit_v1" && !protected_found)
      return common_failure("ProtectedPersonMissing");
    if (b.phase == "planning") {
      if (b.applied || !b.result_id.empty() || !b.outcome.empty() ||
          b.completed_tick != -1)
        return common_failure("PrematureBattleResult");
      auto ai = personal_make_ai(b, c);
      if (!ai.ok)
        return {false, {}, ai.error};
      if (ai.value.size() != b.ai_plans.size())
        return common_failure("InvalidCommittedAIPlan");
      for (auto &row : ai.value.rows())
        if (!b.ai_plans.find(row.first) ||
            !equal_plan(row.second, *b.ai_plans.find(row.first)))
          return common_failure("InvalidCommittedAIPlan");
    } else {
      if (b.outcome != "victory" && b.outcome != "defeat" &&
          b.outcome != "draw" && b.outcome != "withdrawn" &&
          b.outcome != "protected_lost")
        return common_failure("InvalidBattleOutcome");
      auto f = state.facts.find(b.id);
      if (!b.applied || b.result_id.empty() || !f ||
          !common.state().facts.find(b.id) ||
          b.completed_tick < b.created_tick || b.completed_tick > common.tick())
        return common_failure("MissingBattleResult");
      auto people = ids(b.units);
      std::vector<StableId> defeated, withdrawn;
      for (auto &id : people) {
        if (b.units.find(id)->hp == 0)
          defeated.push_back(id);
        if (b.units.find(id)->withdrawn_round)
          withdrawn.push_back(id);
      }
      if (f->battle_id != b.id || f->result_id != b.result_id ||
          f->outcome != b.outcome || !equal_source(f->source, b.source) ||
          f->tick != b.completed_tick || f->participants != people ||
          f->defeated != defeated)
        return common_failure("BattleResultMismatch");
      if (f->withdrawn.value_or(std::vector<StableId>{}) != withdrawn ||
          (withdrawn.empty() && f->withdrawn))
        return common_failure("BattleWithdrawalMismatch");
      if (b.outcome == "withdrawn" && objective->escort_model != "exit_v1") {
        if (!objective->allow_withdrawal)
          return common_failure("PersonalOutcomeMismatch");
        auto before = b;
        for (auto &row : before.units.rows())
          if (row.second.withdrawn_round == b.round)
            row.second.withdrawn_round.reset();
        if (!personal_outcome(before, c).empty())
          return common_failure("PersonalOutcomeMismatch");
        auto &controller = *b.units.find(b.controller_id);
        for (auto &row : b.units.rows()) {
          auto &u = row.second;
          if (row.first != b.controller_id &&
              !(u.side == controller.side && u.protected_actor))
            continue;
          if (u.hp <= 0 || !personal_at_exit(*arena, u.side, u.position) ||
              (u.withdrawn_round && u.withdrawn_round != b.round) ||
              (controller.withdrawn_round && u.withdrawn_round != b.round))
            return common_failure("PersonalOutcomeMismatch");
          for (auto &e : b.last_events)
            if (e.actor_id == row.first && e.action == "withdraw" &&
                (e.code == "ExitNotReached" || e.code == "WithdrawalMeleeHit" ||
                 e.code == "ProtectedExitNotReached"))
              return common_failure("PersonalOutcomeMismatch");
        }
      } else {
        auto expected = personal_outcome(b, c);
        if (expected.empty() && b.round >= objective->max_rounds)
          expected = personal_limit_outcome(b, c);
        if (expected.empty() || b.outcome != expected ||
            (objective->escort_model != "exit_v1" &&
             b.units.find(b.controller_id)->withdrawn_round))
          return common_failure(objective->escort_model == "exit_v1"
                                    ? "EscortOutcomeMismatch"
                                    : "PersonalOutcomeMismatch");
        if (objective->escort_model == "exit_v1" &&
            (b.outcome == "victory" ||
             (b.outcome == "draw" && b.round >= objective->max_rounds))) {
          Integer count = 0;
          for (auto &e : b.last_events)
            if (e.action == "objective" &&
                e.code == (b.outcome == "victory" ? "EscortExitReached"
                                                  : "EscortExitTimeout") &&
                !e.actor_id && !e.target_id && !e.skill_id && !e.damage &&
                !e.status && !e.expires_round && !e.amount && !e.hit_bp &&
                !e.roll && !e.position)
              ++count;
          if (count != 1)
            return common_failure("MissingEscortOutcomeReason");
        }
      }
    }
    auto geometry = personal_resolve_moves(*arena, b.units, {});
    if (!geometry.ok)
      return common_failure("BattleBodyOverlap");
  }
  return common_success();
}
} // namespace hanmo::resident
