#include "private_shop_age3.hpp"
// 订单从原客户当面声明发行，Task接受后一次绑定；两层验证避免Task与完整私店资格互相递归。
#include "daily_rent/private_shop_daily_rent_financial.hpp"
#include "private_shop_operation.hpp"
#include "private_shop_orders.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_contract.hpp"
#include "resident/domains/tasks/task_model.hpp"
#include <set>
namespace hanmo::resident {
namespace {
template <class T> Outcome<T> fail(const StableId &code) {
  return {false, {}, common_failure(code).error};
}
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool before(const CommonStore &c, const StableId &a, const StableId &b) {
  const auto &v = c.state().facts.order();
  const auto x = std::find(v.begin(), v.end(), a),
             y = std::find(v.begin(), v.end(), b);
  return x != v.end() && y != v.end() && x < y;
}
bool header(const CommonStore &c, const StableId &id, const StableId &prefix,
            const StableId &kind, const StableId &domain) {
  const auto serial = organization_instance_sequence(id, prefix);
  const auto *h = c.state().facts.find(id);
  return serial && *serial < c.state().next_instance_id && h && h->id == id &&
         h->kind == kind && h->domain == domain;
}
const DialogueNode *declaration(const InteractionCatalog &catalog,
                                const PrivateShopOrderPolicy &p) {
  for (const auto &d : catalog.dialogues)
    if (d.id == p.dialogue_id)
      for (const auto &n : d.nodes)
        if (n.id == p.node_id)
          return &n;
  return nullptr;
}
bool terms_equal(const PrivateShopOrderTerms &a,
                 const PrivateShopOrderTerms &b) {
  return a.order_id == b.order_id && a.shop_id == b.shop_id &&
         a.proprietor_actor_id == b.proprietor_actor_id &&
         owner_equal(a.original_client, b.original_client) &&
         a.task_template_id == b.task_template_id && a.item_id == b.item_id &&
         a.stock_inventory_id == b.stock_inventory_id &&
         a.shop_wallet_id == b.shop_wallet_id &&
         a.client_inventory_id == b.client_inventory_id &&
         a.client_wallet_id == b.client_wallet_id && a.quantity == b.quantity &&
         a.contract_price == b.contract_price &&
         a.accepted_tick == b.accepted_tick &&
         a.deadline_tick == b.deadline_tick;
}
// 原作者明确客户和真实岗位；私人初版不借组织客户的余额替代公共预算许可。
Status policy_valid(const PrivateShopContext &x,
                    const PrivateShopOrderPolicy &p) {
  const auto *premises = x.catalog.premises.find(p.premises_id);
  const auto *role = x.common_catalog.roles.find(p.client_role_id);
  const auto *person = x.common.actor(p.client_actor_id);
  const auto *n = declaration(x.interaction_catalog, p);
  const auto *definition =
      x.order_tasks ? x.order_tasks->templates.find(p.task_template_id)
                    : nullptr;
  if (p.id.empty() || !premises || !role ||
      role->initial_actor != p.client_actor_id || role->organization_id ||
      !role->dialogue_id || *role->dialogue_id != p.dialogue_id ||
      role->place_id != premises->place_id || !person || !n ||
      n->kind != "say" ||
      (n->speaker != "$provider" && n->speaker != person->id) || !definition ||
      definition->template_id != "procurement" ||
      !x.common_catalog.items.find(p.item_id) || p.quantity <= 0 ||
      p.quantity > 200 || p.contract_price <= 0 || p.accept_ticks < 1 ||
      p.deadline_ticks <= p.accept_ticks || p.deadline_ticks > 720)
    return common_failure("InvalidPrivateShopOrderPolicy", p.id);
  return common_success();
}
PrivateShopOrderTerms expected_terms(const PrivateShopRecord &shop,
                                     const PrivateShopOrderPolicy &p,
                                     const ActorCoreView &client,
                                     const StableId &id, Integer deadline,
                                     Integer accepted) {
  PrivateShopOrderTerms t;
  t.order_id = id;
  t.shop_id = shop.id;
  t.proprietor_actor_id = shop.terms.proprietor_actor_id;
  t.original_client = {Owner::Kind::actor, client.id, {}};
  t.task_template_id = p.task_template_id;
  t.item_id = p.item_id;
  t.stock_inventory_id = shop.inventory_id;
  t.shop_wallet_id = shop.wallet_id;
  t.client_inventory_id = client.inventory_id;
  t.client_wallet_id = client.wallet_id;
  t.quantity = p.quantity;
  t.contract_price = p.contract_price;
  t.accepted_tick = accepted;
  t.deadline_tick = deadline;
  return t;
}
// 只在当前本人办理检查生命/在场；后来的离场或死亡不能取消过去真实声明。
Status current_parties(const PrivateShopContext &x,
                       const PrivateShopRecord &shop,
                       const PrivateShopOrderPolicy &policy) {
  for (const auto &id :
       {shop.terms.proprietor_actor_id, policy.client_actor_id}) {
    const auto *actor = x.common.actor(id);
    if (!actor || !actor->alive || !actor->available ||
        !actor->action_id.empty() || x.activity.in_open_battle(id))
      return common_failure("PrivateShopOrderPartyUnavailable", id);
    auto age = read_private_shop_party_adult(x, id);
    if (!age.ok)
      return {false, {}, age.error};
    if (!age.value.adult)
      return common_failure("PrivateShopOrderAdultRequired", id);
  }
  const auto *client = x.common.actor(policy.client_actor_id);
  const Owner owner{Owner::Kind::actor, client->id, {}};
  const auto *wallet = x.common.wallet(client->wallet_id);
  const auto *inventory = x.common.inventory(client->inventory_id);
  if (!wallet || !inventory || !owner_equal(wallet->owner, owner) ||
      !owner_equal(inventory->owner, owner))
    return common_failure("PrivateShopOrderPrivateClientRequired");
  return common_success();
}
Outcome<AuthoredProviderSourceProof> current_declaration(
    const PrivateShopContext &x, const PrivateShopMeetingContext &meeting,
    const PrivateShopRecord &shop, const PrivateShopOrderPolicy &p) {
  auto valid = current_parties(x, shop, p);
  if (!valid.ok)
    return {false, {}, valid.error};
  auto proof = preview_authored_provider_source(
      x.common, meeting.state, x.interaction_catalog, meeting.access,
      shop.terms.proprietor_actor_id, p.client_actor_id);
  if (!proof.ok)
    return proof;
  const auto &m = proof.value.meeting;
  if (m.dialogue_id != p.dialogue_id || m.node_id != p.node_id ||
      m.context.role_id != p.client_role_id ||
      m.context.place_id != shop.terms.place_id)
    return fail<AuthoredProviderSourceProof>(
        "PrivateShopOrderClientDeclarationRequired");
  return proof;
}
} // namespace
Status validate_private_shop_order_ledger(const PrivateShopContext &x) {
  auto observed = private_shop_observed_tick(x);
  if (!observed.ok)
    return {false, {}, observed.error};
  for (const auto &row : x.catalog.orders.policies.rows()) {
    if (row.first != row.second.id)
      return common_failure("InvalidPrivateShopOrderPolicy", row.first);
    auto valid = policy_valid(x, row.second);
    if (!valid.ok)
      return valid;
  }
  std::set<StableId> bindings, tasks, acceptances;
  for (const auto &row : x.state.orders.orders.rows()) {
    const auto &r = row.second;
    const auto *shop = x.state.shops.find(r.terms.shop_id);
    const auto *p = x.catalog.orders.policies.find(r.policy_id);
    const auto *client = p ? x.common.actor(p->client_actor_id) : nullptr;
    Integer through = 0, deadline = 0;
    if (row.first != r.id || !shop || !p || !client ||
        p->premises_id != shop->terms.premises_id ||
        client->id == shop->terms.proprietor_actor_id ||
        !header(x.common, r.id, "private_shop_order",
                "private_shop_order_issued", private_shop_module) ||
        !before(x.common, shop->id, r.id) ||
        r.issued_tick < shop->opened_tick || r.issued_tick > observed.value ||
        __builtin_add_overflow(r.issued_tick, p->accept_ticks, &through) ||
        __builtin_add_overflow(r.issued_tick, p->deadline_ticks, &deadline) ||
        r.valid_through_tick != through ||
        !terms_equal(r.terms, expected_terms(*shop, *p, *client, r.id, deadline,
                                             r.terms.accepted_tick)))
      return common_failure("InvalidPrivateShopOrder", r.id);
    const auto &m = r.client_declaration;
    if (m.actor_id != shop->terms.proprietor_actor_id ||
        m.provider_actor_id != client->id || m.session_id.empty() ||
        m.dialogue_id != p->dialogue_id || m.node_id != p->node_id ||
        m.role_id != p->client_role_id || m.place_id != shop->terms.place_id)
      return common_failure("PrivateShopOrderDeclarationMismatch", r.id);
    for (const auto &actor : {shop->terms.proprietor_actor_id, client->id}) {
      auto age = read_private_shop_party_adult_before_fact(x, actor,
                                                           r.issued_tick, r.id);
      if (!age.ok)
        return {false, {}, age.error};
      if (!age.value.adult)
        return common_failure("PrivateShopOrderAdultRequired", r.id);
    }
    if (r.status == "pending_order") {
      if (r.terms.accepted_tick != -1 || !r.task_id.empty() ||
          !r.acceptance_fact_id.empty() || !r.binding_fact_id.empty())
        return common_failure("PrivateShopPendingOrderBindingMismatch", r.id);
    } else if (r.status == "accepted") {
      const auto *b = x.state.orders.bindings.find(r.binding_fact_id);
      if (!b || b->id != r.binding_fact_id || b->order_id != r.id ||
          b->task_id != r.task_id ||
          b->acceptance_fact_id != r.acceptance_fact_id ||
          b->tick != r.terms.accepted_tick || b->tick < r.issued_tick ||
          b->tick > r.valid_through_tick || b->tick > observed.value ||
          (!organization_instance_sequence(b->task_id, "task") ||
           *organization_instance_sequence(b->task_id, "task") >=
               x.common.state().next_instance_id) ||
          !bindings.insert(b->id).second || !tasks.insert(b->task_id).second ||
          !acceptances.insert(b->acceptance_fact_id).second ||
          !header(x.common, b->id, "private_shop_order_bound",
                  "private_shop_order_bound", private_shop_module) ||
          !header(x.common, b->acceptance_fact_id,
                  task_shop_order_acceptance_prefix,
                  task_shop_order_acceptance_kind, task_shop_order_domain) ||
          !before(x.common, r.id, b->acceptance_fact_id) ||
          !before(x.common, b->acceptance_fact_id, b->id))
        return common_failure("PrivateShopOrderBindingMismatch", r.id);
    } else
      return common_failure("UnsupportedPrivateShopOrderStatus", r.id);
  }
  if (bindings.size() != x.state.orders.bindings.size())
    return common_failure("PrivateShopOrderBindingRegistryMismatch");
  return validate_private_shop_order_settlements(x, false);
}
Status validate_private_shop_orders(const PrivateShopContext &x) {
  auto valid = validate_private_shop_order_ledger(x);
  if (!valid.ok)
    return valid;
  for (const auto &row : x.state.orders.orders.rows())
    if (row.second.status == "accepted") {
      if (!x.order_acceptance)
        return common_failure("PrivateShopOrderAcceptanceHistoryUnavailable",
                              row.first);
      auto source = x.order_acceptance->task_shop_order_acceptance(row.first);
      if (!source.ok)
        return {false, {}, source.error};
      const auto &r = row.second;
      if (!source.value ||
          !identity(source.value->source_identity, x.common.state().identity) ||
          source.value->order_issue_fact_id != r.id ||
          source.value->task_id != r.task_id ||
          source.value->acceptance_fact_id != r.acceptance_fact_id ||
          !terms_equal(source.value->terms, r.terms))
        return common_failure("PrivateShopOrderAcceptanceMismatch", r.id);
    }
  if (!x.state.orders.bindings.empty() && !x.order_execution)
    return common_failure("PrivateShopOrderExecutionHistoryUnavailable");
  for (const auto &row : x.state.orders.orders.rows())
    if (row.second.status == "accepted") {
      auto source =
          x.order_execution->private_shop_order_execution(row.second.task_id);
      if (!source.ok)
        return {false, {}, source.error};
      bool settled = false;
      for (const auto &record : x.state.order_settlements.rows())
        settled |= record.second.order_id == row.first;
      if (bool(source.value) != settled)
        return common_failure("PrivateShopOrderSettlementRequired", row.first);
    }
  return validate_private_shop_order_settlements(x, true);
}
Outcome<PrivateShopOrderQuote> preview_private_shop_order(
    const PrivateShopContext &x, const PrivateShopMeetingContext &meeting,
    const StableId &actor, const PrivateShopRequestOrder &command) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return fail<PrivateShopOrderQuote>(
        "PrivateShopOrderRequiresCommittedClock");
  auto valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *shop = x.state.shops.find(command.shop_id);
  const auto *p = x.catalog.orders.policies.find(command.policy_id);
  if (!shop || shop->terms.proprietor_actor_id != actor)
    return fail<PrivateShopOrderQuote>("PrivateShopProprietorRequired");
  if (shop->status != "open")
    return fail<PrivateShopOrderQuote>("PrivateShopOrdersSuspended");
  auto rent = private_shop_rent_use_current(x, shop->id);
  if (!rent.ok)
    return {false, {}, rent.error};
  if (!rent.value)
    return fail<PrivateShopOrderQuote>("PrivateShopOrdersSuspended");
  if (!p || p->premises_id != shop->terms.premises_id ||
      p->client_actor_id == actor)
    return fail<PrivateShopOrderQuote>("PrivateShopOrderPolicyMismatch");
  for (const auto &row : x.state.orders.orders.rows())
    if (row.second.terms.shop_id == shop->id && row.second.policy_id == p->id) {
      bool settled = false;
      for (const auto &done : x.state.order_settlements.rows())
        settled |= done.second.order_id == row.first;
      if ((row.second.status == "accepted" && !settled) ||
          (row.second.status == "pending_order" &&
           x.common.tick() <= row.second.valid_through_tick))
        return fail<PrivateShopOrderQuote>(
            "PrivateShopOrderAlreadyOutstanding");
    }
  auto funds = x.common.spendable(shop->wallet_id);
  if (!funds.ok)
    return {false, {}, funds.error};
  Integer daily = x.daily_rent ? 0 : shop->terms.daily_rent;
  for (const auto &staff : x.state.staff.contracts.rows())
    if (staff.second.terms.shop_id == shop->id &&
        staff.second.status == "active")
      daily += 3;
  if (funds.value < daily)
    return fail<PrivateShopOrderQuote>("PrivateShopOrdersSuspended");
  auto proof = current_declaration(x, meeting, *shop, *p);
  if (!proof.ok)
    return {false, {}, proof.error};
  Integer through = 0, deadline = 0;
  if (__builtin_add_overflow(x.common.tick(), p->accept_ticks, &through) ||
      __builtin_add_overflow(x.common.tick(), p->deadline_ticks, &deadline))
    return fail<PrivateShopOrderQuote>("PrivateShopOrderTickOverflow");
  PrivateShopOrderQuote out;
  out.source_identity = x.common.state().identity;
  out.actor_id = actor;
  out.command = command;
  out.terms = expected_terms(*shop, *p, *x.common.actor(p->client_actor_id), {},
                             deadline, -1);
  out.valid_through_tick = through;
  out.agreement = std::move(proof.value);
  return {true, std::move(out), {}};
}
Outcome<StableId>
issue_private_shop_order_in_candidate(CommonStore &c, PrivateShopState &state,
                                      const PrivateShopContext &x,
                                      const PrivateShopMeetingContext &meeting,
                                      const PrivateShopOrderQuote &quote) {
  if (&c != &x.common || &state != &x.state)
    return fail<StableId>("PrivateShopCandidateMismatch");
  if (!identity(quote.source_identity, c.state().identity))
    return fail<StableId>("StalePrivateShopOrderQuote");
  auto current =
      preview_private_shop_order(x, meeting, quote.actor_id, quote.command);
  if (!current.ok)
    return {false, {}, current.error};
  if (!terms_equal(current.value.terms, quote.terms) ||
      current.value.valid_through_tick != quote.valid_through_tick)
    return fail<StableId>("StalePrivateShopOrderQuote");
  auto valid = revalidate_authored_provider_source(
      c, meeting.state, x.interaction_catalog, meeting.access, quote.agreement);
  if (!valid.ok)
    return {false, {}, valid.error};
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(state);
  auto id = c.next_id("private_shop_order");
  if (!id.ok)
    return id;
  valid = c.append_fact(
      {id.value, "private_shop_order_issued", private_shop_module});
  if (!valid.ok)
    return {false, {}, valid.error};
  PrivateShopOrderRecord r;
  r.id = id.value;
  r.policy_id = quote.command.policy_id;
  r.terms = current.value.terms;
  r.terms.order_id = id.value;
  r.issued_tick = c.tick();
  r.valid_through_tick = quote.valid_through_tick;
  const auto &m = current.value.agreement.meeting;
  r.client_declaration = {
      m.actor_id, m.provider_actor_id, m.session_id,     m.dialogue_id,
      m.node_id,  m.context.place_id,  m.context.role_id};
  if (!state.orders.orders.insert(r.id, r))
    return fail<StableId>("PrivateShopOrderDuplicate");
  valid = validate_private_shops(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  return id;
}
Status revalidate_private_shop_order_acceptance_meeting(
    const PrivateShopContext &x, const PrivateShopMeetingContext &meeting,
    const StableId &actor, const StableId &id) {
  if (x.phase || !validate_private_shop_committed_read3(x).ok)
    return common_failure("PrivateShopOrderRequiresCommittedClock");
  auto valid = validate_private_shop_order_ledger(x);
  if (!valid.ok)
    return valid;
  const auto *r = x.state.orders.orders.find(id);
  if (!r || r->status != "pending_order" ||
      r->terms.proprietor_actor_id != actor ||
      x.common.tick() > r->valid_through_tick)
    return common_failure("PrivateShopOrderAcceptanceUnavailable", id);
  const auto *shop = x.state.shops.find(r->terms.shop_id);
  if (!shop || shop->status != "open")
    return common_failure("PrivateShopOrdersSuspended", id);
  auto rent = private_shop_rent_use_current(x, shop->id);
  if (!rent.ok)
    return {false, {}, rent.error};
  if (!rent.value)
    return common_failure("PrivateShopOrdersSuspended", id);
  auto funds = x.common.spendable(shop->wallet_id);
  if (!funds.ok)
    return {false, {}, funds.error};
  Integer daily = x.daily_rent ? 0 : shop->terms.daily_rent;
  for (const auto &staff : x.state.staff.contracts.rows())
    if (staff.second.terms.shop_id == shop->id &&
        staff.second.status == "active")
      daily += 3;
  if (funds.value < daily)
    return common_failure("PrivateShopOrdersSuspended", id);
  auto proof = current_declaration(
      x, meeting, *shop, *x.catalog.orders.policies.find(r->policy_id));
  if (!proof.ok)
    return {false, {}, proof.error};
  return common_success();
}
Outcome<StableId> bind_private_shop_order_in_candidate(
    CommonStore &c, PrivateShopState &state, const PrivateShopContext &x,
    const StableId &order_id, const StableId &task_id) {
  if (&c != &x.common || &state != &x.state || x.phase ||
      !validate_private_shop_committed_read3(x).ok)
    return fail<StableId>("PrivateShopCandidateMismatch");
  auto valid = validate_private_shop_order_ledger(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *pending = state.orders.orders.find(order_id);
  if (!pending || pending->status != "pending_order" ||
      c.tick() > pending->valid_through_tick)
    return fail<StableId>("PrivateShopOrderAcceptanceUnavailable");
  if (!x.order_acceptance)
    return fail<StableId>("PrivateShopOrderAcceptanceHistoryUnavailable");
  auto source = x.order_acceptance->task_shop_order_acceptance(order_id);
  if (!source.ok)
    return {false, {}, source.error};
  auto expected = pending->terms;
  expected.accepted_tick = c.tick();
  if (!source.value ||
      !identity(source.value->source_identity, c.state().identity) ||
      source.value->task_id != task_id ||
      source.value->order_issue_fact_id != order_id ||
      !terms_equal(source.value->terms, expected) ||
      !header(c, source.value->acceptance_fact_id,
              task_shop_order_acceptance_prefix,
              task_shop_order_acceptance_kind, task_shop_order_domain) ||
      !before(c, order_id, source.value->acceptance_fact_id))
    return fail<StableId>("PrivateShopOrderAcceptanceMismatch");
  local_economy_detail::CandidateStateGuard<PrivateShopState> guard(state);
  auto id = c.next_id("private_shop_order_bound");
  if (!id.ok)
    return id;
  valid = c.append_fact(
      {id.value, "private_shop_order_bound", private_shop_module});
  if (!valid.ok)
    return {false, {}, valid.error};
  auto *r = state.orders.orders.find(order_id);
  r->status = "accepted";
  r->terms = expected;
  r->task_id = task_id;
  r->acceptance_fact_id = source.value->acceptance_fact_id;
  r->binding_fact_id = id.value;
  if (!state.orders.bindings.insert(
          id.value,
          {id.value, order_id, task_id, r->acceptance_fact_id, c.tick()}))
    return fail<StableId>("PrivateShopOrderDuplicateBinding");
  valid = validate_private_shop_orders(x);
  if (!valid.ok)
    return {false, {}, valid.error};
  guard.commit();
  return id;
}
Outcome<std::optional<PrivateShopOrderSource>>
PrivateShopOrderAccess::private_shop_order_source(const StableId &id) const {
  auto valid = validate_private_shop_order_ledger(context_);
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *r = context_.state.orders.orders.find(id);
  if (!r)
    return {true, {}, {}};
  PrivateShopOrderSource out;
  out.source_identity = context_.common.state().identity;
  out.terms = r->terms;
  out.policy_id = r->policy_id;
  out.issue_fact_id = r->id;
  out.task_id = r->task_id;
  out.acceptance_fact_id = r->acceptance_fact_id;
  out.status = r->status;
  out.client_declaration = r->client_declaration;
  out.issued_tick = r->issued_tick;
  out.valid_through_tick = r->valid_through_tick;
  for (const auto &settled : context_.state.order_settlements.rows())
    if (settled.second.order_id == id) {
      out.cost_fact_id = settled.first;
      if (settled.second.outcome == "delivered")
        out.goods_cost = settled.second.cost_of_goods;
    }
  return {true, std::move(out), {}};
}
} // namespace hanmo::resident
