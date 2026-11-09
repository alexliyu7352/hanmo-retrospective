#include "director_actor_read.hpp"
#include "resident/domains/affairs/issued_supply/affairs_issued_supply.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_attendance_world.hpp"
#include "resident/domains/interaction/visit_messages/visit_reception_director.hpp"
#include "resident/domains/social/social_time2.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_sources.hpp"
#include "resident/domains/tasks/task_authored_continuation.hpp"
#include "resident/domains/tasks/task_responsibility_director.hpp"
// 有限NPC计划只用本人的占用/责任与可知报告；全部实际动作仍经过中央命令与原领域规则。
#include "director_livelihood.hpp"
#include "director_membership.hpp"
#include "director_model.hpp"
#include "resident/domains/affairs/affairs_model.hpp"
#include "resident/domains/growth/growth_internal.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_delegation_director.hpp"
#include "resident/domains/politics/diplomacy/diplomacy_response_director.hpp"
#include "resident/domains/politics/politics_director.hpp"
#include "resident/domains/politics/treaty/treaty_passage_director.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/kernel/command_types.hpp"
#include <limits>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
// 保留来源插入序而去重，不把知识投影变成另一份authority。
void append_unique(std::vector<StableId> &values, const StableId &id) {
  if (!id.empty() &&
      std::find(values.begin(), values.end(), id) == values.end())
    values.push_back(id);
}
bool enabled(const DirectorPolicy &p, DirectorTemplate t) {
  return std::find(p.templates.begin(), p.templates.end(), t) !=
         p.templates.end();
}
bool local_site(const KnownSubjectView &v, const StableId &site) {
  return !site.empty() && (v.place_id == site || v.settlement_id == site);
}
const KnownReport *actor_report(const DynamicSocialState &s,
                                const StableId &actor, const StableId &report) {
  auto *table = s.known_reports.find("0:" + actor);
  return table ? table->find(report) : nullptr;
}
bool supply_is_current(const KnownSupply &s, Integer tick) {
  return s.observation_tick <= tick &&
         (!s.valid_until_tick || tick <= *s.valid_until_tick);
}
// 转换为具体命令，不存JSON、不提前操作钱包或行动头。
template <class T>
std::shared_ptr<const CommandPayload> command(const StableId &kind, T value) {
  return std::make_shared<const CommandPayload>(
      CommandPayload{kind, std::move(value)});
}
Outcome<DecisionProposal> base_proposal(const CommonStore &c,
                                        const DirectorState &s,
                                        const DirectorCatalog &catalog,
                                        const KnownSubjectView &view,
                                        Integer tick) {
  auto *schedule = director_actor_schedule(s, view.actor_id);
  if (!schedule || !director_policy(s, catalog, view.actor_id) ||
      !director_subject_active(s, view.actor_id) ||
      view.subject.kind != SubjectKind::actor ||
      view.subject.id != view.actor_id || view.tick != tick ||
      view.revision != c.revision() || tick != c.tick())
    return {false, {}, common_failure("InvalidDirectorInput").error};
  if (schedule->decision_seq == std::numeric_limits<Integer>::max() ||
      tick > std::numeric_limits<Integer>::max() -
                 std::max(catalog.retry_delay_ticks,
                          catalog.review_interval_ticks))
    return {false, {}, common_failure("DirectorSequenceOverflow").error};
  DecisionProposal p;
  p.subject_id = view.actor_id;
  auto transition = std::make_shared<DirectorTransition>();
  transition->expected_revision = c.revision();
  if (const auto registration = director_actor_registration(s, view.actor_id))
    transition->registration_epoch = registration->epoch;
  transition->decision_seq = schedule->decision_seq + 1;
  transition->next_review_tick =
      tick + std::max(catalog.retry_delay_ticks, catalog.review_interval_ticks);
  transition->template_id = "await_adjustment";
  transition->phase = PlanPhase::waiting;
  if (!view.current_action_id.empty())
    transition->current_action_id = view.current_action_id;
  auto current_plan = read_director_actor_plan(s, view.actor_id);
  if (!current_plan.ok)
    return {false, {}, current_plan.error};
  if (const auto *plan = current_plan.value) {
    p.plan_id = plan->id;
    transition->template_id = plan->template_id;
    transition->goal_ref = plan->goal_ref;
    transition->next_step_index = plan->step_index;
    transition->commitment_ids = plan->commitment_ids;
    transition->livelihood = plan->livelihood;
    transition->pending_life_action_id = plan->pending_life_action_id;
    transition->last_life_fact_id = plan->last_life_fact_id;
  } else
    p.plan_id = "plan:" + view.actor_id + ":" +
                std::to_string(transition->decision_seq);
  p.transition = transition;
  p.expected_versions.push_back(
      c.read_version({ResourceKey::Kind::actor, view.actor_id}));
  return {true, std::move(p), {}};
}
// 每个提案独有transition，在函数内部填写后对外仅提供const借用。
DirectorTransition &edit_transition(DecisionProposal &p) {
  return const_cast<DirectorTransition &>(*p.transition);
}
void use_report(DecisionProposal &p, const StableId &report,
                const StableId &fact) {
  append_unique(edit_transition(p).known_report_ids, report);
  p.reason_fact_id = fact;
}
// 任职者必须在同一可见地点；失效或远离的角色只给普通不可用结果。
std::optional<InteractionContext> role_context(const CommonStore &c,
                                               const CommonCatalog &catalog,
                                               const KnownSubjectView &view,
                                               const StableId &role_id) {
  auto *definition = catalog.roles.find(role_id);
  auto *role = c.state().roles.find(role_id);
  if (!definition || !role || !role->enabled ||
      definition->place_id != view.place_id)
    return {};
  auto location = c.resolved_location(role->actor_id);
  if (!location.ok)
    return {};
  auto *place = std::get_if<SettlementLocation>(&location.value);
  if (!place || place->place_id != view.place_id)
    return {};
  return InteractionContext{
      view.place_id, role->actor_id, role_id, {}, {}, {}, {}};
}
// 待付款本身不占人身职责；实际受托货物未归还时仍优先履行保管责任。
bool has_task_cargo(const CommonStore &c, const StableId &actor,
                    const StableId &task) {
  auto *person = c.actor(actor);
  auto *bag = person ? c.inventory(person->inventory_id) : nullptr;
  if (!bag)
    return true;
  return std::any_of(bag->lots.begin(), bag->lots.end(),
                     [&](const LotView &lot) {
                       return lot.owner.kind == Owner::Kind::task &&
                              lot.owner.id == task && lot.quantity > 0;
                     });
}
bool active_task(const TaskRecord &task) {
  return task.status == "active" || task.status == "accepted" ||
         task.status == "in_progress" || task.status == "ready" ||
         task.status == "settling";
}
} // namespace
static KnownSubjectView known_subject_view_original(
    const CommonStore &c, const SocialState &social, const StableId &actor,
    const InteractionState *interaction, bool actual_knowledge_only) {
  KnownSubjectView view;
  view.subject = {SubjectKind::actor, actor};
  view.actor_id = actor;
  view.tick = c.tick();
  view.revision = c.revision();
  auto *person = c.actor(actor);
  if (!person)
    return view;
  view.alive = person->alive;
  view.available = person->available;
  view.current_action_id = person->action_id;
  auto location = c.resolved_location(actor);
  if (location.ok)
    if (auto *p = std::get_if<SettlementLocation>(&location.value)) {
      view.place_id = p->place_id;
      view.settlement_id = p->settlement_id;
    }
  if (interaction)
    for (const auto &session : interaction->interactions) {
      if (session.actor_id == actor ||
          session.context.provider_actor_id == actor)
        view.interaction_busy = true;
      for (const auto &person_id :
           {session.actor_id, session.context.provider_actor_id}) {
        auto at = c.resolved_location(person_id);
        auto *place =
            at.ok ? std::get_if<SettlementLocation>(&at.value) : nullptr;
        if (place && place->place_id == view.place_id)
          append_unique(view.locally_occupied_actor_ids, person_id);
      }
    }
  if (auto *memories = social.memories.find(actor))
    for (const auto &row : memories->rows())
      if (row.second.learned_tick <= c.tick())
        view.legacy_fact_ids.push_back(row.first);
  if (interaction)
    for (const auto &knowledge : interaction->knowledge)
      if (knowledge.actor_id == actor)
        for (const auto &offer : knowledge.offers)
          if (offer.discovered_tick <= c.tick())
            append_unique(view.known_offer_ids, offer.offer_id);
  if (!social.dynamic)
    return view;
  std::map<StableId, KnownSupply> supplies;
  std::map<StableId, KnownArrear> arrears;
  for (const auto &row : social.dynamic->reports.rows()) {
    const auto &r = row.second;
    const auto *learned = actor_report(*social.dynamic, actor, row.first);
    const bool local = !actual_knowledge_only &&
                       r.visibility_scope == ReportVisibility::local_public &&
                       r.observation.supply &&
                       local_site(view, r.observation.supply->site_id);
    if ((!learned && !local) || r.observation_tick > c.tick())
      continue;
    const Integer learned_tick = learned ? learned->learned_tick : c.tick();
    if (learned_tick > c.tick())
      continue;
    append_unique(view.known_report_ids, r.id);
    if (r.observation.supply) {
      const auto &s = *r.observation.supply;
      KnownSupply value{r.id,
                        r.fact_id,
                        s.organization_id,
                        s.site_id,
                        s.inventory_id,
                        s.item_id,
                        s.source_key,
                        s.physical_amount,
                        s.target_amount,
                        s.source_revision,
                        r.observation_tick,
                        learned_tick,
                        r.valid_until_tick};
      auto found = supplies.find(s.source_key);
      if (found == supplies.end() ||
          found->second.observation_tick < value.observation_tick ||
          (found->second.observation_tick == value.observation_tick &&
           found->second.source_revision <= value.source_revision))
        supplies[s.source_key] = std::move(value);
      append_unique(view.known_source_keys, s.source_key);
    }
    if (r.observation.obligation) {
      const auto &o = *r.observation.obligation;
      bool participant =
          std::any_of(r.observation.participants.begin(),
                      r.observation.participants.end(), [&](const auto &p) {
                        return p.subject.kind == SubjectKind::actor &&
                               p.subject.id == actor;
                      });
      if (!participant && o.beneficiary_id != actor)
        continue;
      KnownArrear value{r.id,        r.fact_id,
                        o.arrear_id, o.principal_id,
                        actor,       o.outstanding_amount,
                        o.due_tick,  r.observation_tick,
                        learned_tick};
      auto found = arrears.find(o.arrear_id);
      if (found == arrears.end() ||
          found->second.observation_tick <= value.observation_tick)
        arrears[o.arrear_id] = std::move(value);
    }
  }
  for (auto &row : supplies)
    view.supplies.push_back(std::move(row.second));
  for (auto &row : arrears)
    view.arrears.push_back(std::move(row.second));
  return view;
}
KnownSubjectView known_subject_view(const CommonStore &c, const SocialState &s,
                                    const OrganizationRead &,
                                    const StableId &actor,
                                    const InteractionState *i) {
  return known_subject_view_original(c, s, actor, i, false);
}
Outcome<KnownSubjectView> read_known_director_actor2(
    const CommonStore &c, const SocialState &s, const StableId &actor,
    const InteractionState &i, const ExactResultTimeRead &exact,
    const ExactResultReadStamp &stamp, const CurrentMomentRead &clock) {
  auto now = clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto boundary = compare_world_moments(now.value, WorldMoment{c.tick(), {}});
  if (!boundary.ok)
    return {false, {}, boundary.error};
  if (boundary.value)
    return {
        false,
        {},
        common_failure("DirectorPublishedReviewBoundaryRequired", actor).error};
  if (!c.actor(actor) || !s.dynamic || !s.dynamic->time2)
    return {false,
            {},
            common_failure("DirectorActorKnowledgeOwnerRequired", actor).error};
  auto location = c.resolved_location(actor);
  if (!location.ok)
    return {false, {}, location.error};
  auto view = known_subject_view_original(c, s, actor, &i, true);
  for (const auto &id : view.known_report_ids) {
    const auto *r = s.dynamic->reports.find(id);
    if (!r)
      return {
          false, {}, common_failure("DirectorKnownReportMissing", id).error};
    auto receipt = read_knowledge_receipt_view_v2(
        c, s, exact, {SubjectKind::actor, actor}, id, stamp);
    if (!receipt.ok)
      return {false, {}, receipt.error};
    if (!receipt.value)
      return {false,
              {},
              common_failure("DirectorKnowledgeReceiptRequired", id).error};
    auto before = compare_world_moments(receipt.value->learned_at, now.value);
    if (!before.ok)
      return {false, {}, before.error};
    if (before.value > 0)
      return {
          false, {}, common_failure("DirectorKnowledgeFromFuture", id).error};
    auto valid = validate_exact_report_source(c, *r, exact, stamp);
    if (!valid.ok)
      return {false, {}, valid.error};
  }
  return {true, std::move(view), {}};
}
// 新选择政策仅排除已选玩家的自主所有权；旧政策仍拒绝把玩家登记为NPC主体。
Outcome<DirectorState> make_initial_director(const CommonStore &c,
                                             const DirectorCatalog &catalog) {
  DirectorState s;
  s.regional_life = catalog.regional_life;
  s.named_membership = catalog.named_membership;
  s.dynamic_subjects = catalog.dynamic_subjects;
  if (catalog.subjects2) {
    if (!catalog.dynamic_subjects)
      return {
          false, {}, common_failure("DirectorSubjectsProfileRequired").error};
    s.subject2.emplace();
    s.subject2->command_binding_version =
        catalog.command_acceptance_bindings ? 1 : 0;
    return {true, std::move(s), {}};
  }
  if (catalog.dynamic_subjects) {
    auto valid = validate_director_registry(c, s, catalog);
    return {valid.ok, std::move(s), valid.error};
  }
  for (const auto &row : catalog.subjects.rows()) {
    if (!c.actor(row.first))
      return {
          false, {}, common_failure("InvalidDirectorSubject", row.first).error};
    if (row.first == c.state().metadata.player_actor_id) {
      if (catalog.exclude_selected_player)
        continue;
      return {
          false, {}, common_failure("InvalidDirectorSubject", row.first).error};
    }
    s.schedules.insert(
        row.first,
        {{SubjectKind::actor, row.first}, c.tick(), 0, c.tick(), -1, {}, {}});
  }
  auto valid = validate_director(c, s, catalog);
  if (!valid.ok)
    return {false, {}, valid.error};
  return {true, std::move(s), {}};
}
Outcome<std::vector<StableId>>
director_due_subjects(const DirectorState &s, const DirectorCatalog &catalog,
                      Integer tick) {
  if (tick < 0 || s.fairness_cursor < 0 || catalog.decision_budget < 1 ||
      catalog.decision_budget > 64)
    return {false, {}, common_failure("InvalidDirectorBudget").error};
  std::vector<StableId> due;
  for (const auto &row : s.schedules.rows())
    if (director_subject_active(s, row.first) && row.second.next_tick <= tick &&
        row.second.last_decision_tick < tick)
      due.push_back(row.first);
  const auto &order = catalog.dynamic_subjects ? s.registrations.order()
                                               : catalog.subjects.order();
  const auto count = std::max<std::size_t>(1, order.size());
  auto rank = [&](const StableId &id) {
    auto pos = std::find(order.begin(), order.end(), id);
    const auto index =
        static_cast<std::size_t>(std::distance(order.begin(), pos));
    return (index + count -
            static_cast<std::size_t>(s.fairness_cursor %
                                     static_cast<Integer>(count))) %
           count;
  };
  std::stable_sort(due.begin(), due.end(),
                   [&](const StableId &a, const StableId &b) {
                     const auto *x = s.schedules.find(a);
                     const auto *y = s.schedules.find(b);
                     if (x->waiting_since != y->waiting_since)
                       return x->waiting_since < y->waiting_since;
                     if (x->next_tick != y->next_tick)
                       return x->next_tick < y->next_tick;
                     auto ra = rank(a), rb = rank(b);
                     return ra == rb ? a < b : ra < rb;
                   });
  if (due.size() > static_cast<std::size_t>(catalog.decision_budget))
    due.resize(static_cast<std::size_t>(catalog.decision_budget));
  return {true, std::move(due), {}};
}
// 调度可读取真实本人合同确定硬责任先后；计划内容仍只消费该人物的KnownSubjectView。
Outcome<std::vector<StableId>>
director_due_subjects(const CommonStore &c, const DirectorState &s,
                      const DirectorCatalog &catalog, const TaskState &tasks,
                      Integer tick) {
  if (tick != c.tick() || tick < 0 || s.fairness_cursor < 0 ||
      catalog.decision_budget < 1 || catalog.decision_budget > 64)
    return {false, {}, common_failure("InvalidDirectorBudget").error};
  struct Due {
    StableId actor;
    Integer priority = 0, deadline = std::numeric_limits<Integer>::max();
    std::size_t rank = 0;
  };
  std::vector<Due> due;
  const auto &order = catalog.dynamic_subjects ? s.registrations.order()
                                               : catalog.subjects.order();
  const auto count = std::max<std::size_t>(1, order.size());
  for (const auto &row : s.schedules.rows()) {
    const auto &schedule = row.second;
    auto *person = c.actor(row.first);
    if (!director_subject_active(s, row.first) ||
        row.first == c.state().metadata.player_actor_id || !person ||
        !person->alive || !person->available || !person->action_id.empty() ||
        schedule.next_tick > tick || schedule.last_decision_tick >= tick)
      continue;
    Due candidate;
    candidate.actor = row.first;
    auto at = std::find(order.begin(), order.end(), row.first);
    candidate.rank =
        (static_cast<std::size_t>(std::distance(order.begin(), at)) + count -
         static_cast<std::size_t>(s.fairness_cursor %
                                  static_cast<Integer>(count))) %
        count;
    for (const auto &task : tasks.tasks.rows())
      if (task.second.assignee == row.first && active_task(task.second)) {
        const bool custody = task.second.status == "settling";
        if (custody && !has_task_cargo(c, row.first, task.first))
          continue;
        const Integer priority = custody ? 2 : 1;
        if (priority > candidate.priority) {
          candidate.priority = priority;
          candidate.deadline = task.second.deadline_tick;
        } else if (priority == candidate.priority)
          candidate.deadline =
              std::min(candidate.deadline, task.second.deadline_tick);
      }
    due.push_back(std::move(candidate));
  }
  std::stable_sort(due.begin(), due.end(), [&](const Due &a, const Due &b) {
    if (a.priority != b.priority)
      return a.priority > b.priority;
    if (a.priority && a.deadline != b.deadline)
      return a.deadline < b.deadline;
    const auto *x = s.schedules.find(a.actor);
    const auto *y = s.schedules.find(b.actor);
    if (x->waiting_since != y->waiting_since)
      return x->waiting_since < y->waiting_since;
    if (a.rank != b.rank)
      return a.rank < b.rank;
    return a.actor < b.actor;
  });
  std::vector<StableId> result;
  for (std::size_t i = 0;
       i < due.size() && i < static_cast<std::size_t>(catalog.decision_budget);
       ++i)
    result.push_back(due[i].actor);
  return {true, std::move(result), {}};
}
Outcome<std::vector<DecisionProposal>>
propose_director(const CommonStore &c, const DirectorState &s,
                 const DirectorCatalog &catalog, const KnownSubjectView &view,
                 const OrganizationRead &organization, const AffairsState &,
                 Integer tick) {
  auto *schedule = director_actor_schedule(s, view.actor_id);
  if (schedule &&
      (schedule->next_tick > tick || schedule->last_decision_tick >= tick))
    return {true, {}, {}};
  auto result = base_proposal(c, s, catalog, view, tick);
  if (!result.ok)
    return {false, {}, result.error};
  auto proposal = std::move(result.value);
  auto &transition = edit_transition(proposal);
  auto *policy = director_policy(s, catalog, view.actor_id);
  if (!view.alive || !view.available || !view.current_action_id.empty())
    return {true, {std::move(proposal)}, {}};
  auto member =
      organization.member_view(view.actor_id, policy->organization_id);
  if (!catalog.named_membership && member &&
      enabled(*policy, DirectorTemplate::await_adjustment))
    for (const auto &arrear : view.arrears) {
      if (arrear.organization_id != policy->organization_id ||
          arrear.outstanding <= 0 || arrear.due_tick > tick)
        continue;
      auto age = tick - arrear.due_tick;
      std::optional<MemberCommandKind> response;
      if (age >= policy->leave_after_ticks && member->response != "departed" &&
          member->response != "awaiting_release")
        response = MemberCommandKind::lawful_leave;
      else if (age >= policy->decline_work_after_ticks &&
               member->accepts_new_duty)
        response = MemberCommandKind::decline_new_work;
      else if (age >= policy->request_arrear_after_ticks &&
               member->response == "serving")
        response = MemberCommandKind::request_payment;
      if (response) {
        proposal.command = command(
            "organization_member_response",
            OrganizationMemberCommand{view.actor_id, policy->organization_id,
                                      arrear.fact_id, *response});
        transition.template_id = "await_adjustment";
        transition.goal_ref = arrear.arrear_id;
        transition.next_step_index = 0;
        use_report(proposal, arrear.report_id, arrear.fact_id);
        return {true, {std::move(proposal)}, {}};
      }
    }
  // 无即时事务只登记下一次复评；不能以等待行动持续占住可交谈的本人。
  transition.template_id = "await_adjustment";
  transition.goal_ref.clear();
  transition.next_step_index = 0;
  transition.wait_reason = "NoImmediateAction";
  return {true, {std::move(proposal)}, {}};
}
Outcome<std::vector<DecisionProposal>> propose_director(
    const CommonStore &c, const DirectorState &s,
    const DirectorCatalog &catalog, const KnownSubjectView &view,
    const OrganizationRead &organization, const AffairsState &affairs,
    const TaskState &tasks, const TaskCatalog &task_catalog,
    const CommonCatalog &common_catalog, const GrowthCatalog &growth,
    const TravelCatalog &travel, const OrganizationState &org_state,
    const OrganizationCatalog &org_catalog, Integer tick,
    const GrowthAccess *growth_access, const GrowthLifeRead *life,
    const DirectorMembershipContext *membership,
    const DirectorPoliticalContext *politics) {
  return propose_director(c, s, catalog, view, organization, affairs, tasks,
                          task_catalog, common_catalog, growth, travel,
                          org_state, org_catalog, tick, growth_access, life,
                          membership, politics, nullptr);
}
Outcome<std::vector<DecisionProposal>> propose_director(
    const CommonStore &c, const DirectorState &s,
    const DirectorCatalog &catalog, const KnownSubjectView &view,
    const OrganizationRead &organization, const AffairsState &affairs,
    const TaskState &tasks, const TaskCatalog &task_catalog,
    const CommonCatalog &common_catalog, const GrowthCatalog &growth,
    const TravelCatalog &travel, const OrganizationState &org_state,
    const OrganizationCatalog &org_catalog, Integer tick,
    const GrowthAccess *growth_access, const GrowthLifeRead *life,
    const DirectorMembershipContext *membership,
    const DirectorPoliticalContext *politics,
    const DirectorDiplomaticContext *diplomacy,
    const EconomicSupplyPolicyRead *issued_supply,
    const TaskResponsibilityDirectorContext *task_responsibility,
    const VisitReceptionRead *visit_reception,
    const PersonalAppointmentAttendanceRead *appointment_attendance,
    const FamilyDirectorPostProposalContext5 *family_post) {
  if (family_post)
    return {false,
            {},
            common_failure("NOT_IMPLEMENTED", "director_family_autonomy",
                           ErrorKind::unsupported_capability)
                .error};
  auto *schedule = director_actor_schedule(s, view.actor_id);
  if (schedule &&
      (schedule->next_tick > tick || schedule->last_decision_tick >= tick))
    return {true, {}, {}};
  auto base = base_proposal(c, s, catalog, view, tick);
  if (!base.ok)
    return {false, {}, base.error};
  auto p = std::move(base.value);
  auto &next = edit_transition(p);
  auto *policy = director_policy(s, catalog, view.actor_id);
  if (!view.alive || !view.available || !view.current_action_id.empty())
    return {true, {std::move(p)}, {}};
  const TaskRecord *obligation = nullptr;
  for (const auto &row : tasks.tasks.rows())
    if (row.second.assignee == view.actor_id && active_task(row.second) &&
        (row.second.status != "settling" ||
         has_task_cargo(c, view.actor_id, row.first)) &&
        (!obligation || row.second.deadline_tick < obligation->deadline_tick ||
         (row.second.deadline_tick == obligation->deadline_tick &&
          row.first < obligation->id)))
      obligation = &row.second;
  // 店单只能走具名财货授权；首版没有NPC当面履约策略时明确等候，禁止假Offer或自动受领。
  if (obligation && tasks.shop_orders &&
      tasks.shop_orders->by_task.count(obligation->id)) {
    auto valid = validate_task_shop_order_ledger(c, tasks, *tasks.shop_orders);
    if (!valid.ok)
      return {false, {}, valid.error};
    next.template_id = "maintain_commitment";
    next.goal_ref = obligation->id;
    next.commitment_ids = {obligation->id};
    next.phase = PlanPhase::waiting;
    next.wait_reason = "ShopOrderContinuationNotConfigured";
    return {true, {std::move(p)}, {}};
  }
  // 先按真实已受领侧账分流；不得把authority/proposal ID伪装成普通Offer。
  if (obligation && catalog.authored_task_continuation &&
      catalog.authored_task_continuation->version == 2) {
    auto special = propose_task_responsibility_continuation(
        c, tasks, common_catalog, travel, view, obligation->id, p,
        catalog.authored_task_continuation.get(), task_responsibility);
    if (!special.ok)
      return {false, {}, special.error};
    if (special.value)
      return {true, {std::move(*special.value)}, {}};
  }
  if (view.interaction_busy) {
    if (!obligation)
      if (catalog.visit_reception &&
          enabled(*policy, DirectorTemplate::visit_reception) &&
          !(director_actor_registration(s, view.actor_id) &&
            director_actor_registration(s, view.actor_id)->obligations_only)) {
        if (!visit_reception)
          return {false,
                  {},
                  common_failure("VisitReceptionDirectorAccessMissing",
                                 view.actor_id, ErrorKind::internal)
                      .error};
        if (const auto *intent = visit_reception_actor_intent(
                *catalog.visit_reception, view.actor_id)) {
          auto visit =
              propose_visit_reception(*visit_reception, s, view, intent->id, p);
          if (!visit.ok)
            return {false, {}, visit.error};
          if (visit.value)
            return {true, {std::move(*visit.value)}, {}};
        }
      }
    const auto registration = director_actor_registration(s, view.actor_id);
    if (catalog.treaty_passage &&
        enabled(*policy, DirectorTemplate::treaty_ratification) &&
        !(registration && registration->obligations_only)) {
      if (!diplomacy || !diplomacy->treaty)
        return {false,
                {},
                common_failure("TreatyDirectorAccessMissing", view.actor_id,
                               ErrorKind::internal)
                    .error};
      auto proposed =
          propose_director_treaty_passage(*diplomacy->treaty, view, *policy, p);
      if (!proposed.ok)
        return {false, {}, proposed.error};
      if (proposed.value && proposed.value->command)
        return {true, {std::move(*proposed.value)}, {}};
    }
    if (catalog.diplomatic_response && policy->diplomatic_response &&
        enabled(*policy, DirectorTemplate::diplomatic_response) &&
        !(registration && registration->obligations_only)) {
      if (!diplomacy)
        return {false,
                {},
                common_failure("DiplomaticDirectorAccessMissing", view.actor_id,
                               ErrorKind::internal)
                    .error};
      auto reply = propose_director_diplomatic_response(
          *diplomacy, catalog, view, *policy->diplomatic_response, p);
      if (!reply.ok)
        return {false, {}, reply.error};
      if (reply.value && reply.value->command)
        return {true, {std::move(*reply.value)}, {}};
    }
    if (catalog.diplomatic_delegation &&
        policy->diplomatic_delegation_policy_id &&
        enabled(*policy, DirectorTemplate::diplomatic_delegation) &&
        !(registration && registration->obligations_only)) {
      if (!diplomacy)
        return {false,
                {},
                common_failure("DiplomaticDirectorAccessMissing", view.actor_id,
                               ErrorKind::internal)
                    .error};
      auto proposal = propose_director_diplomatic_delegation(
          *diplomacy, view, *policy->diplomatic_delegation_policy_id, p);
      if (!proposal.ok)
        return {false, {}, proposal.error};
      if (proposal.value && proposal.value->command)
        return {true, {std::move(*proposal.value)}, {}};
    }
    if (catalog.regional_life) {
      next.phase = PlanPhase::waiting;
      next.wait_reason = "ExistingInteraction";
      return {true, {std::move(p)}, {}};
    }
  }
  // 已接责任优先于新机会和欠薪离开；当前任务数据是本执行者依法可知的冻结合同。
  // 旧任务能继续时立即继续；仅真实无可执行下一步时允许零时长暂缓/欠薪请求。
  const auto waiting_with_membership =
      [&](DecisionProposal waiting) -> Outcome<std::vector<DecisionProposal>> {
    if (catalog.named_membership && !waiting.command &&
        !(director_actor_registration(s, view.actor_id) &&
          director_actor_registration(s, view.actor_id)->obligations_only)) {
      if (!membership)
        return {false,
                {},
                common_failure("MembershipDirectorAccessMissing", view.actor_id,
                               ErrorKind::internal)
                    .error};
      auto answer = propose_director_membership(
          c, s, catalog, view, membership->social, org_state, org_catalog,
          membership->economy, membership->economy_catalog,
          membership->interaction, membership->activity, membership->release,
          tick, membership->actual_clock);
      if (!answer.ok || !answer.value.empty())
        return answer;
    }
    return {true, {std::move(waiting)}, {}};
  };
  if (obligation) {
    auto *definition =
        resolve_task_definition(tasks, task_catalog, *obligation);
    if (!definition)
      return {false,
              {},
              common_failure("DirectorTaskDefinitionMissing", obligation->id,
                             ErrorKind::internal)
                  .error};
    next.template_id = "maintain_commitment";
    next.goal_ref = obligation->id;
    next.commitment_ids = {obligation->id};
    if (definition->template_id != "procurement") {
      auto continuation = propose_authored_task_continuation(
          c, tasks, task_catalog, common_catalog, travel, view, obligation->id,
          p, catalog.authored_task_continuation.get());
      if (!continuation.ok)
        return {false, {}, continuation.error};
      if (continuation.value)
        return waiting_with_membership(std::move(*continuation.value));
      return waiting_with_membership(std::move(p));
    }
    auto *person = c.actor(view.actor_id);
    auto quantity = c.count(person->inventory_id, definition->goal.item_id,
                            {{Owner::Kind::task, obligation->id, {}}});
    if (!quantity.ok)
      return {false, {}, quantity.error};
    Integer required = definition->goal.quantity;
    if (obligation->contract_id)
      if (auto *contract = tasks.contracts.find(*obligation->contract_id))
        required = std::max<Integer>(0, contract->frozen_terms.agreed_quantity -
                                            contract->delivered_quantity);
    const bool restitution = obligation->status == "settling";
    if ((restitution || required == 0) && quantity.value == 0)
      return waiting_with_membership(std::move(p));
    auto *report_role = common_catalog.roles.find(definition->report_role);
    if (!report_role)
      return {false,
              {},
              common_failure("DirectorReportRoleMissing", obligation->id,
                             ErrorKind::internal)
                  .error};
    if (quantity.value > 0 || required == 0) {
      next.phase = PlanPhase::delivering;
      next.next_step_index = 3;
      if (view.place_id != report_role->place_id)
        p.command = director_move_toward(travel, view, report_role->place_id);
      else if (auto context = role_context(c, common_catalog, view,
                                           definition->report_role)) {
        TaskCommand cmd;
        cmd.kind = "report_offer";
        cmd.offer_id = obligation->offer_id;
        cmd.offer_ref = obligation->offer_ref;
        cmd.context = *context;
        if (obligation->contract_id && !restitution && quantity.value > 0)
          cmd.quantity = std::min(quantity.value, required);
        p.command = command(cmd.kind, cmd);
        next.phase = PlanPhase::reporting;
        next.next_step_index = 4;
      } else {
        next.phase = PlanPhase::waiting;
        next.wait_reason = "ReportProviderUnavailable";
        next.current_action_id.reset();
      }
      return waiting_with_membership(std::move(p));
    }
    const KnownSupply *selected = nullptr;
    const GrowthShop *shop = nullptr;
    Integer considered = 0;
    for (const auto &supply : view.supplies) {
      if (supply.item_id != definition->goal.item_id || supply.quantity <= 0)
        continue;
      if (++considered > catalog.candidates_per_subject)
        break;
      for (const auto &candidate : growth.shops.rows())
        if (candidate.second.inventory_id == supply.inventory_id &&
            (!selected ||
             (local_site(view, supply.site_id) &&
              !local_site(view, selected->site_id)) ||
             (local_site(view, supply.site_id) ==
                  local_site(view, selected->site_id) &&
              supply.observation_tick > selected->observation_tick))) {
          selected = &supply;
          shop = &candidate.second;
        }
    }
    if (!selected || !shop)
      return waiting_with_membership(std::move(p));
    use_report(p, selected->report_id, selected->fact_id);
    next.phase = PlanPhase::sourcing;
    next.next_step_index = 1;
    if (view.place_id != shop->place_id) {
      p.command = director_move_toward(travel, view, shop->place_id);
      return waiting_with_membership(std::move(p));
    }
    if (!supply_is_current(*selected, tick))
      return waiting_with_membership(std::move(p));
    std::optional<InteractionContext> context;
    for (const auto &role : shop->representative_roles)
      if ((context = role_context(c, common_catalog, view, role)))
        break;
    if (!context)
      return waiting_with_membership(std::move(p));
    Integer amount = std::min<Integer>({required, selected->quantity, 500});
    auto weight = c.inventory_weight(person->inventory_id);
    auto *inventory = c.inventory(person->inventory_id);
    auto *item = common_catalog.items.find(definition->goal.item_id);
    if (!weight.ok || !inventory || !item)
      return {false,
              {},
              common_failure("DirectorInventoryUnavailable", view.actor_id,
                             ErrorKind::internal)
                  .error};
    if (item->weight > 0)
      amount = std::min(
          amount, std::max<Integer>(0, inventory->capacity - weight.value) /
                      item->weight);
    if (amount <= 0 || item->base_price <= 0)
      return waiting_with_membership(std::move(p));
    // 仅按已知货源报价与本人受托余额计算可买数量；实际采用仍让原交易口复核报价版本。
    auto funds = c.spendable(obligation->budget_wallet_id);
    if (!funds.ok)
      return {false, {}, funds.error};
    Integer affordable = 0, upper = amount;
    while (affordable < upper) {
      const Integer midpoint = affordable + (upper - affordable + 1) / 2;
      const Integer price = growth::trade_price(
          item->base_price, selected->quantity, midpoint, true);
      if (price <= funds.value)
        affordable = midpoint;
      else
        upper = midpoint - 1;
    }
    amount = affordable;
    if (amount <= 0)
      return waiting_with_membership(std::move(p));
    GrowthCommand purchase;
    purchase.kind = "trade";
    purchase.shop_id = shop->id;
    purchase.side = "buy";
    purchase.item_id = definition->goal.item_id;
    purchase.quantity = amount;
    purchase.quoted_total =
        growth::trade_price(item->base_price, selected->quantity, amount, true);
    purchase.market_revision = selected->source_revision;
    purchase.payer_wallet_id = obligation->budget_wallet_id;
    purchase.context = *context;
    p.command = command("trade", purchase);
    p.expected_versions.push_back(c.read_version(
        {ResourceKey::Kind::wallet, obligation->budget_wallet_id}));
    next.phase = PlanPhase::purchasing;
    next.next_step_index = 2;
    return waiting_with_membership(std::move(p));
  }
  // 原Task责任先行，赴约只消费本人原已答允来源。
  if (catalog.personal_appointment_attendance &&
      enabled(*policy, DirectorTemplate::personal_appointment_attendance)) {
    if (!appointment_attendance)
      return {false,
              {},
              common_failure("AppointmentAttendanceAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto attendance = propose_personal_appointment_attendance(
        *appointment_attendance, *catalog.personal_appointment_attendance,
        travel, view, p);
    if (!attendance.ok)
      return {false, {}, attendance.error};
    if (attendance.value)
      return {true, {std::move(*attendance.value)}, {}};
  }
  // 撤准入后只履行已冻结责任，不能用旧policy申请新职事、购买私物或承包新单。
  if (const auto registration = director_actor_registration(s, view.actor_id);
      registration && registration->obligations_only) {
    if (next.pending_life_action_id) {
      if (!life)
        return {false,
                {},
                common_failure("RegionalLifeAccessMissing", view.actor_id,
                               ErrorKind::internal)
                    .error};
      const auto result = life->life_result(*next.pending_life_action_id);
      if (!result || result->actor_id != view.actor_id ||
          result->action_id != *next.pending_life_action_id ||
          result->fact_id != *next.pending_life_action_id ||
          !c.state().facts.find(result->fact_id))
        return {false,
                {},
                common_failure("MissingLifeActionResult", view.actor_id,
                               ErrorKind::internal)
                    .error};
      if (result->phase != LifeActivityPhase::active) {
        if (result->completed_tick < result->start_tick ||
            result->completed_tick > tick)
          return {false,
                  {},
                  common_failure("InvalidLifeActionResult", view.actor_id,
                                 ErrorKind::internal)
                      .error};
        next.last_life_fact_id = result->fact_id;
        next.pending_life_action_id.reset();
        next.current_action_id.reset();
        p.reason_fact_id = result->fact_id;
        if (result->phase == LifeActivityPhase::cancelled || !result->success) {
          next.phase = PlanPhase::waiting;
          next.wait_reason =
              result->code.empty() ? "LifeActionCancelled" : result->code;
          return {true, {std::move(p)}, {}};
        }
        if (result->settlement_tick < result->start_tick ||
            result->settlement_tick > result->completed_tick)
          return {false,
                  {},
                  common_failure("UnsettledLifeActionResult", view.actor_id,
                                 ErrorKind::internal)
                      .error};
        next.phase = PlanPhase::waiting;
        next.wait_reason = "PolicyChangeAfterSettlement";
        return {true, {std::move(p)}, {}};
      }
    }
    next.wait_reason = "ExistingResponsibilityOnly";
    next.phase = PlanPhase::waiting;
    return {true, {std::move(p)}, {}};
  }
  // 新profile仅在旧任务责任已经优先处理后采用本人已知决定；不和旧自动欠薪路径竞争。
  if (catalog.named_membership) {
    if (!membership)
      return {false,
              {},
              common_failure("MembershipDirectorAccessMissing", view.actor_id,
                             ErrorKind::internal)
                  .error};
    auto answer = propose_director_membership(
        c, s, catalog, view, membership->social, org_state, org_catalog,
        membership->economy, membership->economy_catalog,
        membership->interaction, membership->activity, membership->release,
        tick, membership->actual_clock);
    if (!answer.ok || !answer.value.empty())
      return answer;
  }
  auto response =
      propose_director(c, s, catalog, view, organization, affairs, tick);
  if (!response.ok || response.value.empty())
    return response;
  if (response.value.front().command &&
      response.value.front().command->kind == "organization_member_response")
    return response;
  // 组织行政职责仅由明确作者绑定触发，既有同责任活动（含完成历史）阻止重复建单。
  if (enabled(*policy, DirectorTemplate::organize_activity))
    for (const auto &row : org_state.duties.rows()) {
      const auto &duty = row.second;
      if (!duty.active || duty.actor_id != view.actor_id ||
          !duty.activity_terms_id ||
          duty.organization_id != policy->organization_id)
        continue;
      auto *office = org_state.offices.find(duty.office_id);
      auto *org = org_state.organizations.find(duty.organization_id);
      auto *terms = org_catalog.activity_terms.find(*duty.activity_terms_id);
      if (!office || !org || !terms ||
          terms->kind != OrgActivityKind::maintain_duty ||
          office->status != OfficeStatus::active || !office->holder_actor_id ||
          *office->holder_actor_id != view.actor_id ||
          !organization.actor_accepts_duty(view.actor_id, duty.organization_id))
        continue;
      bool existing = false;
      for (const auto &entry : org_state.activities.rows()) {
        const auto &activity = entry.second;
        if (activity.responsible_office_id == office->id &&
            activity.terms_id == terms->id &&
            activity.commitment_ids == duty.commitment_ids &&
            activity.status != OrgActivityStatus::cancelled) {
          existing = true;
          break;
        }
      }
      if (existing)
        continue;
      if (!org->headquarters_site)
        continue;
      ActingAuthority authority{duty.organization_id, office->id, office->epoch,
                                view.actor_id};
      auto proof = organization.authorize(
          authority, "approve_org_activity",
          {ScopeRef::Kind::site, *org->headquarters_site}, 0);
      if (!proof.ok) {
        if (proof.error.kind != ErrorKind::rule)
          return {false, {}, proof.error};
        continue;
      }
      OrganizationActivityCommand start;
      start.authority = authority;
      start.terms_id = terms->id;
      start.participation = {{{SubjectKind::actor, view.actor_id},
                              "administrator",
                              ParticipationKind::administrative_reference,
                              duty.id}};
      start.commitment_ids = duty.commitment_ids;
      p.command = command("organization_activity", start);
      next.template_id = "organize_activity";
      next.goal_ref = duty.id;
      next.phase = PlanPhase::waiting;
      next.commitment_ids = duty.commitment_ids;
      p.expected_versions.insert(p.expected_versions.end(),
                                 proof.value.resource_versions.begin(),
                                 proof.value.resource_versions.end());
      return {true, {std::move(p)}, {}};
    }
  // 同一个既有计划预算中评估已知任用/到任，不增第二轮公平调度。
  if (enabled(*policy, DirectorTemplate::military_appointment) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.military_appointments || !politics ||
        !politics->military_appointment)
      return {false,
              {},
              common_failure("PoliticalMilitaryDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->military_appointment(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  if (enabled(*policy, DirectorTemplate::field_appointment) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.field_appointments || !politics ||
        !politics->field_appointment)
      return {false,
              {},
              common_failure("PoliticalFieldDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->field_appointment(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  if (enabled(*policy, DirectorTemplate::political_supply_renewal) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.supply_renewals || !politics || !politics->supply_renewal)
      return {false,
              {},
              common_failure("PoliticalSupplyRenewalDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->supply_renewal(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  if (enabled(*policy, DirectorTemplate::shop_registry_acceptance) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.shop_registry_intents || !politics ||
        !politics->shop_registry_acceptance)
      return {false,
              {},
              common_failure("PoliticalShopRegistryDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->shop_registry_acceptance(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  if (enabled(*policy, DirectorTemplate::governance_commission) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.governance_intents || !politics ||
        !politics->governance_commission)
      return {false,
              {},
              common_failure("GovernanceIntentDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->governance_commission(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  // 代理意图不抢原已领责任，只使用同一原公平预算。
  if (enabled(*policy, DirectorTemplate::governance_agency) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.governance_agency_intents || !politics ||
        !politics->governance_agency)
      return {false,
              {},
              common_failure("GovernanceAgencyIntentDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->governance_agency(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  // 新辞任审批不抢旧Task和已受领责任，仍用本次原公平预算。
  if (enabled(*policy, DirectorTemplate::office_resignation_response) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!catalog.office_resignation_responses || !politics ||
        !politics->office_resignation_response)
      return {false,
              {},
              common_failure("OfficeResignationResponseDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto proposed = politics->office_resignation_response(view, *policy, p);
    if (!proposed.ok)
      return {false, {}, proposed.error};
    if (proposed.value)
      return {true, {std::move(*proposed.value)}, {}};
  }
  if (catalog.visit_reception &&
      enabled(*policy, DirectorTemplate::visit_reception) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!visit_reception)
      return {false,
              {},
              common_failure("VisitReceptionDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    if (const auto *intent = visit_reception_actor_intent(
            *catalog.visit_reception, view.actor_id)) {
      auto visit =
          propose_visit_reception(*visit_reception, s, view, intent->id, p);
      if (!visit.ok)
        return {false, {}, visit.error};
      if (visit.value)
        return {true, {std::move(*visit.value)}, {}};
    }
  }
  if (catalog.subordinate_appointments &&
      enabled(*policy, DirectorTemplate::political_career) &&
      !(director_actor_registration(s, view.actor_id) &&
        director_actor_registration(s, view.actor_id)->obligations_only)) {
    if (!politics || !politics->subordinate_appointment)
      return {false,
              {},
              common_failure("PoliticalSubordinateDirectorAccessMissing",
                             view.actor_id, ErrorKind::internal)
                  .error};
    auto offered = politics->subordinate_appointment(view, *policy, p);
    if (!offered.ok)
      return {false, {}, offered.error};
    if (offered.value)
      return {true, {std::move(*offered.value)}, {}};
  }
  if (enabled(*policy, DirectorTemplate::political_career)) {
    if (!catalog.politics || !politics || !policy->political_career)
      return {false,
              {},
              common_failure("PoliticalDirectorAccessMissing", view.actor_id,
                             ErrorKind::internal)
                  .error};
    auto political = propose_director_political_career(
        c, catalog, view, org_state, org_catalog, *politics,
        *policy->political_career, p);
    if (!political.ok)
      return {false, {}, political.error};
    if (political.value)
      return {true, {std::move(*political.value)}, {}};
  }
  // 未安装的 LC01 不能退回私人机会或伪作等待；实际保留计划明确拒绝。
  auto current_life_plan = read_director_actor_plan(s, view.actor_id);
  if (!current_life_plan.ok)
    return {false, {}, current_life_plan.error};
  if (current_life_plan.value &&
      current_life_plan.value->template_id == "life_growth_care_v1")
    return {false,
            {},
            common_failure("NOT_IMPLEMENTED", current_life_plan.value->id,
                           ErrorKind::unsupported_capability)
                .error};
  // 私人备物只能在既有履约与行政责任之后评估；新可选合同不抢走此目标。
  if (catalog.regional_life &&
      enabled(*policy, DirectorTemplate::private_livelihood)) {
    if (!growth_access || !life)
      return {false,
              {},
              common_failure("RegionalLifeAccessMissing", view.actor_id,
                             ErrorKind::internal)
                  .error};
    auto local = with_livelihood_services(
        c, common_catalog, growth, *growth_access, travel, *policy, view);
    auto private_plan = propose_private_livelihood(
        c, s, catalog, local, growth, *growth_access, travel, *life, tick);
    if (!private_plan.ok || !private_plan.value.empty())
      return private_plan;
  }
  if (!enabled(*policy, DirectorTemplate::fulfill_contract) &&
      !enabled(*policy, DirectorTemplate::replenish_supply))
    return response;
  auto opportunities = query_opportunities_with_issued_supply(
      c, affairs, tasks, view, issued_supply);
  if (!opportunities.ok)
    return {false, {}, opportunities.error};
  Integer considered = 0;
  for (const auto &offer : opportunities.value) {
    if (++considered > catalog.candidates_per_subject)
      break;
    auto *definition =
        resolve_offer_definition(tasks, task_catalog, offer.offer);
    if (!definition || definition->template_id != "procurement" ||
        offer.available_amount <= 0)
      continue;
    // 个人承包无需任职；只有该实际委托组织的成员拒接状态限制其接本组织的新单。
    if (definition->issuer.kind == Owner::Kind::organization) {
      auto member =
          organization.member_view(view.actor_id, definition->issuer.id);
      if (member && !member->accepts_new_duty)
        continue;
    }
    auto context =
        role_context(c, common_catalog, view, definition->accept_role);
    if (!context)
      continue;
    TaskCommand accept;
    accept.kind = "accept_offer";
    accept.offer_id = offer.offer.id;
    accept.offer_ref = offer.offer;
    accept.context = *context;
    accept.need_revision = offer.need_revision;
    accept.quantity =
        std::min(offer.available_amount, definition->goal.quantity);
    if (*accept.quantity <= 0)
      continue;
    p.command = command("accept_offer", accept);
    next.template_id = "fulfill_contract";
    next.goal_ref = offer.offer.id;
    next.phase = PlanPhase::accepting;
    next.next_step_index = 0;
    auto *need = affairs.needs.find(offer.need_id);
    if (need)
      for (const auto &supply : view.supplies)
        if (supply.source_key == need->source_key) {
          use_report(p, supply.report_id, supply.fact_id);
          break;
        }
    return {true, {std::move(p)}, {}};
  }
  return response;
}
Status record_director_result(DirectorState &s, const DecisionProposal &p,
                              const DecisionOutcome &outcome, Integer tick) {
  if (s.subject2)
    return common_failure("DirectorSubject2AdoptionWriterRequired");
  if (outcome.kind == DecisionOutcome::Kind::internal_failure)
    return common_failure(outcome.code.empty() ? "DirectorInternalFailure"
                                               : outcome.code,
                          {}, ErrorKind::internal);
  if (s.dynamic_subjects) {
    const auto *registration = s.registrations.find(p.subject_id);
    if (!registration || !p.transition ||
        registration->lifecycle != DirectorLifecycle::active ||
        registration->epoch != p.transition->registration_epoch)
      return common_failure("StaleDirectorRegistration", p.subject_id);
  }
  auto *schedule = s.schedules.find(p.subject_id);
  if (!schedule || !p.transition || p.plan_id.empty() || tick < 0 ||
      schedule->last_decision_tick >= tick ||
      schedule->decision_seq == std::numeric_limits<Integer>::max() ||
      p.transition->decision_seq != schedule->decision_seq + 1 ||
      p.transition->next_review_tick <= tick ||
      s.fairness_cursor == std::numeric_limits<Integer>::max())
    return common_failure("StaleDirectorProposal");
  const auto &t = *p.transition;
  if (!t.wait_reason.empty() && (p.command || t.phase != PlanPhase::waiting))
    return common_failure("InvalidDirectorWaitReason");
  FinitePlan plan;
  if (auto *old = s.plans.find(p.subject_id))
    plan = *old;
  plan.id = p.plan_id;
  plan.subject = schedule->subject;
  if (s.dynamic_subjects)
    plan.policy_id = s.registrations.find(p.subject_id)->policy_id;
  plan.template_id = t.template_id;
  plan.goal_ref = t.goal_ref;
  plan.next_review_tick = t.next_review_tick;
  if (s.regional_life) {
    plan.livelihood = t.livelihood;
    plan.pending_life_action_id = t.pending_life_action_id;
    plan.last_life_fact_id = t.last_life_fact_id;
  }
  if (outcome.kind == DecisionOutcome::Kind::adopted) {
    plan.step_index = t.next_step_index;
    plan.phase = t.phase;
    plan.retry_reason = t.wait_reason;
    plan.current_action_id =
        outcome.action_id ? outcome.action_id : t.current_action_id;
    if (outcome.org_activity_id)
      plan.org_activity_id = outcome.org_activity_id;
    plan.commitment_ids = t.commitment_ids;
    if (s.regional_life && t.template_id == "private_livelihood" && p.command &&
        (p.command->kind == "service" || p.command->kind == "trade")) {
      if (!outcome.action_id)
        return common_failure("MissingLifeAction", p.subject_id,
                              ErrorKind::internal);
      plan.pending_life_action_id = outcome.action_id;
    }
    for (const auto &id : t.known_report_ids)
      append_unique(plan.known_report_ids, id);
  } else {
    plan.phase = PlanPhase::waiting;
    plan.retry_reason = outcome.code.empty() ? "Rejected" : outcome.code;
    plan.current_action_id.reset();
  }
  schedule->decision_seq = t.decision_seq;
  schedule->next_tick = t.next_review_tick;
  schedule->last_decision_tick = tick;
  schedule->last_outcome = outcome.kind == DecisionOutcome::Kind::adopted &&
                                   plan.retry_reason.empty()
                               ? "adopted"
                               : plan.retry_reason;
  schedule->wake_reasons.clear();
  schedule->waiting_since = tick;
  s.plans.set(p.subject_id, std::move(plan));
  ++s.fairness_cursor;
  s.last_boundary_tick = tick;
  return common_success();
}
} // namespace hanmo::resident
