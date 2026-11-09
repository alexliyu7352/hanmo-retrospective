#include "private_shop_age3.hpp"
// 私铺双方本人接受只创建原LE工资义务；尚未实际到岗不预约整月工资，也不先确认收入。
#include "private_shop_operation.hpp"
#include "private_shop_operation_sources.hpp"
#include "private_shop_staff.hpp"
#include "resident/domains/family/actor_death_read.hpp"
#include "resident/domains/family/family_life_history4.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
const DialogueNode *node(const InteractionCatalog &catalog,
                         const StableId &dialogue, const StableId &id) {
  for (const auto &d : catalog.dialogues)
    if (d.id == dialogue)
      for (const auto &n : d.nodes)
        if (n.id == id)
          return &n;
  return nullptr;
}
// 本片没有岗位变更的历史来源；双方说话只认该原角色本人，不能以今天的任职倒填旧雇约。
bool staff_role_source(const CommonCatalog &catalog, const StableId &role_id,
                       const StableId &provider, const StableId &place,
                       const StableId &dialogue) {
  const auto *role = catalog.roles.find(role_id);
  return role && role->id == role_id && role->initial_actor == provider &&
         role->place_id == place && role->dialogue_id &&
         *role->dialogue_id == dialogue;
}
// 当前在岗冲突只读本人原Estate/雇用表，已终止雇约不被误判为永久失业。
Status free_worker(const PrivateShopStaffContext &x, const StableId &worker) {
  const auto &c = x.shops.common;
  const auto *actor = c.actor(worker);
  if (!actor || !actor->alive || !actor->available ||
      !actor->action_id.empty() || x.shops.activity.in_open_battle(worker))
    return common_failure("PrivateShopWorkerUnavailable");
  if (!x.estate_access)
    return common_failure("PrivateShopLaborSourceUnavailable");
  if (x.estate_access->army_id && !x.estate_access->army_id(worker).empty())
    return common_failure("PrivateShopWorkerInArmy");
  if (x.estate_employment) {
    for (const auto &row : x.estate_employment->contracts.rows())
      if (row.second.status == "active" &&
          row.second.terms.worker_actor_id == worker)
        return common_failure("PrivateShopWorkerAlreadyEmployed");
  } else {
    for (const auto &fact : c.state().facts.rows())
      if (fact.second.domain == estate_employment_module)
        return common_failure("PrivateShopEmploymentHistoryUnavailable");
  }
  for (const auto &row : x.estates.estates.rows()) {
    const auto *duty = row.second.duties.find(worker);
    if (row.second.manager_actor_id == worker || (duty && *duty != "idle"))
      return common_failure("PrivateShopWorkerEstateDuty");
  }
  for (const auto &row : x.state.contracts.rows())
    if (row.second.status == "active" &&
        row.second.terms.worker_actor_id == worker)
      return common_failure("PrivateShopWorkerAlreadyEmployed");
  return common_success();
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
} // namespace
static Status validate_staff_history(const PrivateShopStaffContext &x,
                                     bool lifecycle_pending,
                                     bool include_financial_results = true) {
  if (&x.state != &x.shops.state.staff || &x.catalog != &x.shops.catalog.staff)
    return common_failure("PrivateShopStaffCandidateMismatch");
  auto observed = private_shop_observed_tick(x.shops);
  if (!observed.ok)
    return {false, {}, observed.error};
  const auto &c = x.shops.common;
  std::set<StableId> active_shops, active_workers, obligations, decisions;
  for (const auto &row : x.catalog.policies.rows()) {
    const auto &p = row.second;
    const auto *premises = x.shops.catalog.premises.find(p.premises_id);
    const auto *rate = x.shops.economy_catalog.rates.find(p.wage_rate_profile);
    if (p.id != row.first || !premises || !c.actor(p.worker_actor_id) ||
        !rate || rate->per_capita || rate->numerator != 3 ||
        rate->denominator != 2 || p.service_ticks < 2 ||
        p.service_ticks > 720 ||
        !x.shops.common_catalog.items.find(p.service_item_id))
      return common_failure("InvalidPrivateShopStaffPolicy", row.first);
    for (const auto &source :
         {std::make_pair(p.employer_dialogue_id, p.employer_offer_node_id),
          std::make_pair(p.worker_dialogue_id, p.worker_accept_node_id)}) {
      const auto *n =
          node(x.shops.interaction_catalog, source.first, source.second);
      if (!n || n->kind != "say" || n->speaker.empty() ||
          n->speaker == "$player")
        return common_failure("PrivateShopStaffDeclarationRequired", p.id);
    }
  }
  for (const auto &row : x.state.contracts.rows()) {
    const auto &record = row.second;
    const auto &t = record.terms;
    const auto *shop = x.shops.state.shops.find(t.shop_id);
    const auto *policy = x.catalog.policies.find(t.policy_id);
    const auto *worker = c.actor(t.worker_actor_id);
    const auto *head = c.state().facts.find(record.id);
    const auto id = organization_instance_sequence(record.id,
                                                   "private_shop_staff"),
               wage_id = organization_instance_sequence(record.obligation_id,
                                                        "private_shop_wage");
    Integer through = 0;
    if (row.first != record.id || !shop || !policy ||
        policy->premises_id != shop->terms.premises_id || !worker || !head ||
        head->kind != "private_shop_staff_accepted" ||
        head->domain != private_shop_module || !id || !wage_id ||
        *id >= c.state().next_instance_id ||
        *wage_id >= c.state().next_instance_id || *wage_id != *id + 1 ||
        !obligations.insert(record.obligation_id).second ||
        t.employer_actor_id != shop->terms.proprietor_actor_id ||
        t.worker_actor_id != policy->worker_actor_id ||
        t.worker_actor_id == t.employer_actor_id ||
        t.payer_wallet_id != shop->wallet_id ||
        t.payee_wallet_id != worker->wallet_id ||
        t.rate_profile != policy->wage_rate_profile || t.wage_numerator != 3 ||
        t.wage_denominator != 2 || t.service_ticks != policy->service_ticks ||
        t.service_item_id != policy->service_item_id ||
        std::find(c.state().facts.order().begin(),
                  c.state().facts.order().end(), shop->id) >=
            std::find(c.state().facts.order().begin(),
                      c.state().facts.order().end(), record.id) ||
        record.accepted_tick < shop->opened_tick ||
        record.accepted_tick > observed.value ||
        __builtin_add_overflow(record.accepted_tick, t.service_ticks,
                               &through) ||
        record.through_tick != through)
      return common_failure("InvalidPrivateShopStaffContract", record.id);
    for (const auto &actor : {t.employer_actor_id, t.worker_actor_id}) {
      auto age = read_private_shop_party_adult_before_fact(
          x.shops, actor, record.accepted_tick, record.id);
      if (!age.ok)
        return {false, {}, age.error};
      if (!age.value.adult)
        return common_failure("PrivateShopStaffAdultRequired", record.id);
      if (x.shops.family_deaths) {
        if (x.shops.exact_operations) {
          const auto *life = x.shops.exact_operations->life4;
          if (!life || !life->bound_to(c, *x.shops.family_deaths))
            return common_failure("PrivateShopExactLifeHistoryRequired",
                                  record.id);
          auto death = life->read_actor_death_before_original_fact(
              actor, record.accepted_tick,
              {private_shop_module, "private_shop_staff_accepted", record.id});
          if (!death.ok)
            return {false, {}, death.error};
          if (death.value)
            return common_failure("PrivateShopStaffAcceptedAfterDeath",
                                  record.id);
        } else {
          auto death =
              read_actor_death_before_fact(c, *x.shops.family_deaths, actor,
                                           record.accepted_tick, record.id);
          if (!death.ok)
            return {false, {}, death.error};
          if (death.value)
            return common_failure("PrivateShopStaffAcceptedAfterDeath",
                                  record.id);
        }
      } else if (!c.actor(actor)->alive)
        return common_failure("PrivateShopDeathHistoryUnavailable", record.id);
    }
    const auto &meeting = record.agreement;
    const auto provider = record.accepted_by_worker ? t.employer_actor_id
                                                    : t.worker_actor_id,
               requester = record.accepted_by_worker ? t.worker_actor_id
                                                     : t.employer_actor_id;
    if (meeting.actor_id != requester ||
        meeting.provider_actor_id != provider || meeting.session_id.empty() ||
        meeting.place_id != shop->terms.place_id ||
        meeting.dialogue_id != (record.accepted_by_worker
                                    ? policy->employer_dialogue_id
                                    : policy->worker_dialogue_id) ||
        meeting.node_id != (record.accepted_by_worker
                                ? policy->employer_offer_node_id
                                : policy->worker_accept_node_id))
      return common_failure("PrivateShopStaffMeetingMismatch", record.id);
    if (!staff_role_source(x.shops.common_catalog, meeting.role_id, provider,
                           meeting.place_id, meeting.dialogue_id))
      return common_failure("PrivateShopStaffRoleSourceMismatch", record.id);
    const auto *n =
        node(x.shops.interaction_catalog, meeting.dialogue_id, meeting.node_id);
    if (!n || (n->speaker != "$provider" && n->speaker != provider))
      return common_failure("PrivateShopStaffSpeakerMismatch", record.id);
    const auto *obligation =
        x.shops.economy.obligations.find(record.obligation_id);
    const auto *remainder =
        x.shops.economy.obligation_remainders.find(record.obligation_id);
    if (!obligation || !remainder || *remainder < 0 || *remainder > 1 ||
        obligation->id != record.obligation_id ||
        obligation->kind != ObligationKind::wage ||
        obligation->principal.kind != SubjectKind::actor ||
        obligation->principal.id != t.employer_actor_id ||
        obligation->beneficiary.kind != SubjectKind::actor ||
        obligation->beneficiary.id != t.worker_actor_id ||
        obligation->source_wallet_or_inventory != t.payer_wallet_id ||
        obligation->destination != t.payee_wallet_id ||
        obligation->rate_profile != t.rate_profile ||
        obligation->authority_ref != record.id ||
        obligation->period_ticks != 1 ||
        obligation->next_due_tick != record.accepted_tick + 1 ||
        obligation->active || !obligation->item_id.empty())
      return common_failure("PrivateShopStaffWageMismatch", record.id);
    // 只重放已经全验的原Estate与Employment永久记录，不反调其完整validator，避免跨域递归。
    if (!x.estates.estates.empty() && !x.shops.estate_catalog)
      return common_failure("PrivateShopEstateHistoryUnavailable", record.id);
    if (x.shops.estate_catalog)
      for (const auto &entry : x.shops.estate_catalog->estates.rows()) {
        const auto &d = entry.second;
        StableId manager = d.manager_actor_id;
        StableId duty = d.initial_duties.find(t.worker_actor_id)
                            ? *d.initial_duties.find(t.worker_actor_id)
                            : StableId{"idle"};
        for (const auto &event : c.state().facts.rows()) {
          if (event.first == record.id)
            break;
          if (const auto *f = x.estates.facts.find(event.first))
            if (const auto *m = std::get_if<EstateManagementFact>(f))
              if (m->estate_id == d.id) {
                if (const auto *a = std::get_if<EstateAppoint>(&m->command)) {
                  manager = a->manager_actor_id;
                  if (manager == t.worker_actor_id)
                    duty = "idle";
                }
                if (const auto *a = std::get_if<EstateAssign>(&m->command))
                  if (a->resident_actor_id == t.worker_actor_id)
                    duty = a->duty;
              }
          if (x.estate_employment)
            for (const auto &employment :
                 x.estate_employment->contracts.rows()) {
              const auto &e = employment.second;
              if (e.terms.estate_id != d.id ||
                  e.terms.worker_actor_id != t.worker_actor_id)
                continue;
              if (e.acceptance_fact_id == event.first) {
                duty = e.terms.job == "manager" ? "idle" : e.terms.job;
                if (e.terms.job == "manager")
                  manager = t.worker_actor_id;
              }
              if (!e.acceptance_fact_id.empty() &&
                  e.terminal_fact_id == event.first) {
                duty = "idle";
                if (manager == t.worker_actor_id)
                  manager.clear();
              }
            }
        }
        if (manager == t.worker_actor_id || duty != "idle")
          return common_failure("PrivateShopHistoricalEstateDuty", record.id);
      }
    if (x.estate_employment) {
      const auto &facts = c.state().facts.order();
      const auto start = std::find(facts.begin(), facts.end(), record.id);
      for (const auto &employment : x.estate_employment->contracts.rows()) {
        const auto &e = employment.second;
        if (e.terms.worker_actor_id != t.worker_actor_id ||
            e.acceptance_fact_id.empty())
          continue;
        const auto accepted = std::find(facts.begin(), facts.end(),
                                        e.acceptance_fact_id),
                   ended = std::find(facts.begin(), facts.end(),
                                     e.terminal_fact_id);
        if (accepted < start && (e.terminal_fact_id.empty() || ended >= start))
          return common_failure("PrivateShopHistoricalEmploymentConflict",
                                record.id);
      }
    } else
      for (const auto &event : c.state().facts.rows())
        if (event.second.domain == estate_employment_module)
          return common_failure("PrivateShopEmploymentHistoryUnavailable");
    for (const auto &prior : x.state.contracts.rows())
      if (prior.first != record.id &&
          (prior.second.terms.shop_id == t.shop_id ||
           prior.second.terms.worker_actor_id == t.worker_actor_id)) {
        const auto &facts = c.state().facts.order();
        const auto a = std::find(facts.begin(), facts.end(), prior.first),
                   b = std::find(facts.begin(), facts.end(), record.id),
                   end = std::find(facts.begin(), facts.end(),
                                   prior.second.terminal_fact_id);
        if (a != facts.end() && b != facts.end() && a < b &&
            (prior.second.terminal_fact_id.empty() || end == facts.end() ||
             end >= b))
          return common_failure("PrivateShopOverlappingStaffHistory",
                                record.id);
      }
    if (record.status == "active") {
      if (!lifecycle_pending && !x.shops.phase &&
          (record.through_tick <= c.tick() ||
           !c.actor(t.employer_actor_id)->alive || !worker->alive))
        return common_failure("PrivateShopStaffTerminationRequired", record.id);
      if (!record.terminal_fact_id.empty() ||
          !active_shops.insert(shop->id).second ||
          !active_workers.insert(worker->id).second)
        return common_failure("PrivateShopStaffAlreadyAssigned", record.id);
    } else {
      const auto *ended = x.state.decisions.find(record.terminal_fact_id);
      const auto *fact = ended ? c.state().facts.find(ended->id) : nullptr;
      const auto terminal =
          ended ? organization_instance_sequence(ended->id,
                                                 "private_shop_staff_decision")
                : std::nullopt;
      if (!ended || !fact || !terminal || *terminal <= *wage_id ||
          *terminal >= c.state().next_instance_id ||
          fact->kind != "private_shop_staff_ended" ||
          fact->domain != private_shop_module ||
          !decisions.insert(ended->id).second ||
          ended->contract_id != record.id ||
          ended->tick < record.accepted_tick || ended->tick > observed.value ||
          ended->decision != record.status)
        return common_failure("PrivateShopStaffTerminationMismatch", record.id);
      if (record.status == "left" || record.status == "dismissed") {
        if (ended->actor_id != (record.status == "left"
                                    ? t.worker_actor_id
                                    : t.employer_actor_id) ||
            !ended->cause_fact_id.empty())
          return common_failure("PrivateShopStaffTerminationMismatch",
                                record.id);
      } else if (record.status == "expired") {
        if (!ended->actor_id.empty() || !ended->cause_fact_id.empty() ||
            ended->tick != record.through_tick)
          return common_failure("PrivateShopStaffExpiryMismatch", record.id);
      } else if (record.status == "worker_dead" ||
                 record.status == "employer_dead") {
        if (!ended->actor_id.empty() || !x.shops.family_deaths)
          return common_failure("PrivateShopDeathHistoryUnavailable",
                                record.id);
        const auto person = record.status == "worker_dead"
                                ? t.worker_actor_id
                                : t.employer_actor_id;
        if (x.shops.exact_operations) {
          const auto *life = x.shops.exact_operations->life4;
          if (!life || !life->bound_to(c, *x.shops.family_deaths))
            return common_failure("PrivateShopExactLifeHistoryRequired",
                                  record.id);
          auto death = life->read_actor_death_before_original_fact(
              person, ended->tick,
              {private_shop_module, "private_shop_staff_ended", ended->id});
          if (!death.ok)
            return {false, {}, death.error};
          if (!death.value ||
              death.value->death_ref.id != ended->cause_fact_id ||
              death.value->recorded_at.kind !=
                  ActorFoundationAt::Kind::exact_moment ||
              death.value->recorded_at.exact_moment.half_day_tick !=
                  ended->tick)
            return common_failure("PrivateShopStaffDeathMismatch", record.id);
        } else {
          auto death = read_actor_death_before_fact(
              c, *x.shops.family_deaths, person, ended->tick, ended->id);
          if (!death.ok)
            return {false, {}, death.error};
          if (!death.value ||
              death.value->death_fact_id != ended->cause_fact_id ||
              death.value->recorded_tick != ended->tick)
            return common_failure("PrivateShopStaffDeathMismatch", record.id);
        }
      } else
        return common_failure("UnsupportedPrivateShopStaffTermination",
                              record.id);
    }
  }
  if (decisions.size() != x.state.decisions.size())
    return common_failure("PrivateShopStaffDecisionRegistryMismatch");
  return include_financial_results
             ? validate_private_shop_staff_services(x.shops)
             : common_success();
}
Status validate_private_shop_staff(const PrivateShopStaffContext &x) {
  return validate_staff_history(x, false);
}
Outcome<const PrivateShopStaffContract *>
read_original_private_shop_staff_contract(const PrivateShopContext &x,
                                          const SourceRef &source) {
  if (source.domain != private_shop_module ||
      source.kind != "private_shop_staff_accepted" || source.id.empty() ||
      !x.estates || !x.estate_access)
    return {
        false,
        {},
        common_failure("PrivateShopOriginalStaffAcceptanceRequired", source.id)
            .error};
  auto checked =
      validate_staff_history({x, x.catalog.staff, x.state.staff, *x.estates,
                              x.estate_employment, x.estate_access},
                             true, false);
  if (!checked.ok)
    return {false, {}, checked.error};
  const auto *record = x.state.staff.contracts.find(source.id);
  if (!record || record->id != source.id)
    return {
        false,
        {},
        common_failure("PrivateShopOriginalStaffAcceptanceRequired", source.id)
            .error};
  return {true, record, {}};
}

