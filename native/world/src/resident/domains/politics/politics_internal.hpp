// 政治规则内部校验；读口只使用当前候选的真实人物、职责和位置。
#pragma once
#include "politics_model.hpp"
#include "politics_organization_runtime.hpp"
#include "resident/domains/growth/growth_actor_qualification.hpp"
#include "resident/domains/local_economy/local_economy_internal.hpp"
#include "resident/domains/organization/grant_authority.hpp"
#include <limits>
namespace hanmo::resident::politics_detail {
// 各终态保留历史；只有未答复与已受任才占用待任窗口。
inline bool pending(AppointmentStatus s) {
  return s == AppointmentStatus::offered || s == AppointmentStatus::accepted;
}
// 每一份下授Grant必须完整包含于签发者的一份可再委任Grant，不能拼接范围扩权。
inline bool delegation_allowed(const CommonStore &c, const OrganizationState &o,
                               const OrganizationCatalog &oc,
                               const StableId &issuer_office,
                               const StableId &target_office) {
  const auto *from = o.offices.find(issuer_office);
  const auto *to = o.offices.find(target_office);
  if (!from || !to || from->organization_id != to->organization_id)
    return false;
  for (const auto &id : to->grant_ids) {
    const auto *child = oc.grants.find(id);
    if (!child)
      return false;
    bool covered = false;
    for (const auto &parent_id : from->grant_ids) {
      const auto *parent = oc.grants.find(parent_id);
      if (!parent || !parent->may_delegate || c.tick() < parent->valid_from ||
          (parent->valid_until && c.tick() > *parent->valid_until) ||
          child->amount_limit > parent->amount_limit ||
          (parent->valid_until &&
           (!child->valid_until || *child->valid_until > *parent->valid_until)))
        continue;
      bool contains = true;
      for (const auto &kind : child->allowed_command_kinds)
        if (std::find(parent->allowed_command_kinds.begin(),
                      parent->allowed_command_kinds.end(),
                      kind) == parent->allowed_command_kinds.end())
          contains = false;
      for (const auto &scope : child->scopes)
        if (std::none_of(parent->scopes.begin(), parent->scopes.end(),
                         [&](const auto &a) {
                           return a.kind == scope.kind && a.id == scope.id;
                         }))
          contains = false;
      for (const auto &wallet : child->wallet_ids)
        if (std::find(parent->wallet_ids.begin(), parent->wallet_ids.end(),
                      wallet) == parent->wallet_ids.end())
          contains = false;
      for (const auto &inventory : child->inventory_ids)
        if (std::find(parent->inventory_ids.begin(),
                      parent->inventory_ids.end(),
                      inventory) == parent->inventory_ids.end())
          contains = false;
      if (contains) {
        covered = true;
        break;
      }
    }
    if (!covered)
      return false;
  }
  return true;
}
// 完整权源只读唯一current_grants与实际Moment；旧入口保持原静态目录分支。
inline Outcome<bool>
delegation_allowed_at(const CommonStore &c, const OrganizationState &o,
                      const OrganizationCatalog &oc, const StableId &issuer,
                      const StableId &target,
                      const OrganizationAuthorityClock *clock) {
  if (!clock)
    return {true, delegation_allowed(c, o, oc, issuer, target), {}};
  auto now = read_organization_authority_now(c, clock);
  if (!now.ok)
    return {false, false, now.error};
  const auto *from = o.offices.find(issuer);
  const auto *to = o.offices.find(target);
  if (!from || !to || from->organization_id != to->organization_id)
    return {true, false, {}};
  for (const auto &id : to->grant_ids) {
    auto child = read_current_grant(c, o, oc, id);
    if (!child.ok)
      return {false, false, child.error};
    bool covered = false;
    for (const auto &parent_id : from->grant_ids) {
      auto parent = read_current_grant(c, o, oc, parent_id);
      if (!parent.ok) {
        if (parent.error.kind != ErrorKind::rule)
          return {false, false, parent.error};
        continue;
      }
      auto valid =
          authority_window_contains(parent.value.terms.valid_from,
                                    parent.value.terms.valid_until, now.value);
      if (!valid.ok)
        return {false, false, valid.error};
      if (valid.value && parent.value.terms.may_delegate &&
          grant_terms_within(child.value.terms, parent.value.terms)) {
        covered = true;
        break;
      }
    }
    if (!covered)
      return {true, false, {}};
  }
  return {true, true, {}};
}
inline Status political_authority(const CommonStore &c,
                                  const OrganizationState &o,
                                  const OrganizationCatalog &oc,
                                  const ActingAuthority &acting,
                                  const StableId &command,
                                  const ScopeRef &scope,
                                  const OrganizationAuthorityClock *clock) {
  if (!clock) {
    auto old = check_authority(c, o, oc, acting, command, scope, 0);
    return old.ok ? common_success() : Status{false, {}, old.error};
  }
  auto proof = check_current_grant_authority(
      c, o, oc, oc.authority_catalog_identity,
      {acting, command, scope, 0, {}, false, {}}, clock);
  return proof.ok ? common_success() : Status{false, {}, proof.error};
}
inline Status
political_adult(const CommonStore &c, const StableId &actor,
                const PoliticalOrganizationRuntimeReadContext *runtime) {
  if (!runtime)
    return growth_independent_office_age(c, actor);
  if (!runtime->qualifications)
    return common_failure("PoliticalActorQualificationSourceRequired", actor,
                          ErrorKind::unsupported_capability);
  auto now = read_organization_authority_now(c, &runtime->clock);
  if (!now.ok)
    return {false, {}, now.error};
  return runtime->qualifications->validate_independent_office_age(actor,
                                                                  now.value);
}
// 同地判断通过共同位置解析，跟随或在军人物不能凭表现坐标受任。
inline std::optional<SettlementLocation> place(const CommonStore &c,
                                               const StableId &id) {
  auto loc = c.resolved_location(id);
  if (!loc.ok)
    return {};
  if (const auto *p = std::get_if<SettlementLocation>(&loc.value))
    return *p;
  return {};
}
// 行政答复不得抢占已有行动或战斗；真实会话可承载当面办理。
inline Status available(const CommonStore &c, const ActivityRead &a,
                        const StableId &id) {
  const auto *p = c.actor(id);
  if (!p || !p->alive || !p->available)
    return common_failure("PoliticalActorUnavailable", id);
  if (!p->action_id.empty() || a.in_open_battle(id))
    return common_failure("PoliticalActorBusy", id);
  return common_success();
}
// 同一任命保存发起者身份和epoch；换任后须重新签发而非复用旧公权。
inline Status
appointment_source(const CommonStore &c, const PoliticalAppointment &p,
                   const PoliticsState &s, const OrganizationState &o,
                   const OrganizationCatalog &oc,
                   const OrganizationAuthorityClock *clock = nullptr) {
  const auto *j = s.jurisdictions.find(p.jurisdiction_id);
  if (!j || j->revision != p.jurisdiction_revision ||
      j->political_organization_id != p.organization_id ||
      j->controlling_organization_id != p.organization_id)
    return common_failure("PoliticalJurisdictionChanged", p.jurisdiction_id);
  const auto *office = o.offices.find(j->office_id);
  if (!office || office->epoch != p.target_office_epoch ||
      office->holder_actor_id || office->status == OfficeStatus::active)
    return common_failure("PoliticalOfficeNotVacant", j->office_id);
  auto delegation =
      delegation_allowed_at(c, o, oc, p.issuer_office_id, j->office_id, clock);
  if (!delegation.ok)
    return {false, {}, delegation.error};
  if (!delegation.value)
    return common_failure("PoliticalDelegationExceedsAuthority", j->office_id);
  return political_authority(c, o, oc,
                             {p.organization_id, p.issuer_office_id,
                              p.issuer_epoch, p.issuer_actor_id},
                             "political_appoint",
                             {ScopeRef::Kind::site, j->seat_place_id}, clock);
}

// 同候选全局事实头与本域载荷同步追加，任何失败由外层savepoint回滚。
inline Outcome<StableId> append_fact(CommonStore &c, PoliticsState &s,
                                     PoliticsFact fact) {
  auto id = c.next_id("politics");
  if (!id.ok)
    return id;
  fact.id = id.value;
  auto result = c.append_fact({id.value, fact.kind, "politics"});
  if (!result.ok)
    return {false, {}, result.error};
  if (!s.facts.insert(id.value, std::move(fact)))
    return {false, {}, common_failure("DuplicatePoliticsFact", id.value).error};
  return id;
}
// 受任决定明确引用前一步，不覆盖最初签发事实。
inline Outcome<StableId>
appointment_fact(CommonStore &c, PoliticsState &s, PoliticalAppointment &p,
                 const StableId &kind, AppointmentStatus status,
                 const StableId &decision_actor = {},
                 std::optional<WorldMoment> actual_moment = {}) {
  const auto *j = s.jurisdictions.find(p.jurisdiction_id);
  PoliticsFact fact;
  fact.kind = kind;
  fact.jurisdiction_id = p.jurisdiction_id;
  fact.appointment_id = p.id;
  fact.actor_id = p.actor_id;
  fact.issuer_actor_id = p.issuer_actor_id;
  fact.organization_id = p.organization_id;
  fact.office_id = j->office_id;
  fact.decision_actor_id = decision_actor;
  if (!decision_actor.empty()) {
    fact.witness_actor_ids.push_back(decision_actor);
    const auto at = place(c, decision_actor);
    for (const auto &other : {p.actor_id, p.issuer_actor_id}) {
      const auto there = place(c, other);
      if (other != decision_actor && at && there &&
          at->place_id == there->place_id &&
          at->settlement_id == there->settlement_id &&
          std::find(fact.witness_actor_ids.begin(),
                    fact.witness_actor_ids.end(),
                    other) == fact.witness_actor_ids.end())
        fact.witness_actor_ids.push_back(other);
    }
  }
  fact.tick = actual_moment ? actual_moment->half_day_tick : c.tick();
  fact.occurred_at = std::move(actual_moment);
  fact.office_epoch = p.target_office_epoch;
  fact.status = status;
  if (!p.last_fact_id.empty())
    fact.cause_fact_ids.push_back(p.last_fact_id);
  auto id = append_fact(c, s, std::move(fact));
  if (id.ok) {
    p.last_fact_id = id.value;
    p.status = status;
    ++p.revision;
  }
  return id;
}
} // namespace hanmo::resident::politics_detail
