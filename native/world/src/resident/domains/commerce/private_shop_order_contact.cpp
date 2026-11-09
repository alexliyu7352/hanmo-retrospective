// 精确原Task完成引用与原开工接触逐项相等；不读今日会话，不复制历史控制人或本次完成缓存。
#include "private_shop_order_contact.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_contract.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {
namespace {
bool identity(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
         a.revision == b.revision && a.tick == b.tick;
}
bool header(const CommonStore &c, const StableId &id, const StableId &prefix,
            const StableId &kind, const StableId &domain) {
  const auto n = organization_instance_sequence(id, prefix);
  const auto *h = c.state().facts.find(id);
  return n && *n < c.state().next_instance_id && h && h->id == id &&
         h->kind == kind && h->domain == domain;
}
bool precedes(const CommonStore &c, const StableId &a, const StableId &b) {
  const auto &order = c.state().facts.order();
  const auto x = std::find(order.begin(), order.end(), a),
             y = std::find(order.begin(), order.end(), b);
  return x != order.end() && y != order.end() && x < y;
}
} // namespace
// 正向profile选择不能因缺失同候选服务降格成旧无接触史的路径。
Status validate_private_shop_order_contact_access(const PrivateShopContext &x) {
  if (x.order_contacts_required && !x.order_contacts)
    return common_failure("PrivateShopOrderContactHistoryUnavailable");
  return common_success();
}
Status validate_private_shop_order_delivery_contact(
    const PrivateShopContext &x, const PrivateShopOrderRecord &order,
    const PrivateShopOrderExecution &execution) {
  auto valid = validate_private_shop_order_contact_access(x);
  if (!valid.ok)
    return valid;
  if (!x.order_contacts_required || execution.outcome != "delivered")
    return common_success();
  return validate_private_shop_order_delivery_contact(x, order, execution,
                                                      *x.order_contacts);
}
Status validate_private_shop_order_delivery_contact(
    const PrivateShopContext &x, const PrivateShopOrderRecord &order,
    const PrivateShopOrderExecution &execution,
    const TaskShopOrderContactRead &read) {
  auto fail = [&](const StableId &code) {
    return common_failure(code, order.id);
  };
  const auto &c = x.common;
  Integer observed = c.tick();
  if (x.catalog.version == 3 || x.state.version == 3) {
    auto at = read_private_shop_observed_moment3(x);
    if (!at.ok)
      return {false, {}, at.error};
    observed = at.value.half_day_tick;
  } else if (x.phase) {
    if (c.tick() == std::numeric_limits<Integer>::max() ||
        x.phase->from_tick != c.tick() || x.phase->to_tick != c.tick() + 1)
      return fail("PrivateShopOrderContactPhaseMismatch");
    observed = x.phase->to_tick;
  }
  if (x.state.orders.orders.find(order.id) != &order ||
      order.status != "accepted" || order.task_id.empty() ||
      execution.outcome != "delivered" || execution.order_id != order.id ||
      execution.task_id != order.task_id ||
      !identity(execution.source_identity, c.state().identity) ||
      execution.completed_tick < order.terms.accepted_tick ||
      execution.completed_tick > observed)
    return fail("PrivateShopOrderContactExecutionMismatch");
  auto source = read.task_shop_order_delivery_contact(
      order.task_id, execution.receipt_fact_id);
  if (!source.ok)
    return {false, {}, source.error};
  if (!source.value)
    return fail("PrivateShopOrderContactHistoryUnavailable");
  const auto &s = *source.value;
  const auto &a = s.contact;
  const auto &d = order.client_declaration;
  if (!identity(s.source_identity, c.state().identity) ||
      s.settlement_fact_id != execution.receipt_fact_id ||
      s.settlement_id != execution.settlement_id ||
      s.completed_tick != execution.completed_tick ||
      a.task_id != order.task_id || a.order_id != order.id ||
      a.acceptance_fact_id != order.acceptance_fact_id ||
      a.order_issue_fact_id != order.id ||
      a.proprietor_actor_id != order.terms.proprietor_actor_id ||
      order.terms.original_client.kind != Owner::Kind::actor ||
      a.client_actor_id != order.terms.original_client.id ||
      d.actor_id != a.proprietor_actor_id ||
      d.provider_actor_id != a.client_actor_id ||
      a.context.provider_actor_id != d.provider_actor_id ||
      a.context.place_id != d.place_id || a.context.role_id != d.role_id ||
      a.dialogue_id != d.dialogue_id || a.node_id != d.node_id ||
      a.session_id.empty() || a.context.kind || a.context.encounter_id ||
      a.started_tick < order.terms.accepted_tick ||
      a.started_tick > s.completed_tick)
    return fail("PrivateShopOrderContactSourceMismatch");
  const auto action = organization_instance_sequence(a.action_id, "action"),
             contact = organization_instance_sequence(
                 a.fact_id, task_shop_order_contact_started_kind),
             bound = organization_instance_sequence(order.binding_fact_id,
                                                    "private_shop_order_bound"),
             settled = organization_instance_sequence(
                 execution.receipt_fact_id, "task_shop_order_settled");
  if (!action || !contact || !bound || !settled ||
      *action == std::numeric_limits<Integer>::max() || *bound >= *action ||
      *contact != *action + 1 || *contact >= *settled ||
      !header(c, a.fact_id, task_shop_order_contact_started_kind,
              task_shop_order_contact_started_kind,
              task_shop_order_contact_module) ||
      !header(c, execution.receipt_fact_id, "task_shop_order_settled",
              "task_shop_order_settled", "task_shop_orders") ||
      !precedes(c, order.binding_fact_id, a.fact_id) ||
      !precedes(c, a.fact_id, execution.receipt_fact_id))
    return fail("PrivateShopOrderContactOrderMismatch");
  if (!a.actual_result)
    return fail("PrivateShopOrderContactStartResultRequired");
  const auto result =
      organization_instance_sequence(a.actual_result->result_id, "result");
  if (!result || *result <= *contact || *result >= *settled ||
      a.actual_result->request_actor_id.empty() ||
      a.actual_result->request_sequence < 1 ||
      a.actual_result->result_revision < 1 ||
      a.actual_result->result_revision > c.state().identity.revision)
    return fail("PrivateShopOrderContactStartResultMismatch");
  return common_success();
}
} // namespace hanmo::resident