Outcome<PrivateShopStaffQuote> preview_private_shop_staff(
    const PrivateShopStaffContext &x, const PrivateShopMeetingContext &meeting,
    const StableId &actor, const PrivateShopStaffCommand &command) {
  if (x.shops.phase || !validate_private_shop_committed_read3(x.shops).ok)
    return fail<PrivateShopStaffQuote>(
        "PrivateShopStaffRequiresCommittedClock");
  auto valid = validate_private_shops(x.shops);
  if (!valid.ok)
    return {false, {}, valid.error};
  valid = validate_private_shop_staff(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  PrivateShopStaffQuote quote{
      x.shops.common.state().identity, actor, command, {}, {}};
  if (const auto *decision = std::get_if<PrivateShopStaffDecision>(&command)) {
    const auto *contract = x.state.contracts.find(decision->contract_id);
    if (!contract || contract->status != "active" ||
        ((decision->decision != "leave" ||
          actor != contract->terms.worker_actor_id) &&
         (decision->decision != "dismiss" ||
          actor != contract->terms.employer_actor_id)))
      return fail<PrivateShopStaffQuote>("PrivateShopStaffDecisionDenied");
    const auto *person = x.shops.common.actor(actor);
    if (!person || !person->alive || !person->available ||
        !person->action_id.empty() || x.shops.activity.in_open_battle(actor))
      return fail<PrivateShopStaffQuote>("PrivateShopStaffActorUnavailable");
    for (const auto &o : x.shops.state.operations.rows())
      if (o.second.staff_contract_id == contract->id &&
          o.second.result_fact_id.empty())
        return fail<PrivateShopStaffQuote>("PrivateShopStaffActionMustFinish");
    quote.terms = contract->terms;
    return {true, std::move(quote), {}};
  }
  const bool worker = std::holds_alternative<PrivateShopStaffAccept>(command);
  const StableId shop_id =
                     worker ? std::get<PrivateShopStaffAccept>(command).shop_id
                            : std::get<PrivateShopEmploy>(command).shop_id,
                 policy_id =
                     worker
                         ? std::get<PrivateShopStaffAccept>(command).policy_id
                         : std::get<PrivateShopEmploy>(command).policy_id;
  const auto *shop = x.shops.state.shops.find(shop_id);
  const auto *policy = x.catalog.policies.find(policy_id);
  if (!shop || shop->status == "closed" || !policy ||
      policy->premises_id != shop->terms.premises_id)
    return fail<PrivateShopStaffQuote>("PrivateShopStaffPolicyMismatch");
  const StableId employer = shop->terms.proprietor_actor_id,
                 employee = policy->worker_actor_id;
  if (actor != (worker ? employee : employer) || employee == employer)
    return fail<PrivateShopStaffQuote>("PrivateShopStaffPartyRequired");
  for (const auto &row : x.state.contracts.rows())
    if (row.second.status == "active" && row.second.terms.shop_id == shop_id)
      return fail<PrivateShopStaffQuote>("PrivateShopStaffAlreadyAssigned");
  valid = free_worker(x, employee);
  if (!valid.ok)
    return {false, {}, valid.error};
  for (const auto &id : {employer, employee}) {
    const auto *person = x.shops.common.actor(id);
    if (!person || !person->alive || !person->available ||
        !person->action_id.empty() || x.shops.activity.in_open_battle(id))
      return fail<PrivateShopStaffQuote>("PrivateShopStaffActorUnavailable");
    auto age = read_private_shop_party_adult(x.shops, id);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return fail<PrivateShopStaffQuote>("PrivateShopStaffAdultRequired");
  }
  auto proof = preview_authored_provider_source(
      x.shops.common, meeting.state, x.shops.interaction_catalog,
      meeting.access, actor, worker ? employer : employee);
  if (!proof.ok)
    return {false, {}, proof.error};
  if (proof.value.meeting.context.place_id != shop->terms.place_id ||
      proof.value.meeting.dialogue_id != (worker
                                              ? policy->employer_dialogue_id
                                              : policy->worker_dialogue_id) ||
      proof.value.meeting.node_id != (worker ? policy->employer_offer_node_id
                                             : policy->worker_accept_node_id))
    return fail<PrivateShopStaffQuote>("PrivateShopStaffDeclarationRequired");
  const auto &source = proof.value.meeting;
  if (!staff_role_source(x.shops.common_catalog, source.context.role_id,
                         source.provider_actor_id, source.context.place_id,
                         source.dialogue_id))
    return fail<PrivateShopStaffQuote>("PrivateShopStaffRoleSourceMismatch");
  const auto *person = x.shops.common.actor(employee);
  const auto *wallet = x.shops.common.wallet(person->wallet_id);
  if (!wallet ||
      !owner_equal(wallet->owner, {Owner::Kind::actor, employee, {}}))
    return fail<PrivateShopStaffQuote>("PrivateShopStaffPrivatePayeeRequired");
  Integer through = 0;
  if (__builtin_add_overflow(x.shops.common.tick(), policy->service_ticks,
                             &through))
    return fail<PrivateShopStaffQuote>("PrivateShopStaffTickOverflow");
  quote.terms = {policy_id,
                 shop_id,
                 employer,
                 employee,
                 shop->wallet_id,
                 person->wallet_id,
                 policy->wage_rate_profile,
                 3,
                 2,
                 policy->service_ticks,
                 policy->service_item_id};
  quote.agreement = std::move(proof.value);
  return {true, std::move(quote), {}};
}
Outcome<PrivateShopStaffReceipt> apply_private_shop_staff_in_candidate(
    CommonStore &common, LocalEconomyState &economy,
    PrivateShopStaffState &state, const PrivateShopStaffContext &x,
    const PrivateShopMeetingContext &meeting,
    const PrivateShopStaffQuote &quote) {
  if (&common != &x.shops.common || &economy != &x.shops.economy ||
      &state != &x.state || &state != &x.shops.state.staff)
    return fail<PrivateShopStaffReceipt>("PrivateShopStaffCandidateMismatch");
  if (!identity(quote.source_identity, common.state().identity))
    return fail<PrivateShopStaffReceipt>("StalePrivateShopStaffQuote");
  auto current =
      preview_private_shop_staff(x, meeting, quote.actor_id, quote.command);
  if (!current.ok)
    return {false, {}, current.error};
  if (quote.terms.policy_id != current.value.terms.policy_id ||
      quote.terms.shop_id != current.value.terms.shop_id ||
      quote.terms.employer_actor_id != current.value.terms.employer_actor_id ||
      quote.terms.worker_actor_id != current.value.terms.worker_actor_id ||
      quote.terms.payer_wallet_id != current.value.terms.payer_wallet_id ||
      quote.terms.payee_wallet_id != current.value.terms.payee_wallet_id ||
      quote.terms.rate_profile != current.value.terms.rate_profile ||
      quote.terms.wage_numerator != current.value.terms.wage_numerator ||
      quote.terms.wage_denominator != current.value.terms.wage_denominator ||
      quote.terms.service_ticks != current.value.terms.service_ticks ||
      quote.terms.service_item_id != current.value.terms.service_item_id ||
      bool(quote.agreement) != bool(current.value.agreement))
    return fail<PrivateShopStaffReceipt>("StalePrivateShopStaffQuote");
  if (quote.agreement) {
    auto same = revalidate_authored_provider_source(
        common, meeting.state, x.shops.interaction_catalog, meeting.access,
        *quote.agreement);
    if (!same.ok)
      return {false, {}, same.error};
  }
  local_economy_detail::CandidateStateGuard<PrivateShopStaffState> guard(state);
  local_economy_detail::CandidateStateGuard<LocalEconomyState> economic_guard(
      economy);
  const auto &t = current.value.terms;
  if (const auto *decision =
          std::get_if<PrivateShopStaffDecision>(&quote.command)) {
    auto id = common.next_id("private_shop_staff_decision");
    if (!id.ok)
      return {false, {}, id.error};
    auto written = common.append_fact(
        {id.value, "private_shop_staff_ended", private_shop_module});
    if (!written.ok)
      return {false, {}, written.error};
    const auto status = decision->decision == "leave" ? "left" : "dismissed";
    state.decisions.insert(id.value, {id.value, decision->contract_id,
                                      quote.actor_id, status, common.tick()});
    auto *record = state.contracts.find(decision->contract_id);
    record->status = status;
    record->terminal_fact_id = id.value;
    auto valid = validate_private_shop_staff(x);
    if (!valid.ok)
      return {false, {}, valid.error};
    guard.commit();
    economic_guard.commit();
    return {true, {record->id, id.value, record->obligation_id}, {}};
  }
  auto id = common.next_id("private_shop_staff");
  if (!id.ok)
    return {false, {}, id.error};
  auto written = common.append_fact(
      {id.value, "private_shop_staff_accepted", private_shop_module});
  if (!written.ok)
    return {false, {}, written.error};
  auto wage = common.next_id("private_shop_wage");
  if (!wage.ok)
    return {false, {}, wage.error};
  RecurringObligation obligation;
  obligation.id = wage.value;
  obligation.principal = {SubjectKind::actor, t.employer_actor_id};
  obligation.beneficiary = {SubjectKind::actor, t.worker_actor_id};
  obligation.kind = ObligationKind::wage;
  obligation.source_wallet_or_inventory = t.payer_wallet_id;
  obligation.destination = t.payee_wallet_id;
  obligation.rate_profile = t.rate_profile;
  obligation.authority_ref = id.value;
  obligation.period_ticks = 1;
  obligation.next_due_tick = common.tick() + 1;
  obligation.active = false;
  if (!economy.obligations.insert(wage.value, obligation) ||
      !economy.obligation_remainders.insert(wage.value, 0))
    return fail<PrivateShopStaffReceipt>("PrivateShopStaffWageDuplicate");
  const auto &proof = current.value.agreement->meeting;
  PrivateShopStaffContract contract;
  contract.id = id.value;
  contract.obligation_id = wage.value;
  contract.terms = t;
  contract.accepted_tick = common.tick();
  contract.through_tick = common.tick() + t.service_ticks;
  contract.accepted_by_worker =
      std::holds_alternative<PrivateShopStaffAccept>(quote.command);
  contract.agreement = {proof.actor_id,       proof.provider_actor_id,
                        proof.session_id,     proof.dialogue_id,
                        proof.node_id,        proof.context.place_id,
                        proof.context.role_id};
  if (!state.contracts.insert(id.value, contract))
    return fail<PrivateShopStaffReceipt>("PrivateShopStaffDuplicate");
  auto valid = validate_private_shop_staff(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  economic_guard.commit();
  return {true, {id.value, id.value, wage.value}, {}};
}
Status finish_private_shop_staff_lifecycle_in_candidate(
    CommonStore &c, LocalEconomyState &economy, PrivateShopState &state,
    const PrivateShopContext &x) {
  if (&c != &x.common || &economy != &x.economy || &state != &x.state ||
      x.phase)
    return common_failure("PrivateShopStaffLifecycleCandidateMismatch");
  if (x.exact_operations)
    return common_failure("PrivateShopExactStaffLifecycleWriterRequired");
  if (state.staff.contracts.empty())
    return common_success();
  if (!x.estates || !x.estate_access)
    return common_failure("PrivateShopLaborSourceUnavailable");
  PrivateShopStaffContext staff{
      x,          x.catalog.staff,     state.staff,
      *x.estates, x.estate_employment, x.estate_access};
  auto valid = validate_staff_history(staff, true);
  if (!valid.ok)
    return valid;
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(state);
  local_economy_detail::CandidateStateGuard<LocalEconomyState> economic_guard(
      economy);
  const auto ids = state.staff.contracts.order();
  for (const auto &id : ids) {
    const auto original = *state.staff.contracts.find(id);
    if (original.status != "active")
      continue;
    StableId reason, cause;
    if (c.tick() >= original.through_tick) {
      if (c.tick() != original.through_tick)
        return common_failure("PrivateShopStaffMissedExpiry", id);
      reason = "expired";
    } else
      for (const auto &party : {std::make_pair(original.terms.worker_actor_id,
                                               StableId{"worker_dead"}),
                                std::make_pair(original.terms.employer_actor_id,
                                               StableId{"employer_dead"})}) {
        const auto *actor = c.actor(party.first);
        if (!actor)
          return common_failure("UnknownActor", party.first);
        if (!actor->alive) {
          if (!x.family_deaths)
            return common_failure("PrivateShopDeathHistoryUnavailable", id);
          auto death = read_actor_death(c, *x.family_deaths, party.first);
          if (!death.ok)
            return {false, {}, death.error};
          if (!death.value || death.value->recorded_tick != c.tick())
            return common_failure("PrivateShopStaffMissedDeath", id);
          reason = party.second;
          cause = death.value->death_fact_id;
          break;
        }
      }
    if (reason.empty())
      continue;
    const auto operations = state.operations.order();
    for (const auto &operation : operations) {
      const auto *o = state.operations.find(operation);
      if (o->staff_contract_id == id && o->result_fact_id.empty()) {
        auto stopped = interrupt_private_shop_staff_operation_in_candidate(
            c, economy, state, x, operation, reason);
        if (!stopped.ok)
          return {false, {}, stopped.error};
      }
    }
    auto fact = c.next_id("private_shop_staff_decision");
    if (!fact.ok)
      return {false, {}, fact.error};
    valid = c.append_fact(
        {fact.value, "private_shop_staff_ended", private_shop_module});
    if (!valid.ok)
      return valid;
    if (!state.staff.decisions.insert(
            fact.value, {fact.value, id, {}, reason, c.tick(), cause}))
      return common_failure("PrivateShopStaffDuplicateDecision");
    auto *contract = state.staff.contracts.find(id);
    contract->status = reason;
    contract->terminal_fact_id = fact.value;
  }
  valid = validate_private_shop_staff(staff);
  if (!valid.ok)
    return valid;
  guard.commit();
  economic_guard.commit();
  return common_success();
}

} // namespace hanmo::resident
