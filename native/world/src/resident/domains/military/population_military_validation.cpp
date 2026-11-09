#include "resident/domains/organization/authority_basis.hpp"
// 从永久登记及真实战损重建人口连续性；冷准入不得相信当前人数或裸“已校验”标记。
#include "field_battle/field_world.hpp"
#include "military_exact_sources.hpp"
#include "military_integration_assets.hpp"
#include "naval_casualties.hpp"
#include "population_military_attrition.hpp"
#include "population_military_internal.hpp"
#include "population_military_recovery.hpp"
#include "population_occupation.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "hanmo/resident/command_contract.hpp"
#include <charconv>
#include <tuple>
namespace hanmo::resident {

// 接收后的原家户退籍借现接收军的实际军职；宏观office仍由真实当前grant核准。
Outcome<PopulationReleaseAuthority>
population_military_release_authority(const PopulationMilitaryReadContext &c,
                                      const PopulationMilitarySource &source,
                                      Integer tick,
                                      const StableId &before_fact_id) {
  using namespace population_military_detail;
  PopulationReleaseAuthority authority{source.organization_id,
                                       source.military_office_id,
                                       source.organization_office_id,
                                       {}};
  std::set<StableId> earlier;
  if (!before_fact_id.empty()) {
    for (const auto &id : c.common.state().facts.order()) {
      if (id == before_fact_id)
        break;
      earlier.insert(id);
    }
  }
  for (const auto &row : c.state.integration_facts.rows()) {
    const auto &fact = row.second;
    if (fact.source_id != source.id || fact.tick > tick ||
        (!before_fact_id.empty() && !earlier.count(fact.id)))
      continue;
    const auto *head = c.common.state().facts.find(fact.id);
    const auto *transfer =
        find(c.military.integration_facts, fact.military_receipt_id);
    const auto *army =
        transfer ? find(c.military.armies, transfer->recipient_army_id)
                 : nullptr;
    const auto *campaign =
        army ? find(c.military.campaigns, army->order_id) : nullptr;
    const auto *office =
        campaign ? find(c.military_catalog.offices, campaign->office_id)
                 : nullptr;
    const auto *policy = c.catalog.integration_policies.find(fact.policy_id);
    if (!authority.integration_receipt_id.empty() || !head ||
        head->domain != "population_military" ||
        head->kind != "population_military_integration" || !transfer ||
        !has(transfer->population_receipt_ids, fact.id) ||
        fact.unit_id != source.unit_id || !army || !campaign || !office ||
        !policy ||
        office->organization_id != transfer->recipient_organization_id ||
        policy->receiving_organization_id !=
            transfer->recipient_organization_id)
      return {
          false,
          {},
          common_failure("PopulationReleaseIntegrationSource", fact.id).error};
    authority.organization_id = transfer->recipient_organization_id;
    authority.military_office_id = office->id;
    authority.required_organization_office_id.clear();
    authority.integration_receipt_id = fact.id;
  }
  return {true, std::move(authority), {}};
}
namespace population_military_detail {
// 作者军籍source必须显式空槽，且原籍/在役/受伤/被俘/离队分别保留同户身份。
Status source_valid(const PopulationMilitaryReadContext &c,
                    const PopulationMilitarySource &s) {
  const auto *camp = find(c.military_catalog.garrisons, s.garrison_id);
  const auto *unit = camp ? find(camp->units, s.unit_id) : nullptr;
  const auto *office = find(c.military_catalog.offices, s.military_office_id);
  const auto *macro =
      c.organization_catalog.offices.find(s.organization_office_id);
  const auto *home = c.economy.households.find(s.household_group_id);
  const auto *consent = c.catalog.consents.find(s.service_consent_source_id);
  if (s.id.empty() || !camp || !unit ||
      unit->population_source_id != std::optional<StableId>(s.id) ||
      unit->initial_soldiers != 0 ||
      unit->initial_training != std::optional<Integer>(s.initial_training) ||
      unit->initial_fatigue != std::optional<Integer>(s.initial_fatigue) ||
      camp->organization_id != s.organization_id ||
      camp->place_id != s.home_site_id || !office ||
      office->organization_id != s.organization_id ||
      office->garrison_id != s.garrison_id || !macro ||
      macro->organization_id != s.organization_id || !home || !consent ||
      consent->cohort_id != s.reserve_cohort_id ||
      consent->organization_id != s.organization_id ||
      consent->garrison_id != s.garrison_id || consent->from_tick < 0 ||
      consent->through_tick < consent->from_tick ||
      consent->max_active_count <= 0 || s.capacity <= 0 ||
      s.initial_training < 0 || s.initial_training > 100 ||
      s.initial_fatigue < 0 || s.initial_fatigue > 100 ||
      s.initial_morale <= 0 || s.initial_morale > 100)
    return common_failure("InvalidPopulationMilitarySource", s.id);
  std::set<StableId> ids;
  for (const auto &row :
       {std::pair<StableId, PopulationStatus>{
            s.reserve_cohort_id, PopulationStatus::trained_reserve},
        {s.serving_cohort_id, PopulationStatus::serving},
        {s.wounded_cohort_id, PopulationStatus::wounded},
        {s.captive_cohort_id, PopulationStatus::captive},
        {s.displaced_cohort_id, PopulationStatus::displaced}}) {
    const auto *cohort = c.economy.cohorts.find(row.first);
    if (!cohort || !ids.insert(row.first).second ||
        cohort->site_id != s.home_site_id ||
        cohort->household_group_id != s.household_group_id ||
        cohort->private_wallet_id != home->wallet_id ||
        cohort->private_inventory_id != home->inventory_id ||
        (cohort->status != row.second &&
         !(row.first == s.reserve_cohort_id &&
           cohort->status == PopulationStatus::training)) ||
        (cohort->affiliation_id &&
         *cohort->affiliation_id != s.organization_id))
      return common_failure("PopulationMilitaryCohortSourceMismatch",
                            row.first);
    if (row.first != s.reserve_cohort_id) {
      auto *seed = c.economy_catalog.cohorts.find(row.first);
      if (!seed || seed->headcount != 0 || seed->labor_capacity != 0)
        return common_failure("PopulationMilitaryInitialRosterNotEmpty",
                              row.first);
    }
  }
  if (s.wage_obligation_ids.empty() || !unique(s.wage_obligation_ids))
    return common_failure("PopulationMilitaryWagePolicyRequired", s.id);
  for (const auto &id : s.wage_obligation_ids) {
    const auto *wage = c.economy.obligations.find(id);
    const auto *wallet =
        wage ? c.common.wallet(wage->source_wallet_or_inventory) : nullptr;
    const auto *rate =
        wage ? c.economy_catalog.rates.find(wage->rate_profile) : nullptr;
    if (!wage || wage->kind != ObligationKind::wage ||
        wage->principal.kind != SubjectKind::organization ||
        wage->principal.id != s.organization_id ||
        wage->beneficiary.kind != SubjectKind::cohort ||
        !ids.count(wage->beneficiary.id) ||
        wage->destination != home->wallet_id || !wallet ||
        wallet->owner.kind != Owner::Kind::organization ||
        wallet->owner.id != s.organization_id || !rate || !rate->per_capita ||
        rate->numerator < 0 || rate->denominator <= 0)
      return common_failure("PopulationMilitaryWageSourceMismatch", id);
  }
  return common_success();
}
// 任职来源按最初任职与真实更替序号还原；不要求旧执行者今天仍有权。
Status historical_authority(const PopulationMilitaryReadContext &c,
                            const PopulationMilitaryActionFact &f) {
  const auto &t = f.terms;
  const auto *source = c.catalog.sources.find(t.source_id);
  auto service =
      source ? population_military_release_authority(c, *source, f.started_tick,
                                                     f.id)
             : Outcome<PopulationReleaseAuthority>{
                   false,
                   {},
                   common_failure("UnknownPopulationMilitarySource").error};
  const auto *initial =
      c.organization_catalog.offices.find(t.authority.office_id);
  const auto *appointment =
      find(c.military.action_facts, t.military_appointment_id);
  const auto *military_terms =
      appointment ? std::get_if<OfficeTerms>(&appointment->terms) : nullptr;
  if (!source || !service.ok ||
      (!service.value.integration_receipt_id.empty() &&
       f.kind != "population_release") ||
      !initial || initial->organization_id != service.value.organization_id ||
      t.organization_id != service.value.organization_id ||
      t.authority.organization_id != service.value.organization_id ||
      (!service.value.required_organization_office_id.empty() &&
       t.authority.office_id !=
           service.value.required_organization_office_id) ||
      t.authority.actor_id != f.actor_id || t.actor_id != f.actor_id ||
      t.military_office_id != service.value.military_office_id ||
      t.unit_id != source->unit_id || t.garrison_id != source->garrison_id ||
      t.place_id != source->home_site_id ||
      t.household_group_id != source->household_group_id ||
      t.reserve_cohort_id != source->reserve_cohort_id ||
      t.serving_cohort_id != source->serving_cohort_id || !appointment ||
      appointment->phase != "completed" || !appointment->success ||
      appointment->command != "office_accept" ||
      appointment->actor_id != f.actor_id ||
      appointment->completed_tick > f.started_tick || !military_terms ||
      military_terms->office_id != service.value.military_office_id ||
      military_terms->organization_id != service.value.organization_id)
    return common_failure("PopulationMilitaryHistoricalAuthorityMissing", f.id);
  if (!service.value.integration_receipt_id.empty()) {
    const auto *transferred =
        c.state.integration_facts.find(service.value.integration_receipt_id);
    std::set<StableId> earlier;
    for (const auto &id : c.common.state().facts.order()) {
      if (id == f.id)
        break;
      earlier.insert(id);
    }
    Integer home_returns = 0;
    for (const auto &receipt : c.military.return_facts)
      if (receipt.army_id == transferred->to_army_id &&
          receipt.garrison_id == source->garrison_id &&
          receipt.tick >= transferred->tick && receipt.tick <= f.started_tick &&
          earlier.count(receipt.id) && has(receipt.unit_ids, source->unit_id)) {
        auto authentic =
            validate_population_military_return_source(c, receipt.id);
        if (!authentic.ok)
          return authentic;
        ++home_returns;
      }
    if (home_returns != 1)
      return common_failure("PopulationIntegrationHistoricalHomeReturnRequired",
                            f.id);
  }
  Integer epoch = initial->epoch;
  auto holder = initial->holder_actor_id;
  bool military_current = false;
  for (const auto &id : c.common.state().facts.order()) {
    if (id == f.id)
      break;
    if (const auto *changed = c.organization.facts.find(id);
        changed && changed->kind == "organization_office_changed" &&
        changed->office_id == initial->id) {
      if (changed->tick > f.started_tick)
        return common_failure("PopulationMilitaryHistoricalAuthorityMissing",
                              f.id);
      ++epoch;
      holder = changed->actor_id.empty()
                   ? std::nullopt
                   : std::optional<StableId>(changed->actor_id);
    }
    if (const auto *accepted = find(c.military.action_facts, id);
        accepted && accepted->command == "office_accept" &&
        accepted->phase == "completed" && accepted->success) {
      const auto *terms = std::get_if<OfficeTerms>(&accepted->terms);
      if (terms && terms->office_id == t.military_office_id)
        military_current = accepted->id == t.military_appointment_id;
    }
    if (const auto *resigned = find(c.military.resignation_facts, id);
        resigned && resigned->office_id == t.military_office_id)
      military_current = false;
  }
  if (!military_current)
    return common_failure("PopulationMilitaryHistoricalOfficeMissing", f.id);
  if (epoch != t.authority.authority_epoch ||
      holder != std::optional<StableId>(f.actor_id))
    return common_failure("PopulationMilitaryHistoricalAuthorityMissing", f.id);
  PopulationAuthorityClock authority_clock(c);
  if (t.authority.observed_at.half_day_tick != f.started_tick)
    return common_failure("PopulationMilitaryHistoricalAuthorityMomentMismatch",
                          f.id);
  auto original = read_historical_authority_proof(
      c.common, c.organization, c.organization_catalog, t.authority,
      {"population_military", "population_military_action", f.id},
      authority_clock);
  if (!original.ok)
    return {false, {}, original.error};
  const auto &grant = original.value;
  const bool granted =
      grant.office_id == initial->id && t.count <= grant.amount_limit &&
      f.started_tick >= grant.valid_from &&
      (!grant.valid_until ||
       (f.started_tick <= *grant.valid_until &&
        (!f.success || f.completed_tick <= *grant.valid_until)));
  if (!granted || t.authority.scope.kind != ScopeRef::Kind::site ||
      t.authority.scope.id != source->home_site_id ||
      t.authority.amount != t.count ||
      t.authority.command_kind != (f.kind == "population_enroll"
                                       ? "mobilize_reserve"
                                       : "release_reserve"))
    return common_failure("PopulationMilitaryHistoricalGrantMissing", f.id);
  return common_success();
}
} // namespace population_military_detail
namespace {
using namespace population_military_detail;
using S = PopulationSoldierStatus;
// typed检查点直接校验共享ID高水位，不依赖JSON重导出的最大ID扫描。
bool allocated_id(const CommonStore &common, const StableId &id,
                  const StableId &prefix) {
  if (id.rfind(prefix, 0) != 0)
    return false;
  const auto number = id.substr(prefix.size());
  Integer value = 0;
  const auto parsed =
      std::from_chars(number.data(), number.data() + number.size(), value);
  return parsed.ec == std::errc() &&
         parsed.ptr == number.data() + number.size() && value > 0 &&
         value < common.state().next_instance_id &&
         std::to_string(value) == number;
}
// 伤亡收据内部分类必须互斥完整；本域不接受只带一个伤亡数量的伪事实。
Outcome<S> receipt_status(const MilitaryRosterReceipt &r, const StableId &id) {
  MilitaryUnitRecord u;
  u.member_ids = r.member_ids;
  u.fit = r.fit;
  u.wounded = r.wounded;
  u.dead = r.dead;
  u.captured = r.captured;
  u.deserted = r.deserted;
  return actual_status(u, id);
}
// 军粮责任按历史事件重建；同tick供养在登记、战损、归营和占领之前。
struct MilitaryResponsibility {
  S status = S::released;
  StableId army_id, garrison_id, keeper_army_id;
};
// 归营移动须反查实际成功卸装动作，不能只把army当前标为closed。
Status return_source(const PopulationMilitaryReadContext &c,
                     const MilitaryReturnFact &receipt) {
  const auto *head = c.common.state().facts.find(receipt.id);
  const auto *army = find(c.military.armies, receipt.army_id);
  const MilitaryLogisticsFact *action = nullptr;
  for (const auto &row : c.military.logistics_facts)
    if (row.result && row.result->receipt_id == receipt.id) {
      if (action)
        return common_failure("PopulationMilitaryReturnSourceMismatch",
                              receipt.id);
      action = &row;
    }
  const auto *action_header =
      action ? c.common.state().facts.find(action->id) : nullptr;
  if (!head || head->kind != "military_return" || head->domain != "military" ||
      !army || !action || !action_header ||
      action_header->kind != "military_action" ||
      action_header->domain != "military" || action->command != "army_return" ||
      action->phase != "completed" || !action->success ||
      !action->result->ok.value_or(false) ||
      action->completed_tick != receipt.tick ||
      action->result->army_id != receipt.army_id ||
      action->result->order_id != receipt.order_id ||
      action->terms.army_id != receipt.army_id ||
      action->terms.garrison_id != receipt.garrison_id ||
      action->terms.source_garrison_id != receipt.source_garrison_id ||
      action->actor_id != receipt.actor_id || !action->terms.authority ||
      action->terms.authority->kind != receipt.authority.kind ||
      action->terms.authority->id != receipt.authority.id ||
      army->formation_source.empty() || army->order_id != receipt.order_id ||
      army->garrison_id != receipt.source_garrison_id ||
      !unique(receipt.prisoner_ids) ||
      !find(c.military_catalog.garrisons, receipt.garrison_id))
    return common_failure("PopulationMilitaryReturnSourceMismatch", receipt.id);
  return common_success();
}
// 接收羁押只能来自已经落账的原围城占领链，不接受单独篡改captors或名单。
Status custody_occupation_source(const PopulationMilitaryReadContext &c,
                                 const MilitaryOccupationFact &receipt) {
  const auto *header = c.common.state().facts.find(receipt.id);
  const auto *siege = find(c.military.sieges, receipt.siege_id);
  const auto *army = find(c.military.armies, receipt.army_id);
  const auto *hold =
      find(c.military_catalog.strongholds, receipt.stronghold_id);
  const auto *campaign = find(c.military.campaigns, receipt.campaign_id);
  const auto *order =
      campaign ? find(c.military_catalog.orders, campaign->template_id)
               : nullptr;
  if (!header || header->kind != "military_occupation" ||
      header->domain != "military" || receipt.phase != "completed" ||
      !receipt.assets_applied.value_or(false) || !receipt.asset_manifest ||
      !receipt.custody_transfers || !siege || !siege->result ||
      (siege->phase != "result" && siege->phase != "closed") ||
      siege->stronghold_id != receipt.stronghold_id ||
      siege->campaign_id != receipt.campaign_id ||
      siege->result->winner_side != 0 ||
      (siege->result->kind != "occupied" &&
       siege->result->kind != "surrendered") ||
      siege->result->tick > receipt.tick || !army ||
      army->organization_id != receipt.to_organization_id || !hold ||
      hold->garrison_id != receipt.garrison_id || !campaign ||
      campaign->army_id != army->id || army->order_id != campaign->id ||
      !order || order->target_stronghold_id != hold->id)
    return common_failure("PopulationMilitaryCustodySourceMissing", receipt.id);
  bool victorious = false;
  for (const auto &side : siege->armies)
    if (side.first == army->id && side.second == 0)
      victorious = true;
  if (!victorious || has(siege->withdrawn_army_ids, army->id))
    return common_failure("PopulationMilitaryCustodySourceMissing", receipt.id);
  return common_success();
}
// 原编组、返回和羁押事实按完成tick及全局源序重放，供养查询排除该tick之后的身份切换。
Outcome<MilitaryResponsibility>
responsibility(const PopulationMilitaryReadContext &c,
               const PopulationEnrollment &enrollment, const StableId &member,
               Integer tick, bool supply) {
  const auto *source = c.catalog.sources.find(enrollment.source_id);
  if (!source)
    return {
        false,
        {},
        common_failure("UnknownPopulationMilitarySource", enrollment.source_id)
            .error};
  struct Event {
    Integer tick;
    std::size_t order;
    int kind;
    StableId id;
  };
  std::vector<Event> events;
  for (std::size_t n = 0; n < c.common.state().facts.order().size(); ++n) {
    const auto &id = c.common.state().facts.order()[n];
    if (const auto *population = c.state.facts.find(id)) {
      for (const auto &move : population->movements)
        if (move.member_id == member)
          events.push_back({population->tick, n, 0, id});
    }
    if (const auto *formed = find(c.military.action_facts, id);
        formed && formed->command == "army_form" &&
        formed->phase == "completed" && formed->success) {
      const auto *terms = std::get_if<ArmyFormationTerms>(&formed->terms);
      if (terms && has(terms->unit_ids, enrollment.unit_id) &&
          formed->completed_tick >= enrollment.completed_tick)
        events.push_back({formed->completed_tick, n, 1, id});
    }
    if (const auto *returned = find(c.military.return_facts, id))
      events.push_back({returned->tick, n, 2, id});
    if (const auto *integrated = find(c.military.integration_facts, id))
      events.push_back({integrated->tick, n, 4, id});
    if (const auto *occupied = find(c.military.occupation_facts, id);
        occupied && occupied->custody_transfers)
      for (const auto &transfer : *occupied->custody_transfers)
        if (transfer.person_id == member)
          events.push_back({occupied->tick, n, 3, id});
    if (c.naval_combat)
      if (const auto *captured = find(c.naval_combat->battle.captures, id);
          captured && has(captured->original_prisoner_ids, member))
        events.push_back({captured->tick, n, 5, id});
  }
  std::stable_sort(
      events.begin(), events.end(), [](const auto &a, const auto &b) {
        return std::tie(a.tick, a.order) < std::tie(b.tick, b.order);
      });
  MilitaryResponsibility out;
  out.garrison_id = source->garrison_id;
  for (const auto &event : events) {
    if (event.tick > tick || (supply && event.tick == tick))
      continue;
    if (event.kind == 0) {
      const auto *fact = c.state.facts.find(event.id);
      for (const auto &move : fact->movements)
        if (move.member_id == member) {
          out.status = move.after;
          if (move.after == S::fit && move.before == S::released) {
            out.army_id.clear();
            out.garrison_id = source->garrison_id;
            out.keeper_army_id.clear();
          } else if (move.after == S::captured) {
            out.keeper_army_id = move.captor_army_id;
            out.army_id = move.captor_army_id;
            out.garrison_id.clear();
          } else if (move.after == S::released) {
            out.army_id.clear();
            out.garrison_id = source->garrison_id;
            out.keeper_army_id.clear();
          }
        }
    } else if (event.kind == 1 &&
               (out.status == S::fit || out.status == S::wounded)) {
      const auto *formed = find(c.military.action_facts, event.id);
      const auto *terms = std::get_if<ArmyFormationTerms>(&formed->terms);
      const auto *army = formed->result
                             ? find(c.military.armies, formed->result->army_id)
                             : nullptr;
      if (!army || army->formation_source != formed->id ||
          army->organization_id !=
              military_unit_organization_at(c.military, c.military_catalog,
                                            enrollment.unit_id, event.tick,
                                            formed->id) ||
          terms->organization_id != army->organization_id ||
          terms->garrison_id != out.garrison_id || !out.army_id.empty())
        return {false,
                {},
                common_failure("PopulationMilitaryFormationHistoryMismatch",
                               event.id)
                    .error};
      out.army_id = army->id;
      out.garrison_id.clear();
    } else if (event.kind == 2) {
      const auto *returned = find(c.military.return_facts, event.id);
      const bool own = (out.status == S::fit || out.status == S::wounded) &&
                       out.army_id == returned->army_id;
      const bool prisoner = out.status == S::captured &&
                            out.keeper_army_id == returned->army_id &&
                            out.army_id == returned->army_id;
      if (!own && !prisoner)
        continue;
      auto valid = return_source(c, *returned);
      if (!valid.ok)
        return {false, {}, valid.error};
      if ((own && !has(returned->unit_ids, enrollment.unit_id)) ||
          (prisoner && !has(returned->prisoner_ids, member)))
        return {false,
                {},
                common_failure("PopulationMilitaryReturnRosterMissing", member)
                    .error};
      out.army_id.clear();
      out.garrison_id = returned->garrison_id;
    } else if (event.kind == 4) {
      const auto *integrated = find(c.military.integration_facts, event.id);
      for (const auto &old : integrated->source_armies)
        if ((out.status == S::fit || out.status == S::wounded) &&
            out.army_id == old.id && has(old.unit_ids, enrollment.unit_id)) {
          out.army_id = integrated->recipient_army_id;
          out.garrison_id.clear();
        }
      for (const auto &custody : integrated->custody)
        if (custody.member_id == member) {
          if (out.status != S::captured ||
              out.keeper_army_id != custody.from_army_id ||
              out.army_id != custody.from_army_id)
            return {
                false,
                {},
                common_failure("PopulationIntegrationCustodyHistory", member)
                    .error};
          out.keeper_army_id = custody.to_army_id;
          out.army_id = custody.to_army_id;
          out.garrison_id.clear();
        }
    }
    // 船上旧俘虏只转移既有看管责任，不再产生人口损失或第二次捕获。
    else if (event.kind == 5) {
      const auto *capture =
          c.naval_combat ? find(c.naval_combat->battle.captures, event.id)
                         : nullptr;
      if (!capture)
        return {false,
                {},
                common_failure("NavalCaptureSourceRequired", event.id).error};
      auto valid =
          validate_naval_casualty_receipt(c.common, c.military, *c.naval_combat,
                                          capture->casualty_source_id, tick);
      if (!valid.ok)
        return {false, {}, valid.error};
      const auto *from = find(c.naval_combat->fleets, capture->from_fleet_id);
      const auto *to = find(c.naval_combat->fleets, capture->to_fleet_id);
      if (!from || !to || out.status != S::captured ||
          out.keeper_army_id != from->army_id || out.army_id != from->army_id ||
          !out.garrison_id.empty())
        return {false,
                {},
                common_failure("NavalPopulationCustodyHistoryMismatch", member)
                    .error};
      out.keeper_army_id = to->army_id;
      out.army_id = to->army_id;
      out.garrison_id.clear();
    } else if (event.kind == 3) {
      const auto *occupied = find(c.military.occupation_facts, event.id);
      auto valid = custody_occupation_source(c, *occupied);
      if (!valid.ok)
        return {false, {}, valid.error};
      Integer count = 0;
      for (const auto &transfer : *occupied->custody_transfers)
        if (transfer.person_id == member) {
          if (++count != 1 || out.status != S::captured ||
              out.keeper_army_id != transfer.from_army_id ||
              !out.army_id.empty() ||
              out.garrison_id != occupied->garrison_id ||
              transfer.to_army_id != occupied->army_id)
            return {false,
                    {},
                    common_failure("PopulationMilitaryCustodyHistoryMismatch",
                                   member)
                        .error};
          out.keeper_army_id = transfer.to_army_id;
          out.army_id = transfer.to_army_id;
          out.garrison_id.clear();
        }
    }
  }
  return {true, std::move(out), {}};
}

// 事实完整链的只读重建，同时形成用于人口owner的死亡及唯一军粮责任证明。
Outcome<PopulationContinuity>
continuity(const PopulationMilitaryReadContext &c) {
  PopulationContinuity result;
  auto boundary = boundary_tick(c);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  const Integer latest = boundary.value;
  if (!c.catalog.enabled) {
    if (!c.state.enrollments.empty() || !c.state.actions.empty() ||
        !c.state.action_facts.empty() || !c.state.facts.empty() ||
        !c.state.integration_facts.empty())
      return {false,
              {},
              common_failure("DisabledPopulationMilitaryHasState").error};
    return {true, result, {}};
  }
  std::set<StableId> units, cohorts, consents;
  for (const auto &row : c.catalog.sources.rows()) {
    auto checked = source_valid(c, row.second);
    if (!checked.ok)
      return {false, {}, checked.error};
    if (row.first != row.second.id ||
        !units.insert(row.second.unit_id).second ||
        !consents.insert(row.second.service_consent_source_id).second)
      return {
          false,
          {},
          common_failure("DuplicatePopulationMilitarySource", row.first).error};
    for (const auto &id :
         {row.second.reserve_cohort_id, row.second.serving_cohort_id,
          row.second.wounded_cohort_id, row.second.captive_cohort_id,
          row.second.displaced_cohort_id})
      if (!cohorts.insert(id).second)
        return {false,
                {},
                common_failure("SharedPopulationMilitaryCohort", id).error};
  }
  for (const auto &row : c.catalog.integration_policies.rows()) {
    const auto &policy = row.second;
    if (row.first != policy.id || !c.catalog.sources.find(policy.source_id) ||
        !consents.insert(policy.service_consent_source_id).second)
      return {false,
              {},
              common_failure("InvalidPopulationIntegrationPolicy", row.first)
                  .error};
  }
  for (const auto &row : c.catalog.consents.rows())
    if (row.first != row.second.id || !consents.count(row.first))
      return {
          false,
          {},
          common_failure("UnboundPopulationMilitaryConsent", row.first).error};
  auto integration_valid = validate_population_military_integrations(c);
  if (!integration_valid.ok)
    return {false, {}, integration_valid.error};
  std::map<StableId, const PopulationEnrollment *> owners;
  std::map<StableId, S> statuses;
  std::map<StableId, Integer> changed_tick;
  for (const auto &row : c.state.enrollments.rows()) {
    const auto &e = row.second;
    const auto *source = c.catalog.sources.find(e.source_id);
    const auto *action = c.state.action_facts.find(e.action_id);
    const auto *receipt = c.state.facts.find(e.id);
    if (row.first != e.id || !source || e.unit_id != source->unit_id ||
        !action || action->kind != "population_enroll" ||
        action->phase != "completed" || !action->success ||
        action->receipt_id != e.id || action->actor_id != e.actor_id ||
        action->completed_tick != e.completed_tick ||
        !same_terms(e.terms, action->terms) || !receipt ||
        receipt->kind != "population_enrolled" ||
        receipt->action_id != e.action_id || e.terms.count <= 0 ||
        e.terms.count != Integer(e.members.size()) || e.terms.labor_count < 0 ||
        e.terms.labor_count > e.terms.count || e.terms.enrollment_id != e.id ||
        e.completed_tick < 0 || e.completed_tick > latest)
      return {false,
              {},
              common_failure("InvalidPopulationEnrollment", row.first).error};
    std::vector<StableId> tokens;
    for (std::size_t i = 0; i < e.members.size(); ++i) {
      const auto &member = e.members[i];
      if (member.member_id !=
              "soldier:population:" + e.id + ":" + std::to_string(i + 1) ||
          member.source_cohort_id != source->reserve_cohort_id ||
          member.household_group_id != source->household_group_id ||
          member.labor_eligible != (Integer(i) < e.terms.labor_count) ||
          c.common.actor(member.member_id) ||
          !owners.emplace(member.member_id, &e).second)
        return {
            false,
            {},
            common_failure("InvalidPopulationSoldierOrigin", member.member_id)
                .error};
      tokens.push_back(member.member_id);
      result.origins.push_back(member);
      statuses[member.member_id] = S::released;
      changed_tick[member.member_id] = e.completed_tick;
    }
    if (tokens != e.terms.member_ids)
      return {false,
              {},
              common_failure("PopulationEnrollmentRosterMismatch", e.id).error};
  }
  std::vector<StableId> ordered_events;
  std::map<StableId, std::size_t> fact_positions;
  for (std::size_t i = 0; i < c.common.state().facts.order().size(); ++i) {
    const auto &id = c.common.state().facts.order()[i];
    fact_positions[id] = i;
    if (c.state.facts.find(id))
      ordered_events.push_back(id);
  }
  if (ordered_events != c.state.facts.order())
    return {
        false, {}, common_failure("PopulationMilitaryFactOrderMismatch").error};
  std::optional<WorldMoment> previous_event_at;
  std::set<StableId> casualty_ids, recovery_ids, attrition_ids, seen_enrollment;
  for (const auto &row : c.state.facts.rows()) {
    const auto &f = row.second;
    const auto *head = c.common.state().facts.find(f.id);
    if (row.first != f.id ||
        !allocated_id(c.common, f.id, "population_military:") || !head ||
        head->kind != f.kind || head->domain != "population_military" ||
        f.tick < 0 || f.tick > latest)
      return {false,
              {},
              common_failure("InvalidPopulationMilitaryFact", f.id).error};
    if (c.selected_version == 4) {
      auto actual = f.occurred_at
                        ? Outcome<WorldMoment>{true, *f.occurred_at, {}}
                        : normalize_world_moment(f.tick, {});
      if (!actual.ok)
        return {false, {}, actual.error};
      if (previous_event_at) {
        auto order = compare_world_moments(*previous_event_at, actual.value);
        if (!order.ok)
          return {false, {}, order.error};
        if (order.value > 0)
          return {
              false,
              {},
              common_failure("PopulationMilitaryExactFactOrderMismatch", f.id)
                  .error};
      }
      previous_event_at = actual.value;
    }
    const auto *action =
        f.action_id.empty() ? nullptr : c.state.action_facts.find(f.action_id);
    auto tail = validate_population_military_fact_source_tail(c, f);
    if (!tail.ok)
      return {false, {}, tail.error};
    const std::vector<MilitaryRosterReceipt> *casualty = nullptr;
    if (f.kind == "population_casualty") {
      auto actual = read_population_military_casualty(c, f);
      if (!actual.ok)
        return {false, {}, actual.error};
      casualty = actual.value.units;
      const auto *original = c.common.state().facts.find(f.military_receipt_id);
      if (!casualty || !original || original->kind != "military_casualties" ||
          original->domain != "military" ||
          !fact_positions.count(f.military_receipt_id) ||
          fact_positions[f.military_receipt_id] >= fact_positions[f.id] ||
          !casualty_ids.insert(f.military_receipt_id).second ||
          !f.action_id.empty() || !f.source_id.empty())
        return {false,
                {},
                common_failure("PopulationMilitaryCasualtySourceMissing", f.id)
                    .error};
    } else if (f.kind == "population_recovered") {
      auto authentic = validate_population_military_recovered_event(c, f);
      if (!authentic.ok)
        return {false, {}, authentic.error};
      if (!recovery_ids.insert(f.military_receipt_id).second)
        return {
            false,
            {},
            common_failure("PopulationRecoveryAlreadyConsumed", f.id).error};
    } else if (f.kind == "population_deserted") {
      auto authentic = validate_population_military_deserted_event(c, f);
      if (!authentic.ok)
        return {false, {}, authentic.error};
      if (!attrition_ids.insert(f.military_receipt_id).second)
        return {
            false,
            {},
            common_failure("PopulationAttritionAlreadyConsumed", f.id).error};
    } else if (f.kind == "population_enrolled" ||
               f.kind == "population_released") {
      if (!action || !action->success || action->phase != "completed" ||
          action->receipt_id != f.id || action->completed_tick != f.tick ||
          action->terms.source_id != f.source_id ||
          !f.military_receipt_id.empty() ||
          action->kind != (f.kind == "population_enrolled"
                               ? "population_enroll"
                               : "population_release") ||
          f.movements.size() != std::size_t(action->terms.count))
        return {false,
                {},
                common_failure("PopulationMilitaryActionSourceMissing", f.id)
                    .error};
    } else
      return {false,
              {},
              common_failure("UnknownPopulationMilitaryFactKind", f.id).error};
    std::set<StableId> touched;
    std::vector<StableId> token_order;
    for (const auto &move : f.movements) {
      auto owner = owners.find(move.member_id);
      if (owner == owners.end() || !touched.insert(move.member_id).second ||
          owner->second->id != move.enrollment_id ||
          statuses[move.member_id] != move.before ||
          f.tick < changed_tick[move.member_id])
        return {false,
                {},
                common_failure("PopulationMilitaryMovementSourceMismatch",
                               move.member_id)
                    .error};
      const auto &enrollment = *owner->second;
      const auto *source = c.catalog.sources.find(enrollment.source_id);
      if (move.from_cohort_id != cohort(*source, move.before) ||
          move.to_cohort_id != cohort(*source, move.after))
        return {false,
                {},
                common_failure("PopulationMilitaryMovementCohortMismatch",
                               move.member_id)
                    .error};
      if (f.kind == "population_enrolled") {
        if (enrollment.id != f.id || move.before != S::released ||
            move.after != S::fit || enrollment.source_id != f.source_id ||
            !move.captor_army_id.empty())
          return {false,
                  {},
                  common_failure("PopulationMilitaryEnrollmentMovementMismatch",
                                 move.member_id)
                      .error};
        seen_enrollment.insert(enrollment.id);
      } else if (f.kind == "population_released") {
        if (move.before != S::fit || move.after != S::released ||
            move.enrollment_id != action->terms.enrollment_id ||
            enrollment.source_id != f.source_id || !move.captor_army_id.empty())
          return {false,
                  {},
                  common_failure("PopulationMilitaryReleaseMovementMismatch",
                                 move.member_id)
                      .error};
      } else if (f.kind == "population_recovered") {
        if (move.before != S::wounded || move.after != S::fit ||
            !move.captor_army_id.empty())
          return {false,
                  {},
                  common_failure("PopulationRecoveryMovementMismatch",
                                 move.member_id)
                      .error};
      } else if (f.kind == "population_deserted") {
        if (move.before != S::fit || move.after != S::deserted ||
            !move.captor_army_id.empty())
          return {false,
                  {},
                  common_failure("PopulationAttritionMovementMismatch",
                                 move.member_id)
                      .error};
      } else {
        const MilitaryRosterReceipt *roster = nullptr;
        for (const auto &entry : *casualty)
          if (entry.unit_id == enrollment.unit_id)
            roster = &entry;
        if (!roster)
          return {false,
                  {},
                  common_failure("PopulationMilitaryCasualtyRosterMissing",
                                 move.member_id)
                      .error};
        auto after = receipt_status(*roster, move.member_id);
        if (!after.ok || after.value != move.after ||
            move.before == move.after || move.before == S::released ||
            move.before == S::dead || move.before == S::captured ||
            move.before == S::deserted ||
            (move.before == S::wounded && move.after == S::fit))
          return {false,
                  {},
                  common_failure("PopulationMilitaryCasualtyMismatch",
                                 move.member_id)
                      .error};
        StableId captor;
        for (const auto &pair : roster->captors)
          if (pair.first == move.member_id)
            captor = pair.second;
        if (move.captor_army_id !=
                (move.after == S::captured ? captor : StableId{}) ||
            (move.after == S::captured && captor.empty()))
          return {
              false,
              {},
              common_failure("PopulationMilitaryCaptorMismatch", move.member_id)
                  .error};
      }
      statuses[move.member_id] = move.after;
      changed_tick[move.member_id] = f.tick;
      token_order.push_back(move.member_id);
      if (move.after == S::dead) {
        const auto *member = origin(enrollment, move.member_id);
        result.losses.push_back({f.id, f.military_receipt_id, move.member_id,
                                 move.from_cohort_id, f.tick,
                                 member->labor_eligible});
      }
    }
    if (action && token_order != action->terms.member_ids)
      return {
          false,
          {},
          common_failure("PopulationMilitaryFactRosterMismatch", f.id).error};
    if (casualty)
      for (const auto &roster : *casualty)
        for (const auto &member : roster.member_ids) {
          auto owner = owners.find(member);
          if (owner == owners.end() || owner->second->completed_tick > f.tick ||
              fact_positions[owner->second->id] >=
                  fact_positions[f.military_receipt_id])
            continue;
          auto after = receipt_status(roster, member);
          if (!after.ok || statuses[member] != after.value)
            return {
                false,
                {},
                common_failure("PopulationMilitaryCasualtyNotConsumed", member)
                    .error};
        }
    for (const auto &source_row : c.catalog.sources.rows()) {
      Integer active = 0;
      for (const auto &owner : owners)
        if (owner.second->source_id == source_row.first &&
            statuses[owner.first] != S::released &&
            statuses[owner.first] != S::dead)
          ++active;
      const auto *consent =
          c.catalog.consents.find(source_row.second.service_consent_source_id);
      if (active > source_row.second.capacity ||
          active > consent->max_active_count)
        return {false,
                {},
                common_failure("PopulationMilitaryHistoricalCapacityExceeded",
                               source_row.first)
                    .error};
    }
    result.events.push_back(f);
  }
  if (c.selected_version == 4) {
    if (!c.military_exact || !c.actual_clock)
      return {false,
              {},
              common_failure("PopulationMilitaryExactSourcesRequired").error};
    auto now = c.actual_clock->read_current_moment();
    if (!now.ok)
      return {false, {}, now.error};
    for (const auto &row : c.military_exact->dynamic_casualty_facts.rows()) {
      auto order = compare_world_moments(row.second.effective_at, now.value);
      if (!order.ok)
        return {false, {}, order.error};
      if (order.value <= 0 && !casualty_ids.count(row.first))
        return {false,
                {},
                common_failure("PopulationMilitaryExactCasualtyNotConsumed",
                               row.first)
                    .error};
    }
  }
  // 后来再次伤亡不能掩盖曾发生但未交人口owner的恢复；逐原token永久源必须有且只有一次消费。
  for (const auto &recovery : c.military.recovery.facts)
    if (recovery.tick <= latest) {
      bool required = false;
      for (const auto &member : recovery.recovered_ids)
        if (owners.count(member))
          required = true;
      if (required && !recovery_ids.count(recovery.id))
        return {
            false,
            {},
            common_failure("PopulationRecoveryNotConsumed", recovery.id).error};
    }
  for (const auto &loss : c.military.supply_attrition.facts)
    if (loss.tick <= latest) {
      bool required = false;
      for (const auto &member : loss.deserted_ids)
        if (owners.count(member))
          required = true;
      if (required && !attrition_ids.count(loss.id))
        return {
            false,
            {},
            common_failure("PopulationAttritionNotConsumed", loss.id).error};
    }
  if (seen_enrollment.size() != c.state.enrollments.size())
    return {false,
            {},
            common_failure("PopulationMilitaryEnrollmentFactMissing").error};
  for (const auto &row : c.state.action_facts.rows()) {
    const auto &f = row.second;
    const auto *header = c.common.state().facts.find(f.id);
    if (row.first != f.id || !allocated_id(c.common, f.id, "action:") ||
        !header || header->kind != "population_military_action" ||
        header->domain != "population_military" ||
        !c.common.actor(f.actor_id) ||
        (f.kind != "population_enroll" && f.kind != "population_release") ||
        f.started_tick < 0 || f.started_tick > c.common.tick() ||
        f.terms.count <= 0)
      return {
          false,
          {},
          common_failure("InvalidPopulationMilitaryActionFact", f.id).error};
    const auto *enroll = std::get_if<PopulationEnrollCommand>(&f.request);
    const auto *release = std::get_if<PopulationReleaseCommand>(&f.request);
    const ActingAuthority &acting =
        enroll ? enroll->authority : release->authority;
    if ((f.kind == "population_enroll") != bool(enroll) ||
        acting.actor_id != f.actor_id ||
        acting.organization_id != f.terms.authority.organization_id ||
        acting.office_id != f.terms.authority.office_id ||
        acting.authority_epoch != f.terms.authority.authority_epoch ||
        (enroll &&
         (enroll->source_id != f.terms.source_id ||
          enroll->count != f.terms.count ||
          enroll->labor_count != f.terms.labor_count ||
          enroll->release_allocation_ids != f.terms.release_allocation_ids)) ||
        (release && release->enrollment_id != f.terms.enrollment_id))
      return {
          false,
          {},
          common_failure("PopulationMilitaryRequestTermsMismatch", f.id).error};
    if (enroll) {
      const auto *source = c.catalog.sources.find(f.terms.source_id);
      const auto *consent =
          source ? c.catalog.consents.find(source->service_consent_source_id)
                 : nullptr;
      if (!consent || f.started_tick < consent->from_tick ||
          f.started_tick > consent->through_tick ||
          (f.success && f.completed_tick > consent->through_tick))
        return {
            false,
            {},
            common_failure("PopulationMilitaryHistoricalConsentMissing", f.id)
                .error};
    }
    auto authority = historical_authority(c, f);
    if (!authority.ok)
      return {false, {}, authority.error};
    if (enroll) {
      const auto *source = c.catalog.sources.find(f.terms.source_id);
      if (!source)
        return {
            false,
            {},
            common_failure("UnknownPopulationMilitarySource", f.terms.source_id)
                .error};
      auto window = validate_population_recruitment_occupation_before(
          c, *source, f.started_tick, f.id);
      if (!window.ok)
        return {false, {}, window.error};
      if (f.success) {
        window = validate_population_recruitment_occupation_before(
            c, *source, f.completed_tick, f.receipt_id);
        if (!window.ok)
          return {false, {}, window.error};
      }
    }
    if (f.phase == "started") {
      auto pending = c.state.actions.find(f.id);
      auto action = c.common.action(f.id);
      auto unit = find(c.military.military_units, f.terms.unit_id);
      if (!pending || !action || action->kind != f.kind ||
          action->actor_id != f.actor_id ||
          action->start_tick != f.started_tick ||
          action->remaining_ticks != 1 ||
          action->participants != std::vector<StableId>{f.actor_id} ||
          !same_terms(pending->terms, f.terms) ||
          !same_request(pending->request, f.request) || !unit ||
          unit->mobilizing_action_id != f.id || f.completed_tick != -1 ||
          f.success || !f.receipt_id.empty())
        return {false,
                {},
                common_failure("InvalidPopulationMilitaryAction", f.id).error};
    } else if (f.phase == "completed" || f.phase == "cancelled") {
      if (c.state.actions.find(f.id) || f.completed_tick < f.started_tick ||
          f.completed_tick > latest ||
          (f.phase == "completed" && f.completed_tick != f.started_tick + 1) ||
          (f.phase == "cancelled" && f.success) ||
          f.success != !f.receipt_id.empty() ||
          (f.success && !c.state.facts.find(f.receipt_id)))
        return {false,
                {},
                common_failure("InvalidPopulationMilitaryTerminalAction", f.id)
                    .error};
    } else
      return {
          false,
          {},
          common_failure("InvalidPopulationMilitaryActionPhase", f.id).error};
  }
  std::set<StableId> pending_units, pending_cohorts;
  for (const auto &row : c.state.actions.rows())
    if (row.first != row.second.action_id ||
        !c.state.action_facts.find(row.first) ||
        !pending_units.insert(row.second.terms.unit_id).second ||
        !pending_cohorts.insert(row.second.terms.reserve_cohort_id).second)
      return {false,
              {},
              common_failure("DuplicatePopulationMilitaryOccupation", row.first)
                  .error};
  std::set<std::pair<StableId, Integer>> fed;
  for (const auto &receipt : c.military.supply_facts)
    for (const auto &member : receipt.mouth_ids)
      if (owners.count(member)) {
        auto payer =
            responsibility(c, *owners.at(member), member, receipt.tick, true);
        if (!payer.ok)
          return {false, {}, payer.error};
        if (!population_military_historical_member(c, member, receipt.tick) ||
            !fed.insert({member, receipt.tick}).second ||
            (payer.value.status != S::fit && payer.value.status != S::wounded &&
             payer.value.status != S::captured) ||
            payer.value.army_id != receipt.army_id ||
            payer.value.garrison_id != receipt.garrison_id)
          return {false,
                  {},
                  common_failure("PopulationMilitaryHistoricalSupplyMismatch",
                                 member)
                      .error};
      }
  for (const auto &pair : owners) {
    const auto &member = pair.first;
    const auto &e = *pair.second;
    const auto *source = c.catalog.sources.find(e.source_id);
    const auto *unit = find(c.military.military_units, e.unit_id);
    if (!unit)
      return {false,
              {},
              common_failure("PopulationMilitaryUnitMissing", e.unit_id).error};
    auto current = actual_status(*unit, member);
    if (!current.ok || current.value != statuses[member])
      return {false,
              {},
              common_failure("PopulationMilitaryCurrentRosterMismatch", member)
                  .error};
    const auto status = current.value;
    if (status == S::dead || status == S::released || status == S::deserted)
      continue;
    PopulationMilitaryProvision proof;
    proof.source_id = e.source_id;
    proof.cohort_id = cohort(*source, status);
    proof.member_id = member;
    proof.status = status;
    proof.labor_eligible = origin(e, member)->labor_eligible;
    auto expected = responsibility(c, e, member, latest, false);
    if (!expected.ok)
      return {false, {}, expected.error};
    if (expected.value.status != status)
      return {false,
              {},
              common_failure("PopulationMilitaryResponsibilityMismatch", member)
                  .error};
    if (status == S::captured) {
      StableId keeper;
      for (const auto &pair : unit->captors)
        if (pair.first == member)
          keeper = pair.second;
      if (keeper != expected.value.keeper_army_id)
        return {
            false,
            {},
            common_failure("PopulationMilitaryCustodyHistoryMismatch", member)
                .error};
    } else if (unit->army_id != expected.value.army_id ||
               unit->garrison_id != expected.value.garrison_id)
      return {
          false,
          {},
          common_failure("PopulationMilitaryLocationHistoryMismatch", member)
              .error};
    proof.army_id = expected.value.army_id;
    proof.garrison_id = expected.value.garrison_id;
    if (!proof.army_id.empty()) {
      const auto *army = find(c.military.armies, proof.army_id);
      if (!army || army->status != "active" ||
          (status == S::captured ? !has(army->prisoner_ids, member)
                                 : !has(army->unit_ids, unit->id)))
        return {
            false,
            {},
            common_failure("PopulationMilitaryArmyMouthMissing", member).error};
      proof.inventory_id =
          army->inventory_id; // 先读同一 Field 真实绑定，不能让 Common
                              // 的冻结锚点冒当前军供位置。
      std::optional<FieldBattleSpatialView> field_position;
      if (c.field_battle) {
        auto actual = read_field_battle_actor_position(
            c.common, c.military, *c.field_battle, army->commander_actor_id);
        if (!actual.ok)
          return {false, {}, actual.error};
        field_position = std::move(actual.value);
      }
      if (field_position)
        proof.location = std::move(*field_position);
      else {
        auto location = c.common.resolved_location(army->commander_actor_id);
        if (!location.ok)
          return {false, {}, location.error};
        proof.location = std::move(location.value);
      }
    } else {
      const auto *camp = find(c.military_catalog.garrisons, proof.garrison_id);
      const auto *garrison = find(c.military.garrisons, proof.garrison_id);
      if (!camp || !garrison ||
          (status == S::captured && !has(garrison->prisoner_ids, member)))
        return {false,
                {},
                common_failure("PopulationMilitaryCaptorMouthMissing", member)
                    .error};
      proof.inventory_id = camp->inventory_id;
      proof.location = Location{SettlementLocation{{}, camp->place_id}};
    }
    if (!c.common.inventory(proof.inventory_id))
      return {false,
              {},
              common_failure("PopulationMilitarySupplySourceMissing", member)
                  .error};
    if (changed_tick[member] < c.common.tick()) {
      bool supplied = false;
      for (const auto &receipt : c.military.supply_facts)
        if (receipt.tick == c.common.tick() && has(receipt.mouth_ids, member))
          supplied = true;
      if (!supplied)
        return {false,
                {},
                common_failure("PopulationMilitaryActualSupplyMissing", member)
                    .error};
    }
    result.provisions.push_back(std::move(proof));
  }
  return {true, std::move(result), {}};
}
} // namespace
// 同tick仍按全局来源顺序证明先整编、再开始返乡卸装、最后取得真实归营回执。
Status validate_population_military_home_return_source(
    const PopulationMilitaryReadContext &c, const StableId &integration_id,
    const StableId &return_id) {
  const auto *continued = c.state.integration_facts.find(integration_id);
  const auto *integrated =
      continued
          ? population_military_detail::find(c.military.integration_facts,
                                             continued->military_receipt_id)
          : nullptr;
  const auto *returned =
      population_military_detail::find(c.military.return_facts, return_id);
  if (!continued || !integrated || !returned ||
      returned->army_id != continued->to_army_id ||
      integrated->recipient_army_id != returned->army_id)
    return common_failure("PopulationIntegrationHomeReturnOrder", return_id);
  auto authentic = return_source(c, *returned);
  if (!authentic.ok)
    return authentic;
  const MilitaryLogisticsFact *action = nullptr;
  for (const auto &row : c.military.logistics_facts)
    if (row.result && row.result->receipt_id == return_id)
      action = &row;
  std::map<StableId, std::size_t> positions;
  for (std::size_t i = 0; i < c.common.state().facts.order().size(); ++i)
    positions[c.common.state().facts.order()[i]] = i;
  if (!action || action->start_tick < integrated->tick ||
      !positions.count(integrated->id) || !positions.count(continued->id) ||
      !positions.count(action->id) || !positions.count(return_id) ||
      positions[integrated->id] >= positions[continued->id] ||
      positions[continued->id] >= positions[action->id] ||
      positions[action->id] >= positions[return_id])
    return common_failure("PopulationIntegrationHomeReturnOrder", return_id);
  return common_success();
}
// 原归营源及每一份已接收军籍均须先于本次卸装，公开接缝不接受无先后关系的同tick事实。
Status validate_population_military_return_source(
    const PopulationMilitaryReadContext &c, const StableId &id) {
  const auto *receipt =
      population_military_detail::find(c.military.return_facts, id);
  if (!receipt)
    return common_failure("PopulationMilitaryReturnSourceMismatch", id);
  auto checked = return_source(c, *receipt);
  if (!checked.ok)
    return checked;
  for (const auto &row : c.state.integration_facts.rows())
    if (row.second.to_army_id == receipt->army_id &&
        population_military_detail::has(receipt->unit_ids,
                                        row.second.unit_id)) {
      auto ordered =
          validate_population_military_home_return_source(c, row.first, id);
      if (!ordered.ok)
        return ordered;
    }
  return common_success();
}
// 伤亡只能来自原会战的有序结算链；行、全局头和人物分类本身不足以证明伤亡。
Status validate_population_military_casualty_source(
    const PopulationMilitaryReadContext &c, const StableId &receipt_id) {
  auto limit = boundary_tick(c);
  if (!limit.ok)
    return {false, {}, limit.error};
  const auto *receipt = find(c.military.casualty_facts, receipt_id);
  if (receipt && (receipt->battle_id.rfind("naval_round:", 0) == 0 ||
                  receipt->battle_id.rfind("naval_capture:", 0) == 0)) {
    if (!c.naval_combat)
      return common_failure("NavalCombatProfileRequired", receipt_id);
    return validate_naval_casualty_receipt(
        c.common, c.military, *c.naval_combat, receipt_id, limit.value);
  }
  const auto *fact =
      receipt ? find(c.military.battle_facts, receipt->battle_id) : nullptr;
  const auto *battle =
      fact ? find(c.military.battles, fact->session_id) : nullptr;
  const auto *contact =
      battle ? find(c.military.contact_facts, battle->source_id) : nullptr;
  auto fail = [&]() {
    return common_failure("PopulationMilitaryCasualtySettlementMissing",
                          receipt_id);
  };
  if (!receipt || !fact || !battle || !contact || !contact->battle_id ||
      *contact->battle_id != battle->id || receipt->tick != fact->tick ||
      receipt->tick < 0 || receipt->tick > limit.value ||
      fact->phase != "completed" || battle->created_tick < 0 ||
      battle->created_tick > receipt->tick || battle->last_tick > limit.value ||
      battle->completed_tick > limit.value ||
      !has(battle->round_receipt_ids, receipt_id) ||
      !unique(battle->round_receipt_ids) ||
      !has(std::vector<StableId>{"planning", "result", "closed"},
           battle->phase))
    return fail();
  std::map<StableId, std::size_t> positions;
  for (std::size_t i = 0; i < c.common.state().facts.order().size(); ++i)
    positions[c.common.state().facts.order()[i]] = i;
  auto header = [&](const StableId &id, const StableId &kind) {
    const auto *h = c.common.state().facts.find(id);
    return h && h->domain == "military" && h->kind == kind;
  };
  if (!header(contact->id, "military_contact"))
    return fail();
  std::vector<StableId> units;
  for (const auto &u : battle->units)
    units.push_back(u.unit_id);
  std::sort(units.begin(), units.end());
  auto contact_units = contact->unit_ids;
  std::sort(contact_units.begin(), contact_units.end());
  if (!unique(units) || units != contact_units || !unique(battle->army_ids))
    return fail();
  auto shaped = *battle;
  if (shaped.phase == "closed")
    shaped.phase = "result";
  auto valid =
      validate_military_battle(shaped, c.military_catalog.battle_catalog, true);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &unit : battle->units) {
    const auto *army = find(c.military.armies, unit.army_id);
    const auto *formed =
        army ? find(c.military.action_facts, army->formation_source) : nullptr;
    const auto *terms =
        formed ? std::get_if<ArmyFormationTerms>(&formed->terms) : nullptr;
    bool contact_army = false;
    for (const auto &row : contact->army_order_ids)
      if (row.first == unit.army_id && army && row.second == army->order_id)
        contact_army = true;
    bool side = false;
    for (const auto &row : contact->side_by_army)
      if (row.first == unit.army_id && row.second == unit.side)
        side = true;
    if (!army || !formed || !terms || !header(formed->id, "military_action") ||
        formed->command != "army_form" || formed->phase != "completed" ||
        !formed->success || formed->completed_tick > battle->created_tick ||
        !formed->result || formed->result->army_id != army->id ||
        !has(military_army_units_at(c.military, army->id, battle->created_tick,
                                    contact->id),
             unit.unit_id) ||
        !contact_army || !side)
      return fail();
  }
  Integer previous = battle->created_tick;
  std::set<StableId> seen_rounds;
  for (std::size_t n = 0; n < battle->round_receipt_ids.size(); ++n) {
    const auto *r =
        find(c.military.casualty_facts, battle->round_receipt_ids[n]);
    const auto *round =
        r ? find(c.military.battle_facts, r->battle_id) : nullptr;
    const bool final = n + 1 == battle->round_receipt_ids.size() &&
                       battle->phase != "planning";
    if (!r || !round || !header(r->id, "military_casualties") ||
        !header(round->id, "military_battle") ||
        positions[contact->id] >= positions[round->id] ||
        positions[round->id] >= positions[r->id] ||
        !seen_rounds.insert(round->id).second || round->phase != "completed" ||
        round->session_id != battle->id || round->round != Integer(n) + 1 ||
        round->final != final || round->army_ids != battle->army_ids ||
        round->unit_ids != units || round->contact_source_id != contact->id ||
        round->source_id != (final ? contact->id : round->id) ||
        (final && round->id != battle->id) || r->tick != round->tick ||
        r->tick > limit.value || round->start_tick < previous ||
        round->tick < round->start_tick)
      return fail();
    const bool terminated =
        final && battle->result &&
        has(std::vector<StableId>{"authority_lost", "responsible_actor_lost",
                                  "truce", "cancelled", "timeout"},
            battle->result->reason);
    if (terminated) {
      if (round->start_tick != round->tick ||
          (round->tick != previous && round->tick != previous + 1) ||
          battle->last_tick != previous)
        return fail();
    } else if (round->start_tick != previous || round->tick != previous + 1 ||
               (n + 1 == battle->round_receipt_ids.size() &&
                battle->last_tick != round->tick))
      return fail();
    if (round->result.has_value() != (final && battle->result.has_value()) ||
        round->winner_side !=
            (final && battle->result ? battle->result->winner_side : -1))
      return fail();
    if (round->result &&
        (round->result->winner_side != battle->result->winner_side ||
         round->result->reason != battle->result->reason))
      return fail();
    std::vector<StableId> roster_ids;
    for (const auto &roster : r->units) {
      roster_ids.push_back(roster.unit_id);
      const BattleUnit *session_unit = nullptr;
      for (const auto &u : battle->units)
        if (u.unit_id == roster.unit_id)
          session_unit = &u;
      if (!session_unit || roster.member_ids != session_unit->member_ids ||
          !unique(roster.member_ids))
        return fail();
      for (const auto &member : roster.member_ids) {
        auto status = receipt_status(roster, member);
        if (!status.ok || status.value == S::released)
          return fail();
      }
      std::vector<StableId> classified = roster.fit;
      for (const auto *group :
           {&roster.wounded, &roster.dead, &roster.captured, &roster.deserted})
        classified.insert(classified.end(), group->begin(), group->end());
      std::sort(classified.begin(), classified.end());
      auto members = roster.member_ids;
      std::sort(members.begin(), members.end());
      if (classified != members)
        return fail();
      if (n + 1 == battle->round_receipt_ids.size() &&
          (roster.fit != session_unit->fit ||
           roster.wounded != session_unit->wounded ||
           roster.dead != session_unit->dead ||
           roster.captured != session_unit->captured ||
           roster.deserted != session_unit->deserted))
        return fail();
    }
    std::sort(roster_ids.begin(), roster_ids.end());
    if (roster_ids != units)
      return fail();
    previous = round->tick;
  }
  if (battle->phase == "planning") {
    if (battle->applied || battle->completed_tick != -1 ||
        !battle->settlement_receipt_id.empty() ||
        battle->round != Integer(battle->round_receipt_ids.size()) + 1)
      return fail();
  } else if (!battle->applied ||
             battle->round != Integer(battle->round_receipt_ids.size()) ||
             battle->round_receipt_ids.empty() ||
             battle->settlement_receipt_id !=
                 battle->round_receipt_ids.back() ||
             battle->completed_tick != previous)
    return fail();
  return common_success();
}

