// 无任何私铺雇约的旧profile保持原行为；实体缺读口、事实缺载荷或失去明确phase均拒绝。
#include "private_shop_staff_exclusion.hpp"
#include "private_shop.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include <set>
namespace hanmo::resident {
namespace {
Outcome<Integer> observed(const CommonStore &c, const DynamicStepInput *p) {
  if (!p)
    return {true, c.tick(), {}};
  if (c.tick() == INT64_MAX || p->from_tick != c.tick() ||
      p->to_tick != c.tick() + 1)
    return {false, 0,
            common_failure("PrivateShopEmploymentPhaseMismatch").error};
  return {true, p->to_tick, {}};
}
bool head(const CommonStore &c, const StableId &id, const StableId &prefix,
          const StableId &kind) {
  const auto *h = c.state().facts.find(id);
  const auto n = organization_instance_sequence(id, prefix);
  return h && h->id == id && h->domain == private_shop_module &&
         h->kind == kind && n && *n < c.state().next_instance_id;
}
} // namespace
Outcome<PrivateShopWorkerCommitments>
PrivateShopWorkerCommitmentAccess::private_shop_worker_commitments(
    const StableId &actor) const {
  const auto &c = common_;
  auto limit = observed(c, phase_);
  if (!limit.ok)
    return {false, {}, limit.error};
  if (!c.actor(actor) ||
      (state_.version != 1 && state_.version != 2 && state_.version != 3) ||
      state_.initialized_tick < 0 || state_.initialized_tick > c.tick() ||
      state_.last_step_tick < state_.initialized_tick ||
      state_.last_step_tick > limit.value)
    return {
        false, {}, common_failure("PrivateShopEmploymentSourceMismatch").error};
  PrivateShopWorkerCommitments out{c.state().identity, actor, limit.value, {}};
  const auto &order = c.state().facts.order();
  std::set<StableId> terminals;
  for (const auto &row : state_.staff.contracts.rows()) {
    const auto &r = row.second;
    const auto start = std::find(order.begin(), order.end(), r.id);
    if (row.first != r.id ||
        !head(c, r.id, "private_shop_staff", "private_shop_staff_accepted") ||
        start == order.end() || r.accepted_tick < state_.initialized_tick ||
        r.accepted_tick > limit.value || !c.actor(r.terms.worker_actor_id) ||
        !state_.shops.find(r.terms.shop_id))
      return {false,
              {},
              common_failure("PrivateShopEmploymentSourceMismatch", row.first)
                  .error};
    Integer ended = -1;
    if (r.status == "active") {
      if (!r.terminal_fact_id.empty())
        return {
            false,
            {},
            common_failure("PrivateShopEmploymentSourceMismatch", r.id).error};
    } else {
      if (r.status != "left" && r.status != "dismissed" &&
          r.status != "expired" && r.status != "worker_dead" &&
          r.status != "employer_dead")
        return {
            false,
            {},
            common_failure("PrivateShopEmploymentSourceMismatch", r.id).error};
      const auto *e = state_.staff.decisions.find(r.terminal_fact_id);
      const auto end =
          std::find(order.begin(), order.end(), r.terminal_fact_id);
      if (!e ||
          !organization_instance_sequence(e->id,
                                          "private_shop_staff_decision") ||
          *organization_instance_sequence(e->id,
                                          "private_shop_staff_decision") <=
              *organization_instance_sequence(r.id, "private_shop_staff") ||
          e->id != r.terminal_fact_id || e->contract_id != r.id ||
          e->decision != r.status ||
          !head(c, e->id, "private_shop_staff_decision",
                "private_shop_staff_ended") ||
          end == order.end() || end <= start ||
          !terminals.insert(e->id).second || e->tick < r.accepted_tick ||
          e->tick > limit.value)
        return {
            false,
            {},
            common_failure("PrivateShopEmploymentSourceMismatch", r.id).error};
      ended = e->tick;
    }
    if (r.terms.worker_actor_id == actor)
      out.contracts.push_back({r.id, r.terms.shop_id, actor, r.id,
                               r.terminal_fact_id, r.accepted_tick, ended});
  }
  if (terminals.size() != state_.staff.decisions.size())
    return {
        false,
        {},
        common_failure("PrivateShopEmploymentSourceRegistryMismatch").error};
  for (const auto &f : c.state().facts.rows())
    if (f.second.domain == private_shop_module &&
        ((f.second.kind == "private_shop_staff_accepted" &&
          !state_.staff.contracts.find(f.first)) ||
         (f.second.kind == "private_shop_staff_ended" &&
          !terminals.count(f.first))))
      return {
          false,
          {},
          common_failure("PrivateShopEmploymentSourceMissing", f.first).error};
  return {true, std::move(out), {}};
}
Status guard_private_shop_worker_commitment(
    const CommonStore &c, const PrivateShopWorkerCommitmentRead *read,
    const StableId &actor, const StableId &boundary, Integer tick,
    const DynamicStepInput *phase) {
  auto limit = observed(c, phase);
  if (!limit.ok)
    return {false, {}, limit.error};
  if (actor.empty() || !c.actor(actor))
    return common_failure("UnknownActor", actor);
  const auto &order = c.state().facts.order();
  const auto end = boundary.empty()
                       ? order.end()
                       : std::find(order.begin(), order.end(), boundary);
  if (!boundary.empty()) {
    const auto *h = c.state().facts.find(boundary);
    const bool estate =
        h && h->domain == "estate" &&
        (h->kind == "estate_management" || h->kind == "estate_production") &&
        organization_instance_sequence(boundary, "estate");
    const bool employment =
        h && h->domain == "estate_employment" &&
        h->kind == "estate_employment_decided" &&
        organization_instance_sequence(boundary, "estate_employment_decision");
    if (end == order.end() || tick < 0 || tick > limit.value ||
        (!estate && !employment))
      return common_failure("PrivateShopEmploymentBoundaryMismatch");
  }
  if (!read) {
    for (const auto &row : c.state().facts.rows())
      if (row.second.domain == private_shop_module &&
          row.second.kind == "private_shop_staff_accepted")
        return common_failure("PrivateShopEmploymentContextRequired");
    return common_success();
  }
  auto value = read->private_shop_worker_commitments(actor);
  if (!value.ok)
    return {false, {}, value.error};
  const auto &out = value.value;
  const auto &identity = c.state().identity;
  if (out.source_identity.world_id != identity.world_id ||
      out.source_identity.execution_epoch != identity.execution_epoch ||
      out.source_identity.revision != identity.revision ||
      out.source_identity.tick != identity.tick ||
      out.worker_actor_id != actor || out.observed_tick != limit.value)
    return common_failure("PrivateShopEmploymentSourceMismatch");
  std::set<StableId> seen;
  for (const auto &s : out.contracts) {
    const auto start =
        std::find(order.begin(), order.end(), s.acceptance_fact_id);
    if (!seen.insert(s.contract_id).second || s.worker_actor_id != actor ||
        s.contract_id != s.acceptance_fact_id ||
        !head(c, s.contract_id, "private_shop_staff",
              "private_shop_staff_accepted") ||
        start == order.end())
      return common_failure("PrivateShopEmploymentSourceMismatch",
                            s.contract_id);
    if (boundary.empty()) {
      if (s.terminal_fact_id.empty())
        return common_failure("PrivateShopWorkerCommitted", s.contract_id);
      continue;
    }
    const auto boundary_number = organization_instance_sequence(
        boundary, c.state().facts.find(boundary)->domain == "estate"
                      ? "estate"
                      : "estate_employment_decision");
    const auto accepted_number = organization_instance_sequence(
        s.acceptance_fact_id, "private_shop_staff");
    if (!boundary_number || *boundary_number >= c.state().next_instance_id ||
        !accepted_number ||
        ((*accepted_number < *boundary_number) != (start < end)))
      return common_failure("PrivateShopEmploymentSourceOrderMismatch",
                            s.contract_id);
    if (start >= end) {
      if (s.accepted_tick < tick)
        return common_failure("PrivateShopEmploymentSourceOrderMismatch",
                              s.contract_id);
      continue;
    }
    if (s.accepted_tick > tick)
      return common_failure("PrivateShopEmploymentSourceOrderMismatch",
                            s.contract_id);
    const auto terminal =
        std::find(order.begin(), order.end(), s.terminal_fact_id);
    if (s.terminal_fact_id.empty() || terminal == order.end() ||
        terminal >= end)
      return common_failure("PrivateShopWorkerCommitted", s.contract_id);
    if (s.terminal_tick > tick)
      return common_failure("PrivateShopEmploymentSourceOrderMismatch",
                            s.contract_id);
  }
  return common_success();
}
} // namespace hanmo::resident
