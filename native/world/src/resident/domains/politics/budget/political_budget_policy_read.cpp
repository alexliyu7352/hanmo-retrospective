#include "political_budget_policy_read.hpp"
#include "political_budget_bucket.hpp"
#include "resident/domains/politics/takeover/politics_takeover_absence.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <set>
namespace hanmo::resident {
namespace {
bool same_unit(const BudgetUnit &a, const BudgetUnit &b) {
  if (a.index() != b.index())
    return false;
  const auto *i = std::get_if<BudgetItemQuantity>(&a);
  return !i || i->item_id == std::get<BudgetItemQuantity>(b).item_id;
}
bool same_catalog(const CatalogIdentity &a, const CatalogIdentity &b) {
  return a.content_id == b.content_id &&
         a.content_version == b.content_version &&
         a.content_digest == b.content_digest && a.ruleset_id == b.ruleset_id &&
         a.schema_sha256 == b.schema_sha256 &&
         a.pack_set_digest == b.pack_set_digest &&
         a.module_versions == b.module_versions &&
         a.artifact_sha256 == b.artifact_sha256;
}
PublicBudgetPolicyPurpose policy_purpose(BudgetExecutionPurpose p) {
  return p == BudgetExecutionPurpose::material_aid
             ? PublicBudgetPolicyPurpose::aid
             : PublicBudgetPolicyPurpose::work;
}
PublicBudgetTerms terms_of(const BudgetAuthorizationSource &s) {
  return {s.policy_id,      s.policy_revision, s.period,     s.scope,
          s.container_refs, s.purposes,        s.total_caps, s.category_caps};
}
bool has(const std::vector<StableId> &v, const StableId &id) {
  return std::find(v.begin(), v.end(), id) != v.end();
}
} // namespace
NativePublicBudgetPolicyRead::NativePublicBudgetPolicyRead(
    const WorldBundle &w, const PoliticsState &p,
    const PublicBudgetPolicyCatalog &c, const CatalogIdentity &i,
    const PoliticalTakeoverContext *takeover,
    const PoliticalBudgetState &budgets,
    const PublicBudgetTreasuryHistoryRead &treasury)
    : world_(w), politics_(p), policies_(c), identity_(i),
      takeover_(takeover), budgets_(budgets), treasury_(treasury) {}
bool NativePublicBudgetPolicyRead::bound_to(const WorldBundle &w) const {
  if(&w!=&world_||&w.politics!=&politics_||!w.catalog||!treasury_.bound_to(w))return false;
  const bool selected=w.catalog->political_takeover&&w.catalog->political_takeover->enabled;
  if(selected!=bool(takeover_))return false;
  if(!takeover_)return true;
  const auto&t=*takeover_;
  return w.catalog->common&&w.catalog->organization&&w.catalog->politics&&w.catalog->military&&w.organization_runtime&&
   &t.common==&w.common&& &t.common_catalog==w.catalog->common.get()&& &t.organizations==&w.organization&&
   &t.organization_catalog==w.catalog->organization.get()&& &t.politics_catalog==w.catalog->politics.get()&&
   &t.catalog==w.catalog->political_takeover.get()&& &t.state==&w.political_takeover&& &t.military==&w.military&&
   &t.military_catalog==w.catalog->military.get()&&t.politics_state==&w.politics&&t.organization_runtime==&*w.organization_runtime&&
   t.actual_clock&&t.state.version==2&&t.catalog.version==2;
}
Outcome<StableId> NativePublicBudgetPolicyRead::jurisdiction_owner(
    const StableId &id, const BudgetAuthorizationSource *approval) const {
  if (!bound_to(world_) || !world_.catalog || !world_.catalog->politics)
    return {false,
            {},
            common_failure("PublicBudgetPoliticalOwnerMismatch", id).error};
  if (const auto *context = takeover_) {
    PoliticalTakeoverTerritoryAccess actual(*context);
    auto control =
        approval ? actual.public_budget_jurisdiction_control_before_approval(
                       budgets_, *approval, id)
                 : actual.governance_jurisdiction_control_current(id);
    if (!control.ok)
      return {false, {}, control.error};
    return {true, control.value.controlling_organization_id, {}};
  }
  auto absent = validate_unselected_political_takeover_sources(world_);
  if (!absent.ok)
    return {false, {}, absent.error};
  const auto *seed = world_.catalog->politics->jurisdictions.find(id);
  const auto *current = politics_.jurisdictions.find(id);
  if (!seed || !current || seed->revision != 0 || current->revision != 0 ||
      current->controlling_organization_id !=
          seed->controlling_organization_id ||
      current->public_wallet_id != seed->public_wallet_id ||
      current->public_inventory_ids != seed->public_inventory_ids)
    return {
        false,
        {},
        common_failure("PublicBudgetJurisdictionOriginalMismatch", id).error};
  return {true, seed->controlling_organization_id, {}};
}
Status
NativePublicBudgetPolicyRead::validate_terms(const PublicBudgetTerms &t,
                                             const StableId &principal,
                                             const WorldMoment &at,const BudgetAuthorizationSource *previous,const SourceRef *actual_boundary) const {
  if (!bound_to(world_) || !world_.catalog ||
      !same_catalog(identity_, world_.catalog->identity) ||
      !same_catalog(policies_.identity, identity_))
    return common_failure("PublicBudgetPolicyOwnerMismatch");
  auto valid = validate_public_budget_policy_catalog_shape(policies_);
  if (!valid.ok)
    return valid;
  const auto *p =
      find_public_budget_policy(policies_, t.policy_id, t.policy_revision);
  if (!p)
    return common_failure("PublicBudgetPolicyRevisionMissing", t.policy_id);
  if (principal.empty() ||
      (p->scope.kind == PublicBudgetPolicyScope::Kind::organization &&
       principal != p->scope.id))
    return common_failure("PublicBudgetPolicyPrincipalMismatch", t.policy_id);
  auto start = compare_world_moments(p->period.from, at);
  if (!start.ok)
    return {false, {}, start.error};
  auto end = compare_world_moments(at, p->period.to);
  if (!end.ok)
    return {false, {}, end.error};
  if (start.value > 0 || end.value >= 0)
    return common_failure("PublicBudgetPolicyNotEffective", t.policy_id);
  // 原预算周期为整半日，精确作者窗不得通过截断分数时刻扩大。
  if (t.period.period_from_tick < 0 ||
      t.period.period_until_tick <= t.period.period_from_tick)
    return common_failure("InvalidBudgetPeriod", t.policy_id);
  auto from = compare_world_moments(p->period.from,
                                    WorldMoment{t.period.period_from_tick, {}});
  if (!from.ok)
    return {false, {}, from.error};
  auto until = compare_world_moments(
      WorldMoment{t.period.period_until_tick, {}}, p->period.to);
  if (!until.ok)
    return {false, {}, until.error};
  if (from.value > 0 || until.value > 0)
    return common_failure("PublicBudgetPolicyPeriodDenied", t.policy_id);
  if (t.purposes.empty() || t.total_caps.empty() || t.container_refs.empty())
    return common_failure("IncompletePublicBudgetTerms", t.policy_id);
  std::set<BudgetExecutionPurpose> purposes;
  for (auto purpose : t.purposes) {
    valid = validate_budget_execution_purpose(purpose, 2);
    if (!valid.ok)
      return valid;
    if (!purposes.insert(purpose).second)
      return common_failure("DuplicateBudgetPurpose", t.policy_id);
    if (std::find(p->purposes.begin(), p->purposes.end(),
                  policy_purpose(purpose)) == p->purposes.end())
      return common_failure("PublicBudgetPolicyPurposeDenied",
                            budget_execution_purpose_key(purpose));
  }
  std::set<StableId> containers;
  for (const auto &id : t.container_refs) {
    if (!containers.insert(id).second)
      return common_failure("DuplicateBudgetContainer", id);
    if (std::none_of(p->containers.begin(), p->containers.end(),
                     [&](const auto &r) { return r.id == id; }))
      return common_failure("PublicBudgetPolicyContainerDenied", id);
  }
  std::set<StableId> unit_containers;
  for (std::size_t n = 0; n < t.total_caps.size(); ++n) {
    const auto &cap = t.total_caps[n];
    const PublicBudgetPolicyLimit *limit = nullptr;
    for (const auto &row : p->total_caps)
      if (same_unit(row.unit, cap.unit))
        limit = &row;
    if (!limit || cap.cap < 0 || cap.cap > limit->cap || cap.civil_floor < 0 ||
        cap.garrison_floor < 0)
      return common_failure("PublicBudgetPolicyCapDenied", t.policy_id);
    for (std::size_t j = 0; j < n; ++j)
      if (same_unit(t.total_caps[j].unit, cap.unit))
        return common_failure("DuplicateBudgetUnit", t.policy_id);
    if (cap.container_refs.empty())
      return common_failure("BudgetUnitContainersMissing", t.policy_id);
    std::set<StableId> local;
    const auto kind = std::holds_alternative<BudgetMoneyQian>(cap.unit)
                          ? ResourceKey::Kind::wallet
                          : ResourceKey::Kind::inventory;
    for (const auto &id : cap.container_refs) {
      if (!local.insert(id).second || !containers.count(id) ||
          std::none_of(
              p->containers.begin(), p->containers.end(),
              [&](const auto &r) { return r.id == id && r.kind == kind; }))
        return common_failure("PublicBudgetPolicyUnitContainerDenied", id);
      unit_containers.insert(id);
    }
    for (auto purpose : t.purposes) {
      const PublicBudgetCategoryLimit *category = nullptr;
      for (const auto &row : t.category_caps)
        if (row.purpose == purpose && same_unit(row.unit, cap.unit)) {
          if (category)
            return common_failure("DuplicateBudgetCategory", t.policy_id);
          category = &row;
        }
      const PublicBudgetPolicyPurposeLimit *allowed = nullptr;
      for (const auto &row : p->purpose_caps)
        if (row.purpose == policy_purpose(purpose) &&
            same_unit(row.unit, cap.unit))
          allowed = &row;
      if (!category || !allowed || category->cap < 0 ||
          category->cap > cap.cap || category->cap > allowed->cap)
        return common_failure("PublicBudgetPolicyPurposeCapDenied",
                              budget_execution_purpose_key(purpose));
    }
  }
  if (unit_containers != containers)
    return common_failure("BudgetUnitContainerCoverageMismatch", t.policy_id);
  for (const auto &category : t.category_caps) {
    if (std::none_of(t.total_caps.begin(),t.total_caps.end(),[&](const auto&cap){return same_unit(cap.unit,category.unit)&&category.cap>=0&&category.cap<=cap.cap;}))
      return common_failure("BudgetCategoryNotAuthorized",t.policy_id);
    if (!purposes.count(category.purpose)) {
      // Retired purpose retains its original liability capacity, not permission
      // to adopt more work. No new policy version can erase this historical cap.
      const PublicBudgetCategoryLimit *old=nullptr;
      if(previous)for(const auto&value:previous->category_caps)
        if(value.purpose==category.purpose&&same_unit(value.unit,category.unit))old=&value;
      if(!previous){
        auto roots=read_public_budget_reopening_roots(world_.common,budgets_,principal,t.period,t.purposes,actual_boundary);
        if(!roots.ok)return {false,{},roots.error};
        for(const auto&ref:roots.value){const auto*prior=find_budget_authorization(budgets_,ref.authorization_id,ref.authorization_revision);
          for(const auto&value:prior->category_caps)if(value.purpose==category.purpose&&same_unit(value.unit,category.unit))old=&value;
        }
      }
      if(!old||category.cap>old->cap)
        return common_failure("BudgetRetiredPurposeOriginalRequired",t.policy_id);
    }
  }
  return common_success();
}
Status NativePublicBudgetPolicyRead::validate_approval_terms(const ActingAuthority&a,PublicBudgetDecisionKind k,const PublicBudgetTerms&t)const {
  return validate_approval_transition_terms(a,k,t,nullptr);
}
Status NativePublicBudgetPolicyRead::validate_approval_transition_terms(
    const ActingAuthority &a, PublicBudgetDecisionKind decision,
    const PublicBudgetTerms &t,const BudgetAuthorizationSource *previous) const {
  auto at = world_.actual_phase_moment();
  if (!at.ok)
    return {false, {}, at.error};
  if(previous && (current_budget_authorization(budgets_,previous->budget_id)!=previous || previous->principal_org_id!=a.organization_id))
    return common_failure("PublicBudgetPolicyPreviousSourceMismatch");
  if(!previous&&decision!=PublicBudgetDecisionKind::authorize)
    for(const auto&category:t.category_caps)if(std::find(t.purposes.begin(),t.purposes.end(),category.purpose)==t.purposes.end())
      return common_failure("PublicBudgetDelegationCannotInheritRootCategory");
  auto valid = validate_terms(t, a.organization_id, at.value,previous);
  if (!valid.ok)
    return valid;
  const auto &p =
      *find_public_budget_policy(policies_, t.policy_id, t.policy_revision);
  const Jurisdiction *j = nullptr;
  if (p.scope.kind == PublicBudgetPolicyScope::Kind::jurisdiction) {
    j = politics_.jurisdictions.find(p.scope.id);
    auto owner = jurisdiction_owner(p.scope.id, nullptr);
    if (!owner.ok)
      return {false, {}, owner.error};
    if (!j || owner.value != a.organization_id)
      return common_failure("PublicBudgetPolicyControlMismatch", p.scope.id);
  }
  for (const auto &r : p.containers) {
    if (!has(t.container_refs, r.id))
      continue;
    if (r.kind == ResourceKey::Kind::wallet) {
      const auto *w = world_.common.wallet(r.id);
      if (!w || w->owner.kind != Owner::Kind::organization ||
          w->owner.id != a.organization_id ||
          (j && j->public_wallet_id != r.id))
        return common_failure("PublicBudgetTreasuryMismatch", r.id);
    } else {
      const auto *i = world_.common.inventory(r.id);
      if (!i || i->owner.kind != Owner::Kind::organization ||
          i->owner.id != a.organization_id ||
          (j && !has(j->public_inventory_ids, r.id)))
        return common_failure("PublicBudgetTreasuryMismatch", r.id);
    }
  }
  // Office/SingleGrant/GrantScope核验由原writer在同一actualM执行，不将政策当成授权。
  return common_success();
}
Status NativePublicBudgetPolicyRead::validate_historical_terms(
    const BudgetAuthorizationSource &s) const {
  if(!bound_to(world_)||budgets_.schema_version!=2||find_budget_authorization(budgets_,s.budget_id,s.budget_revision)!=&s)
    return common_failure("PublicBudgetActualApprovalRequired",s.approval_source.id);
  auto source_shape=validate_budget_authorization_shape(s,2);if(!source_shape.ok)return source_shape;
  const auto*head=world_.common.state().facts.find(s.approval_source.id);
  const char*kind=nullptr;switch(s.decision){case PublicBudgetDecisionKind::authorize:kind="public_budget_authorized";break;case PublicBudgetDecisionKind::delegate:kind="public_budget_delegated";break;case PublicBudgetDecisionKind::amend:kind="public_budget_amended";break;case PublicBudgetDecisionKind::close:kind="public_budget_closed";break;}
  if(!kind||!head||head->id!=s.approval_source.id||head->domain!="politics"||head->kind!=kind||s.approval_source.domain!=head->domain||s.approval_source.kind!=head->kind)
    return common_failure("PublicBudgetActualApprovalRequired",s.approval_source.id);
  auto now=world_.actual_phase_moment();if(!now.ok)return {false,{},now.error};
  auto order=compare_world_moments(s.approved_at,now.value);if(!order.ok)return {false,{},order.error};if(order.value>0)return common_failure("PublicBudgetFutureApproval",s.approval_source.id);
  // 当前控制权不能重写既有批准；历史Office/Grant仍由原AuthorityBasis反读。
  if(s.budget_revision==1&&s.parent_budget_ref)
    for(const auto&category:s.category_caps)if(std::find(s.purposes.begin(),s.purposes.end(),category.purpose)==s.purposes.end())
      return common_failure("PublicBudgetDelegationCannotInheritRootCategory",s.budget_id);
  const auto*previous=s.budget_revision>1?find_budget_authorization(budgets_,s.budget_id,s.budget_revision-1):nullptr;
  if(s.budget_revision>1&&!previous)return common_failure("PublicBudgetPolicyPreviousSourceMissing",s.budget_id);
  auto valid = validate_terms(terms_of(s), s.principal_org_id, s.approved_at,previous,&s.approval_source);
  if (!valid.ok)
    return valid;
  const auto &p =
      *find_public_budget_policy(policies_, s.policy_id, s.policy_revision);
  if (p.scope.kind == PublicBudgetPolicyScope::Kind::jurisdiction) {
    auto owner = jurisdiction_owner(p.scope.id, &s);
    if (!owner.ok)
      return {false, {}, owner.error};
    if (owner.value != s.principal_org_id)
      return common_failure("PublicBudgetHistoricalJurisdictionMismatch",
                            p.scope.id);
  }
  const Jurisdiction* authored_jurisdiction=nullptr;
  if(p.scope.kind==PublicBudgetPolicyScope::Kind::jurisdiction){
    if(!world_.catalog||!world_.catalog->politics)return common_failure("PublicBudgetJurisdictionOriginalMissing",p.scope.id);
    authored_jurisdiction=world_.catalog->politics->jurisdictions.find(p.scope.id);
    if(!authored_jurisdiction)return common_failure("PublicBudgetJurisdictionOriginalMissing",p.scope.id);
  }
  // Only selected actual containers; no current Owner==seed shortcut and no
  // first allowlisted resource fallback. The lease follows actual transfer
  // history before this exact approved row, including leave-and-return paths.
  for(const auto&resource:p.containers){if(!has(s.container_refs,resource.id))continue;
    if(authored_jurisdiction&&((resource.kind==ResourceKey::Kind::wallet&&authored_jurisdiction->public_wallet_id!=resource.id)||(resource.kind==ResourceKey::Kind::inventory&&!has(authored_jurisdiction->public_inventory_ids,resource.id))))
      return common_failure("PublicBudgetHistoricalJurisdictionContainerMismatch",resource.id);
    auto owner=treasury_.read_public_budget_asset_owner_before_approval(budgets_,s,resource);if(!owner.ok)return {false,{},owner.error};
    if(owner.value.kind!=Owner::Kind::organization||owner.value.id!=s.principal_org_id)return common_failure("PublicBudgetHistoricalTreasuryMismatch",resource.id);
  }
  return common_success();
}
} // namespace hanmo::resident