// 冷续接记录必须同时追溯军事收据、原军籍、独立服役许可和冻结的旧新工资链。
Status validate_population_military_integrations(
    const PopulationMilitaryReadContext &c) {
  using namespace population_military_detail;
  auto boundary = boundary_tick(c);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  std::set<StableId> sources, used;
  for (const auto &row : c.state.integration_facts.rows()) {
    const auto &fact = row.second;
    const auto *header = c.common.state().facts.find(fact.id);
    const auto *military =
        find(c.military.integration_facts, fact.military_receipt_id);
    const auto *policy = c.catalog.integration_policies.find(fact.policy_id);
    const auto *source =
        policy ? c.catalog.sources.find(policy->source_id) : nullptr;
    const auto *consent =
        policy ? c.catalog.consents.find(policy->service_consent_source_id)
               : nullptr;
    const auto *army =
        military ? find(c.military.armies, military->recipient_army_id)
                 : nullptr;
    if (row.first != fact.id ||
        !allocated_id(c.common, fact.id, "population_integration:") ||
        !header || header->domain != "population_military" ||
        header->kind != "population_military_integration" || !military ||
        !has(military->population_receipt_ids, fact.id) ||
        !has(military->population_policy_ids, fact.policy_id) || !policy ||
        !source || !consent || !army || !sources.insert(source->id).second ||
        fact.source_id != source->id || fact.unit_id != source->unit_id ||
        fact.tick != military->tick || fact.tick > boundary.value ||
        fact.tick < consent->from_tick || fact.tick > consent->through_tick ||
        fact.to_army_id != military->recipient_army_id ||
        policy->receiving_organization_id !=
            military->recipient_organization_id ||
        policy->receiving_garrison_id != army->garrison_id ||
        fact.service_consent_source_id != policy->service_consent_source_id ||
        consent->cohort_id != source->reserve_cohort_id ||
        consent->organization_id != policy->receiving_organization_id ||
        consent->garrison_id != policy->receiving_garrison_id)
      return common_failure("PopulationIntegrationSourceMismatch", fact.id);
    const SiegeIntegrationUnit *snapshot = nullptr;
    for (const auto &old : military->source_armies)
      if (old.id == fact.from_army_id)
        for (const auto &unit : old.units)
          if (unit.id == source->unit_id)
            snapshot = &unit;
    if (!snapshot || snapshot->member_ids != fact.member_ids ||
        !unique(fact.member_ids) ||
        static_cast<Integer>(snapshot->fit.size() + snapshot->wounded.size() +
                             snapshot->captured.size() +
                             snapshot->deserted.size()) >
            consent->max_active_count ||
        fact.old_wage_obligation_ids != source->wage_obligation_ids ||
        fact.next_due_ticks.size() != source->wage_obligation_ids.size() ||
        fact.wage_remainders.size() != source->wage_obligation_ids.size() ||
        fact.receiving_wage_remainders_before.size() !=
            source->wage_obligation_ids.size() ||
        fact.new_wage_obligation_ids.size() !=
            source->wage_obligation_ids.size() ||
        !unique(fact.new_wage_obligation_ids))
      return common_failure("PopulationIntegrationRosterMismatch", fact.id);
    auto actual = fact.new_wage_obligation_ids,
         expected = policy->wage_obligation_ids;
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    if (actual != expected)
      return common_failure("PopulationIntegrationWageMismatch", fact.id);
    for (std::size_t i = 0; i < fact.old_wage_obligation_ids.size(); ++i) {
      const auto *old =
          c.economy.obligations.find(fact.old_wage_obligation_ids[i]);
      const auto *next =
          c.economy.obligations.find(fact.new_wage_obligation_ids[i]);
      const auto *seed =
          next ? c.economy_catalog.obligations.find(next->id) : nullptr;
      if (!old || !next || !seed || seed->active || old->active ||
          !next->active || !used.insert(next->id).second ||
          old->next_due_tick != fact.next_due_ticks[i] ||
          fact.next_due_ticks[i] <= fact.tick ||
          next->next_due_tick < fact.next_due_ticks[i] ||
          next->period_ticks <= 0 ||
          (next->next_due_tick - fact.next_due_ticks[i]) % next->period_ticks !=
              0 ||
          next->beneficiary.kind != old->beneficiary.kind ||
          next->beneficiary.id != old->beneficiary.id ||
          next->destination != old->destination ||
          next->rate_profile != old->rate_profile ||
          next->period_ticks != old->period_ticks ||
          next->principal.kind != SubjectKind::organization ||
          next->principal.id != policy->receiving_organization_id)
        return common_failure("PopulationIntegrationWageContinuity", fact.id);
      const auto *rate = c.economy_catalog.rates.find(old->rate_profile);
      const auto *old_remainder = c.economy.obligation_remainders.find(old->id);
      const auto *new_remainder =
          c.economy.obligation_remainders.find(next->id);
      if (!rate || !old_remainder || !new_remainder ||
          fact.receiving_wage_remainders_before[i] != 0 ||
          fact.wage_remainders[i] < 0 ||
          fact.wage_remainders[i] >= rate->denominator ||
          *old_remainder != fact.wage_remainders[i] || *new_remainder < 0 ||
          *new_remainder >= rate->denominator ||
          (next->next_due_tick == fact.next_due_ticks[i] &&
           *new_remainder != fact.wage_remainders[i]))
        return common_failure("PopulationIntegrationWageRemainderMismatch",
                              fact.id);
      for (const auto &debt : c.economy.arrears.rows()) {
        if (debt.second.obligation_id == old->id &&
            debt.second.due_tick > fact.tick)
          return common_failure("PopulationIntegrationOldWageAccrued",
                                debt.first);
        if (debt.second.obligation_id == next->id &&
            debt.second.due_tick < fact.next_due_ticks[i])
          return common_failure("PopulationIntegrationEarlyReceivingWage",
                                debt.first);
      }
    }

    if (fact.old_ration_obligation_ids.size() !=
            fact.new_ration_obligation_ids.size() ||
        fact.new_ration_obligation_ids.size() !=
            fact.ration_next_due_ticks.size() ||
        fact.new_ration_obligation_ids.size() !=
            fact.ration_remainders.size() ||
        fact.new_ration_obligation_ids.size() !=
            fact.receiving_ration_remainders_before.size() ||
        fact.new_ration_obligation_ids.size() !=
            fact.ration_provision_binding_ids.size() ||
        !unique(fact.old_ration_obligation_ids) ||
        !unique(fact.new_ration_obligation_ids) ||
        !unique(fact.ration_provision_binding_ids))
      return common_failure("PopulationIntegrationRationContinuity", fact.id);
    auto actual_rations = fact.new_ration_obligation_ids,
         expected_rations = policy->ration_obligation_ids;
    std::sort(actual_rations.begin(), actual_rations.end());
    std::sort(expected_rations.begin(), expected_rations.end());
    const auto *hold =
        find(c.military_catalog.strongholds, policy->target_stronghold_id);
    if (actual_rations != expected_rations ||
        (!actual_rations.empty() &&
         (!hold || hold->id != military->stronghold_id ||
          hold->garrison_id != source->garrison_id)))
      return common_failure("PopulationIntegrationRationTargetMismatch",
                            fact.id);
    for (std::size_t i = 0; i < fact.old_ration_obligation_ids.size(); ++i) {
      const auto *old =
          c.economy.obligations.find(fact.old_ration_obligation_ids[i]);
      const auto *next =
          c.economy.obligations.find(fact.new_ration_obligation_ids[i]);
      const auto *seed =
          next ? c.economy_catalog.obligations.find(next->id) : nullptr;
      const auto *binding = c.economy.provision_bindings.find(
          fact.ration_provision_binding_ids[i]);
      if (!old || !next || !seed || seed->active ||
          old->kind != ObligationKind::ration ||
          next->kind != ObligationKind::ration || old->active ||
          !next->active || old->principal.kind != SubjectKind::organization ||
          old->principal.id != source->organization_id ||
          next->principal.kind != SubjectKind::organization ||
          next->principal.id != policy->receiving_organization_id ||
          !used.insert(next->id).second ||
          old->next_due_tick != fact.ration_next_due_ticks[i] ||
          fact.ration_next_due_ticks[i] <= fact.tick ||
          next->next_due_tick < fact.ration_next_due_ticks[i] ||
          next->period_ticks <= 0 ||
          (next->next_due_tick - fact.ration_next_due_ticks[i]) %
                  next->period_ticks !=
              0 ||
          next->beneficiary.kind != old->beneficiary.kind ||
          next->beneficiary.id != old->beneficiary.id ||
          next->source_wallet_or_inventory != old->source_wallet_or_inventory ||
          !has(hold->public_inventory_ids, old->source_wallet_or_inventory) ||
          next->rate_profile != old->rate_profile ||
          next->period_ticks != old->period_ticks ||
          next->item_id != old->item_id || !binding ||
          binding->policy_owner != ProvisionOwner::macro ||
          binding->subject.kind != next->beneficiary.kind ||
          binding->subject.id != next->beneficiary.id ||
          binding->payer_inventory != next->source_wallet_or_inventory ||
          binding->obligation_id != next->id)
        return common_failure("PopulationIntegrationRationContinuity", fact.id);
      const auto *rate = c.economy_catalog.rates.find(old->rate_profile);
      const auto *old_remainder = c.economy.obligation_remainders.find(old->id);
      const auto *new_remainder =
          c.economy.obligation_remainders.find(next->id);
      if (!rate || !old_remainder || !new_remainder ||
          fact.receiving_ration_remainders_before[i] != 0 ||
          fact.ration_remainders[i] < 0 ||
          fact.ration_remainders[i] >= rate->denominator ||
          *old_remainder != fact.ration_remainders[i] || *new_remainder < 0 ||
          *new_remainder >= rate->denominator ||
          (next->next_due_tick == fact.ration_next_due_ticks[i] &&
           *new_remainder != fact.ration_remainders[i]))
        return common_failure("PopulationIntegrationRationRemainderMismatch",
                              fact.id);
      for (const auto &receipt : c.economy.rations.rows()) {
        if (receipt.second.obligation_id == old->id &&
            receipt.second.due_tick >= fact.ration_next_due_ticks[i])
          return common_failure("PopulationIntegrationOldRationContinued",
                                receipt.first);
        if (receipt.second.obligation_id == next->id &&
            receipt.second.due_tick < fact.ration_next_due_ticks[i])
          return common_failure("PopulationIntegrationEarlyReceivingRation",
                                receipt.first);
      }
    }
  }
  for (const auto &policy_row : c.catalog.integration_policies.rows()) {
    const auto &policy = policy_row.second;
    for (const auto *ids :
         {&policy.wage_obligation_ids, &policy.ration_obligation_ids})
      for (const auto &id : *ids)
        if (!used.count(id)) {
          const auto *obligation = c.economy.obligations.find(id);
          const auto *seed = c.economy_catalog.obligations.find(id);
          const auto *remainder = c.economy.obligation_remainders.find(id);
          if (!obligation || !seed || !remainder || *remainder != 0 ||
              seed->active || obligation->active ||
              !obligation->last_receipt_id.empty() ||
              obligation->next_due_tick != seed->next_due_tick)
            return common_failure("PopulationIntegrationUnprovenActivation",
                                  id);
        }
  }
  for (const auto &military : c.military.integration_facts) {
    std::set<StableId> expected;
    for (const auto &source_army : military.source_armies)
      for (const auto &unit : source_army.units)
        for (const auto &source : c.catalog.sources.rows())
          if (source.second.unit_id == unit.id)
            expected.insert(source.first);
    std::set<StableId> actual;
    for (const auto &row : c.state.integration_facts.rows())
      if (row.second.military_receipt_id == military.id)
        actual.insert(row.second.source_id);
    if (expected != actual)
      return common_failure("PopulationIntegrationReceiptMissing", military.id);
    const auto *hold =
        find(c.military_catalog.strongholds, military.stronghold_id);
    if (hold) {
      std::set<StableId> expected_rations, actual_rations;
      for (const auto &row : c.economy_catalog.obligations.rows())
        if (row.second.active && row.second.kind == ObligationKind::ration &&
            has(hold->public_inventory_ids,
                row.second.source_wallet_or_inventory))
          expected_rations.insert(row.first);
      for (const auto &row : c.state.integration_facts.rows())
        if (row.second.military_receipt_id == military.id)
          for (const auto &id : row.second.old_ration_obligation_ids)
            if (!actual_rations.insert(id).second)
              return common_failure("PopulationIntegrationDuplicateRation", id);
      if (expected_rations != actual_rations)
        return common_failure("PopulationIntegrationRationCoverage",
                              military.id);
    }
  }
  return common_success();
}
// 训练开始边界包含刚完成登记，但排除同边界已经退籍或受伤的token。
bool population_military_training_member(const PopulationMilitaryReadContext &c,
                                         const StableId &source_id,
                                         const StableId &unit_id,
                                         const StableId &member_id,
                                         Integer start_tick) {
  auto limit = boundary_tick(c);
  if (!c.catalog.enabled || !limit.ok || start_tick < 0 ||
      start_tick > limit.value)
    return false;
  const auto *source = c.catalog.sources.find(source_id);
  if (!source || source->unit_id != unit_id)
    return false;
  for (const auto &row : c.state.enrollments.rows())
    if (row.second.source_id == source_id && row.second.unit_id == unit_id &&
        origin(row.second, member_id)) {
      if (row.second.completed_tick > start_tick)
        return false;
      auto status = S::released;
      for (const auto &event : c.state.facts.rows())
        if (event.second.tick <= start_tick) {
          if (event.second.occurred_at) {
            auto at = normalize_world_moment(start_tick, {});
            if (!at.ok)
              return false;
            auto order =
                compare_world_moments(*event.second.occurred_at, at.value);
            if (!order.ok)
              return false;
            if (order.value > 0)
              continue;
          }
          for (const auto &move : event.second.movements)
            if (move.member_id == member_id)
              status = move.after;
        }
      return status == S::fit;
    }
  return false;
}

