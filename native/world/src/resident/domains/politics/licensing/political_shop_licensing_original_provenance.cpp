#include "political_shop_licensing_original_provenance.hpp"
#include "political_shop_licensing_exact2.hpp"
#include "resident/domains/organization/grant_authority_history_provenance.hpp"
#include "resident/domains/organization/organization_model.hpp"
#include <functional>
namespace hanmo::resident {
namespace {
Status fact(const PoliticalShopLicensingContext &x, const OrganizationFact &f) {
  const auto *h = x.common.state().facts.find(f.id);
  if (!h || h->id != f.id || h->kind != f.kind || h->domain != "organization")
    return common_failure("PoliticalShopOriginalHeadRequired", f.id);
  return x.original_sink->organization_fact(x, *h, f);
}
Status office(const PoliticalShopLicensingContext &x, const StableId &id,
              bool historical = true) {
  const auto *initial = x.organization_catalog.offices.find(id);
  const auto *current = x.organizations.offices.find(id);
  if (!initial || !current)
    return common_failure("PoliticalShopOriginalOfficeRequired", id);
  auto result = x.original_sink->office(x, *initial, *current);
  if (!result.ok)
    return result;
  if (!historical)
    return common_success();
  // 原任职读验证完整后缀；只该Office，不枚举无关Common。
  for (const auto &row : x.organizations.facts.rows())
    if (row.second.kind == "organization_office_changed" &&
        row.second.office_id == id) {
      result = fact(x, row.second);
      if (!result.ok)
        return result;
    }
  return common_success();
}
} // namespace
namespace shop_law_provenance {
Status boundary(const PoliticalShopLicensingContext &x,
                const StableId &boundary) {
  if (!x.original_sink)
    return common_success();
  const auto *head = x.common.state().facts.find(boundary);
  if (!head || head->id != boundary)
    return common_failure("PoliticalShopOriginalCutRequired", boundary);
  auto at = read_political_shop_permission_boundary_moment2(
      x, {head->domain, head->kind, head->id});
  if (!at.ok)
    return {false, {}, at.error};
  return x.original_sink->boundary(x, *head, *at.value);
}
Status authority(const PoliticalShopLicensingContext &x,
                 const ActingAuthority &actor, const StableId &cut) {
  if (!x.original_sink)
    return common_success();
  auto result = boundary(x, cut);
  if (!result.ok)
    return result;
  result = office(x, actor.office_id);
  if (!result.ok)
    return result;
  for (const auto &row : x.organizations.members.rows()) {
    const auto &m = row.second;
    if (m.actor_id != actor.actor_id ||
        m.organization_id != actor.organization_id)
      continue;
    const auto *policy =
        x.organization_catalog.member_policies.find(m.policy_id);
    if (!policy)
      return common_failure("PoliticalShopOriginalMemberPolicyRequired", m.id);
    result = x.original_sink->member(
        x, x.organization_catalog.members.find(m.id), m, *policy);
    if (!result.ok)
      return result;
    if (m.episode_source) {
      const auto &source = *m.episode_source;
      const auto *offer =
          x.organizations.membership_offers.find(source.offer_id);
      const auto *decision =
          x.organizations.membership_decisions.find(source.decision_id);
      const auto *terms =
          x.organization_catalog.membership_terms.find(source.terms_id);
      if (!offer || !decision || !terms)
        return common_failure("PoliticalShopOriginalMembershipRequired", m.id);
      result = x.original_sink->membership(x, *offer, *decision, *terms);
      if (!result.ok)
        return result;
      const auto *f = x.organizations.facts.find(source.decision_id);
      if (!f)
        return common_failure("PoliticalShopOriginalMembershipHeadRequired",
                              source.decision_id);
      result = fact(x, *f);
      if (!result.ok)
        return result;
    }
  }
  for (const auto &row : x.organizations.facts.rows())
    if (row.second.kind == "organization_member_response" &&
        row.second.actor_id == actor.actor_id &&
        row.second.organization_id == actor.organization_id) {
      result = fact(x, row.second);
      if (!result.ok)
        return result;
    }
  // 原actual-use Grant门已执行全历史校验，而非只校验所选Grant。
  for (const auto &row : x.organization_catalog.grants.rows()) {
    result = x.original_sink->initial_grant(x, row.second);
    if (!result.ok)
      return result;
    result = office(x, row.second.office_id, false);
    if (!result.ok)
      return result;
  }
  if (x.organization_catalog.authority_lifecycle_enabled) {
    // 原全Grant门反扫每个Office的grant_ids，零Grant Office也是真正依赖。
    // 这里只借目录/当前行，不把未读取的全Office历史或Common负扫描永久保留。
    for (const auto &row : x.organization_catalog.offices.rows()) {
      result = office(x, row.first, false);
      if (!result.ok)
        return result;
    }
    for (const auto &row : x.organization_catalog.grant_policies.rows()) {
      result = x.original_sink->grant_policy(x, row.second);
      if (!result.ok)
        return result;
    }
    for (const auto &row : x.organizations.current_grants.rows()) {
      result = x.original_sink->current_grant(x, row.second);
      if (!result.ok)
        return result;
    }
    for (const auto &row : x.organizations.facts.rows())
      if (row.second.grant_change_payload) {
        result = fact(x, row.second);
        if (!result.ok)
          return result;
      }
    class ActualOfficeUses final : public GrantAuthorityHistoryOfficeSink {
      const PoliticalShopLicensingContext &x_;

    public:
      explicit ActualOfficeUses(const PoliticalShopLicensingContext &x)
          : x_(x) {}
      Status office_use(const CommonStore &common,
                        const OrganizationState &state,
                        const OrganizationCatalog &catalog,
                        const FactRecord &head, const OrganizationFact &change,
                        const WorldMoment &at, const OfficeState &initial,
                        const OfficeState &current) override {
        if (&common != &x_.common || &state != &x_.organizations ||
            &catalog != &x_.organization_catalog ||
            common.state().facts.find(head.id) != &head ||
            state.facts.find(head.id) != &change ||
            catalog.offices.find(initial.id) != &initial ||
            state.offices.find(initial.id) != &current)
          return common_failure("PoliticalShopGrantProvenanceWrongCandidate",
                                head.id);
        auto result = x_.original_sink->boundary(x_, head, at);
        if (!result.ok)
          return result;
        return office(x_, initial.id);
      }
    } actual_uses(x);
    if (!x.clock)
      return common_failure("PoliticalShopOriginalClockRequired");
    result = visit_grant_authority_history_office_uses(
        x.common, x.organizations, x.organization_catalog, *x.clock,
        actual_uses);
    if (!result.ok)
      return result;
  }
  return boundary(x, cut);
}
} // namespace shop_law_provenance
Status visit_political_shop_permission_originals2(
    const PoliticalShopLicensingContext &x,
    const PrivateShopPermissionRequest &request, const StableId &opening,
    Integer tick, PoliticalShopLicensingOriginalSink &sink) {
  if (x.state.version != 2 || x.original_sink)
    return common_failure("PoliticalShopOriginalVisitorSelectionRequired");
  // reference_wrapper保持原死亡reader借用，不复制std::function拥有对象；本栈结束即失效。
  PoliticalShopLicensingContext borrowed{x.common,
                                         x.common_catalog,
                                         x.politics,
                                         x.politics_catalog,
                                         x.organizations,
                                         x.organization_catalog,
                                         x.economy,
                                         x.activity,
                                         x.interactions,
                                         x.interaction_catalog,
                                         x.interaction_access,
                                         x.catalog,
                                         x.state,
                                         x.premises,
                                         x.takeover,
                                         std::cref(x.death_origin),
                                         x.phase,
                                         x.registry_successions,
                                         x.clock,
                                         x.character_sources,
                                         x.moment_clock,
                                         x.opening_sources,
                                         &sink,
                                         x.read_boundary2};
  PoliticalPrivateShopPermissionAccess actual(borrowed);
  auto result = actual.private_shop_opening_permission_before_fact_owned(
      request, opening, tick);
  return result.ok ? common_success() : Status{false, {}, result.error};
}
} // namespace hanmo::resident