// 只重建来源证明；人口owner随后用它核实每一笔转移和明确死亡sink。
Outcome<PopulationContinuity>
population_military_continuity(const PopulationMilitaryReadContext &c) {
  return continuity(c);
}
// 冷准入要求军籍事实链与实际人口账同时成立。
Status validate_population_military(const PopulationMilitaryReadContext &c) {
  auto proof = continuity(c);
  if (!proof.ok)
    return {false, {}, proof.error};
  return validate_population_continuity(c.common, c.economy, c.economy_catalog,
                                        proof.value);
}
// 当前军籍来源列表保持所有未退籍身份，包括明确死亡、被俘和离队历史。
Outcome<std::vector<StableId>>
population_military_roster(const PopulationMilitaryReadContext &c,
                           const StableId &source_id, const StableId &unit_id) {
  auto boundary = boundary_tick(c);
  if (!boundary.ok)
    return {false, {}, boundary.error};
  const auto *source = c.catalog.sources.find(source_id);
  if (!c.catalog.enabled || !source || source->unit_id != unit_id)
    return {false,
            {},
            common_failure("MilitaryPopulationSourceMissing", source_id).error};
  std::vector<StableId> result;
  for (const auto &row : c.state.enrollments.rows())
    if (row.second.source_id == source_id && row.second.unit_id == unit_id) {
      if (row.second.completed_tick > boundary.value)
        return {false,
                {},
                common_failure("PopulationMilitaryFutureEnrollment", row.first)
                    .error};
      for (const auto &member : row.second.members)
        if (recorded_status(c.state, member.member_id) != S::released)
          result.push_back(member.member_id);
    }
  return {true, std::move(result), {}};
}
// 当半日先供养后切换身份；只有登记后、明确退籍前含该边界的历史军粮合法。
bool population_military_historical_member(
    const PopulationMilitaryReadContext &c, const StableId &member,
    Integer tick) {
  auto boundary = boundary_tick(c);
  if (!c.catalog.enabled || !boundary.ok || tick < 0 || tick > boundary.value)
    return false;
  for (const auto &row : c.state.enrollments.rows())
    if (origin(row.second, member)) {
      if (tick <= row.second.completed_tick)
        return false;
      for (const auto &f : c.state.facts.rows())
        for (const auto &move : f.second.movements)
          if (move.member_id == member &&
              (move.after == S::released || move.after == S::dead ||
               move.after == S::deserted) &&
              tick > f.second.tick)
            return false;
      return true;
    }
  return false;
}
// 未选军籍的否定证明仍读实际目录；空表或 null reader 自身不是权限。
Status validate_unselected_population_military_sources(
    const CommonStore &common, const LocalEconomyState &economy,
    const LocalEconomyCatalog &economy_catalog, const MilitaryState &military,
    const MilitaryCatalog &military_catalog, const PopulationMilitaryState &state,
    const PopulationMilitaryCatalog *catalog, const CatalogIdentity &identity) {
  const auto &actual = common.state();
  const auto reject = [](const StableId &id = {}) {
    return common_failure("UnselectedPopulationMilitaryHasSources", id);
  };
  if (actual.metadata.content_id != identity.content_id ||
      actual.metadata.content_version != identity.content_version ||
      actual.metadata.content_digest != identity.content_digest ||
      actual.metadata.ruleset_id != identity.ruleset_id)
    return common_failure("PopulationMilitaryActualCatalogIdentityRequired");
  for (const auto &module : identity.module_versions)
    if (module.first == "population_military") return reject(module.first);
  if (catalog && (catalog->enabled || catalog->selected_version != 0 ||
                  !catalog->sources.empty() || !catalog->consents.empty() ||
                  !catalog->integration_policies.empty()))
    return reject("population_military/catalog");
  if (!state.enrollments.empty() || !state.actions.empty() ||
      !state.action_facts.empty() || !state.facts.empty() ||
      !state.integration_facts.empty() || !economy.population_losses.empty())
    return reject("population_military/state");
  const auto fact_kind = [](const StableId &kind) {
    return kind == "population_military_action" ||
           kind == "population_enrolled" || kind == "population_released" ||
           kind == "population_casualty" || kind == "population_recovered" ||
           kind == "population_deserted" ||
           kind == "population_military_integration" || kind == "population_loss";
  };
  const auto source_id = [](const StableId &id) {
    return id.starts_with("population_military:") ||
           id.starts_with("population_integration:");
  };
  // 错 domain 的保留 kind 也不得被解释为“与军籍无关”。
  for (const auto &[id, head] : actual.facts.rows())
    if (head.domain == "population_military" || fact_kind(head.kind) || source_id(id))
      return reject(id);
  for (const auto &[id, action] : actual.actions.rows())
    if (population_military_command_kind(action.kind)) return reject(id);
  for (const auto &history : actual.history)
    if (population_military_command_kind(history.kind)) return reject(history.result_id);
  for (const auto &[id, result] : actual.results.rows())
    if (result && population_military_command_kind(result->kind)) return reject(id);
  for (const auto &[id, transfer] : economy.population_transfers.rows())
    if (fact_kind(transfer.reason) || source_id(transfer.source_fact_id)) return reject(id);
  for (const auto *bindings : {&economy.provision_bindings, &economy_catalog.provision_bindings})
    for (const auto &[id, binding] : bindings->rows())
      if (binding.policy_owner == ProvisionOwner::military || !binding.military_source_id.empty())
        return reject(id);
  for (const auto &garrison : military_catalog.garrisons)
    for (const auto &unit : garrison.units)
      if (unit.population_source_id) return reject(unit.id);
  const auto no_population_members = [](const auto &ids) {
    return std::none_of(ids.begin(), ids.end(), [](const auto &id) {
      return id.starts_with("soldier:population:");
    });
  };
  const auto roster_clear = [&](const auto &unit) {
    return no_population_members(unit.member_ids) && no_population_members(unit.fit) &&
           no_population_members(unit.wounded) && no_population_members(unit.dead) &&
           no_population_members(unit.captured) && no_population_members(unit.deserted);
  };
  for (const auto &unit : military.military_units) {
    if (!roster_clear(unit)) return reject(unit.id);
    for (const auto &[actor, keeper] : unit.captors)
      if (actor.starts_with("soldier:population:")) return reject(actor);
    if (!unit.mobilizing_action_id.empty()) {
      // 原成军也使用同一锁；只允许仍有真实原 army_form Action 的该分支。
      const auto *action = actual.actions.find(unit.mobilizing_action_id);
      if (!action || action->kind != "army_form") return reject(unit.mobilizing_action_id);
    }
  }
  for (const auto &army : military.armies)
    if (!no_population_members(army.prisoner_ids)) return reject(army.id);
  for (const auto &garrison : military.garrisons)
    if (!no_population_members(garrison.prisoner_ids)) return reject(garrison.id);
  for (const auto &fact : military.casualty_facts)
    for (const auto &unit : fact.units) if (!roster_clear(unit)) return reject(fact.id);
  for (const auto &battle : military.battles)
    for (const auto &unit : battle.units) if (!roster_clear(unit)) return reject(battle.id);
  for (const auto &returned : military.return_facts)
    if (!no_population_members(returned.prisoner_ids)) return reject(returned.id);
  return common_success();
}
} // namespace hanmo::resident
