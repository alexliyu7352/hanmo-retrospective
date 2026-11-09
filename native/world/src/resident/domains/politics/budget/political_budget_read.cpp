#include "political_budget_history_order.hpp"
#include "political_budget_read.hpp"
#include "political_budget_bucket.hpp"
#include "political_budget_commands.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/original_catalog_identity.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
bool same_unit(const BudgetUnit &a, const BudgetUnit &b) {
  if (a.index() != b.index())
    return false;
  const auto *x = std::get_if<BudgetItemQuantity>(&a);
  return !x || x->item_id == std::get<BudgetItemQuantity>(b).item_id;
}
bool same_period(const BudgetPeriod &a, const BudgetPeriod &b) {
  return a.period_from_tick == b.period_from_tick &&
         a.period_until_tick == b.period_until_tick;
}
const char *kind(PublicBudgetDecisionKind k) {
  switch (k) {
  case PublicBudgetDecisionKind::authorize:
    return "public_budget_authorized";
  case PublicBudgetDecisionKind::delegate:
    return "public_budget_delegated";
  case PublicBudgetDecisionKind::amend:
    return "public_budget_amended";
  case PublicBudgetDecisionKind::close:
    return "public_budget_closed";
  }
  return nullptr;
}
const char *command(PublicBudgetDecisionKind k) {
  switch (k) {
  case PublicBudgetDecisionKind::authorize:
    return "authorize_public_budget";
  case PublicBudgetDecisionKind::delegate:
    return "delegate_public_budget";
  case PublicBudgetDecisionKind::amend:
    return "amend_public_budget";
  case PublicBudgetDecisionKind::close:
    return "close_public_budget";
  }
  return nullptr;
}
std::optional<std::size_t> ordinal(const CommonStore &c, const SourceRef &ref) {
  const auto *f = c.state().facts.find(ref.id);
  if (!f || f->domain != ref.domain || f->kind != ref.kind)
    return {};
  const auto &order = c.state().facts.order();
  const auto found = std::find(order.begin(), order.end(), ref.id);
  if (found == order.end())
    return {};
  return static_cast<std::size_t>(found - order.begin());
}
} // namespace
bool PoliticalBudgetRead::bound_to(const WorldBundle&w,const PoliticalBudgetState&s)const noexcept {
 return &world_==&w && &common_==&w.common && &state_==&s && w.catalog && w.catalog->organization && w.organization_runtime &&
  &organization_==&w.organization && &organization_catalog_==w.catalog->organization.get() &&
  &authority_bases_==&w.organization_runtime->authority_bases && same_original_catalog_identity(catalog_identity_,w.catalog->identity) &&
  (s.schema_version!=2||(policies_&&policies_->bound_to(w)&&policies_->bound_to_budget(s)));
}
PoliticalBudgetRead::PoliticalBudgetRead(const WorldBundle &w,
                                         const PoliticalBudgetState &s,
                                         const AuthorityBasisRegistry &bases,
                                         const OrganizationState &o,
                                         const OrganizationCatalog &oc,
                                         const CatalogIdentity &identity,
                                         const PublicBudgetPolicyRead *policies)
    : world_(w), common_(w.common), state_(s), authority_bases_(bases),
      organization_(o), organization_catalog_(oc), catalog_identity_(identity),
      policies_(policies) {}
Outcome<WorldMoment>
PoliticalBudgetRead::read_organization_authority_now() const {
  return world_.actual_phase_moment();
}
Outcome<AuthorityIssueSourceView>
PoliticalBudgetRead::read_authority_issue_source(const SourceRef &ref) const {
  const BudgetAuthorizationSource *found = nullptr;
  for (const auto &s : state_.authorization_history)
    if (same_source_ref(s.approval_source, ref)) {
      if (found)
        return {false,
                {},
                common_failure("DuplicateBudgetApprovalSource", ref.id).error};
      found = &s;
    }
  if (!found || !ordinal(common_, ref) || !kind(found->decision) ||
      ref.kind != kind(found->decision))
    return {
        false, {}, common_failure("BudgetApprovalSourceMissing", ref.id).error};
  ActorFoundationAt at;
  at.kind = ActorFoundationAt::Kind::exact_moment;
  at.exact_moment = found->approved_at;
  // 分量从实际批准cap/具名容器重建，不抄Basis来证明Basis本身。
  std::vector<AuthorityAmountComponent> amounts;
  std::set<StableId> all_containers;
  for (const auto &limit : found->total_caps) {
    if (limit.container_refs.empty())
      return {false,
              {},
              common_failure("BudgetUnitContainersMissing", found->budget_id)
                  .error};
    std::set<StableId> unit_containers;
    for (const auto &id : limit.container_refs) {
      if (!unit_containers.insert(id).second)
        return {false,
                {},
                common_failure("DuplicateBudgetUnitContainer", id).error};
      all_containers.insert(id);
    }
    if (std::holds_alternative<BudgetMoneyQian>(limit.unit)) {
      for (const auto &id : limit.container_refs)
        if (!common_.wallet(id))
          return {
              false, {}, common_failure("BudgetWalletSourceMissing", id).error};
      amounts.push_back(
          MoneyAuthorityAmount{"qian", limit.container_refs, limit.cap});
    } else {
      const auto &item = std::get<BudgetItemQuantity>(limit.unit);
      if (!common_.item_definition(item.item_id))
        return {false,
                {},
                common_failure("BudgetItemSourceMissing", item.item_id).error};
      for (const auto &id : limit.container_refs)
        if (!common_.inventory(id))
          return {false,
                  {},
                  common_failure("BudgetInventorySourceMissing", id).error};
      amounts.push_back(
          ItemAuthorityAmount{item.item_id, limit.container_refs, limit.cap});
    }
  }
  if (all_containers != std::set<StableId>(found->container_refs.begin(),
                                           found->container_refs.end()))
    return {
        false,
        {},
        common_failure("BudgetUnitContainerCoverageMismatch", found->budget_id)
            .error};
  return {true,
          {ref, found->issuer_actor_id, command(found->decision), std::move(at),
           std::move(amounts)},
          {}};
}
Status PoliticalBudgetRead::validate_budget_authorization(
    const BudgetAuthorizationRef &ref, const AuthorityBasis &basis) const {
  const auto *source = find_budget_authorization(state_, ref.authorization_id,
                                                 ref.authorization_revision);
  if (!source || !same_source_ref(source->approval_source, ref.approval_source))
    return common_failure("AuthorityBudgetSourceMismatch",
                          ref.authorization_id);
  const auto approval = ordinal(common_, source->approval_source),
             use = ordinal(common_, basis.issued_source);
  if (!approval || !use || *approval >= *use || source->closed ||
      basis.issued_tick < source->period.period_from_tick ||
      basis.issued_tick >= source->period.period_until_tick)
    return common_failure("AuthorityBudgetNotEffectiveAtIssue",
                          ref.authorization_id);
  // 原签发点前若有更新版本，不得拿已被改写/关闭的旧批准生成新的basis。
  for (const auto &other : state_.authorization_history)
    if (other.budget_id == source->budget_id &&
        other.budget_revision > source->budget_revision) {
      const auto next = ordinal(common_, other.approval_source);
      if (!next)
        return common_failure("BudgetApprovalSourceMissing", other.budget_id);
      if (*next < *use)
        return common_failure("HistoricalBudgetRevisionMismatch",
                              source->budget_id);
    }
  if(state_.schema_version==2){
    const BudgetAuthorizationSource* ancestor=source;std::set<StableId> seen;
    while(ancestor->parent_budget_ref){const auto id=ancestor->parent_budget_ref->authorization_id;
      if(!seen.insert(id).second)return common_failure("BudgetParentCycle",source->budget_id);
      const BudgetAuthorizationSource* original=nullptr;
      for(const auto&row:state_.authorization_history)if(row.budget_id==id){const auto sequence=ordinal(common_,row.approval_source);if(!sequence)return common_failure("BudgetApprovalSourceMissing",id);if(*sequence<*use&&(!original||row.budget_revision>original->budget_revision))original=&row;}
      if(!original||original->closed||!same_period(original->period,source->period))return common_failure("BudgetAncestorClosedAtAuthorityIssue",source->budget_id);
      ancestor=original;
    }
  }
  if (source->scope.kind != basis.scope.kind ||
      source->scope.id != basis.scope.id)
    return common_failure("AuthorityBudgetScopeDenied", source->budget_id);
  for (const auto &resource : basis.resources)
    if (std::find(source->container_refs.begin(), source->container_refs.end(),
                  resource.id) == source->container_refs.end())
      return common_failure("AuthorityBudgetContainerDenied", resource.id);
  return common_success();
}
Status PoliticalBudgetRead::validate_sources() const {
  if (state_.schema_version != 1 && state_.schema_version != 2)
    return common_failure("UnsupportedPoliticalBudgetsSchema",
                          std::to_string(state_.schema_version));
  if (state_.schema_version == 2 &&
      state_.authorization_history.size() > political_budget_history_limit)
    return common_failure("CheckpointCollectionTooLarge",
                          "political_budgets/authorization_history");
  if (state_.schema_version == 2 &&
      (!policies_ || !policies_->bound_to(world_) ||
       !policies_->bound_to_budget(state_)))
    return common_failure("PublicBudgetPolicyOwnerMismatch");
  if(state_.schema_version==2){auto buckets=validate_public_budget_bucket_history(common_,state_);if(!buckets.ok)return buckets;}
  auto actual_order=read_public_budget_authorizations_in_actual_order(common_,state_);
  if(!actual_order.ok)return {false,{},actual_order.error};
  std::map<StableId, const BudgetAuthorizationSource *> previous;
  std::set<StableId> sources;
  for (const auto* original : actual_order.value) {
    const auto& s=*original;
    auto valid = validate_budget_authorization_shape(s, state_.schema_version);
    if (!valid.ok)
      return valid;
    if (state_.schema_version == 2) {
      valid = policies_->validate_historical_terms(s);
      if (!valid.ok)
        return valid;
    }
    if (!sources.insert(s.approval_source.id).second ||
        s.identity.world_id != common_.state().identity.world_id ||
        s.identity.revision < 0 || s.identity.revision > common_.revision() ||
        s.identity.tick > s.approved_at.half_day_tick)
      return common_failure("InvalidBudgetApprovalIdentity", s.budget_id);
    auto actual = read_authority_issue_source(s.approval_source);
    if (!actual.ok)
      return {false, {}, actual.error};
    const auto it = previous.find(s.budget_id);
    if (it == previous.end()) {
      if (s.budget_revision != 1 || s.closed ||
          (s.decision != PublicBudgetDecisionKind::authorize &&
           s.decision != PublicBudgetDecisionKind::delegate))
        return common_failure("BudgetInitialRevisionMissing", s.budget_id);
    } else {
      const auto &before = *it->second;
      const auto old = ordinal(common_, before.approval_source),
                 now = ordinal(common_, s.approval_source);
      if (before.budget_revision == INT64_MAX ||
          s.budget_revision != before.budget_revision + 1 || before.closed ||
          !old || !now || *old >= *now ||
          !same_period(before.period, s.period) ||
          before.principal_org_id != s.principal_org_id ||
          (s.decision != PublicBudgetDecisionKind::amend &&
           s.decision != PublicBudgetDecisionKind::close))
        return common_failure("BudgetRevisionChainMismatch", s.budget_id);
    }
    if (it != previous.end()) {
      const auto &before = *it->second;
      if (bool(before.parent_budget_ref) != bool(s.parent_budget_ref) ||
          (s.parent_budget_ref && before.parent_budget_ref->authorization_id !=
                                      s.parent_budget_ref->authorization_id))
        return common_failure("BudgetParentCannotBeReassigned", s.budget_id);
      for (const auto &old : before.total_caps) {
        bool preserved = false;
        for (const auto &now : s.total_caps)
          if (same_unit(old.unit, now.unit))
            preserved = true;
        if (!preserved)
          return common_failure("BudgetUnitHistoryCannotBeRemoved",
                                s.budget_id);
      }
    }
    if (s.closed != (s.decision == PublicBudgetDecisionKind::close))
      return common_failure("BudgetCloseSourceMismatch", s.budget_id);
    auto registered = read_registered_authority_basis(
        common_, authority_bases_, organization_, organization_catalog_,
        catalog_identity_, s.authority_basis_ref, *this, this);
    if (!registered.ok)
      return {false, {}, registered.error};
    const auto &basis = *registered.value;
    if (basis.actor_id != s.issuer_actor_id ||
        basis.organization_id != s.principal_org_id ||
        basis.scope.kind != s.scope.kind || basis.scope.id != s.scope.id)
      return common_failure("BudgetAuthorityScopeMismatch", s.budget_id);
    if (basis.source_identity.world_id != s.identity.world_id ||
        basis.source_identity.execution_epoch != s.identity.execution_epoch ||
        basis.source_identity.revision != s.identity.revision ||
        basis.source_identity.tick != s.identity.tick)
      return common_failure("BudgetAuthorityIdentityMismatch", s.budget_id);
    if (bool(basis.budget_ref) != bool(s.parent_budget_ref))
      return common_failure("BudgetAuthorityParentMismatch", s.budget_id);
    if (s.parent_budget_ref &&
        (basis.budget_ref->authorization_id !=
             s.parent_budget_ref->authorization_id ||
         basis.budget_ref->authorization_revision !=
             s.parent_budget_ref->authorization_revision ||
         !same_source_ref(basis.budget_ref->approval_source,
                          s.parent_budget_ref->approval_source)))
      return common_failure("BudgetAuthorityParentMismatch", s.budget_id);
    std::set<StableId> resources;
    for (const auto &r : basis.resources) {
      if (r.kind != ResourceKey::Kind::wallet &&
          r.kind != ResourceKey::Kind::inventory)
        return common_failure("BudgetAuthorityResourceKindDenied", r.id);
      resources.insert(r.id);
    }
    if (resources !=
        std::set<StableId>(s.container_refs.begin(), s.container_refs.end()))
      return common_failure("BudgetAuthorityResourcesMismatch", s.budget_id);
    if (s.parent_budget_ref) {
      valid = validate_budget_authorization(*s.parent_budget_ref, basis);
      if (!valid.ok)
        return valid;
    }
    if (state_.schema_version == 2 && s.parent_budget_ref) {
      const auto *parent = find_budget_authorization(
          state_, s.parent_budget_ref->authorization_id,
          s.parent_budget_ref->authorization_revision);
      if (!parent)
        return common_failure("BudgetParentSourceMissing", s.budget_id);
      for (const auto &category : s.category_caps) {
        bool present = false;
        for (const auto &original : parent->category_caps)
          if (original.purpose == category.purpose &&
              same_unit(original.unit, category.unit))
            present = true;
        if (!present)
          return common_failure("BudgetParentCategoryDenied",
                                budget_execution_purpose_key(category.purpose));
      }
    }
    previous[s.budget_id] = &s;
  }
  for (const auto &entry : previous)
    for (const auto &limit : entry.second->total_caps) {
      auto allocated = read_budget_allocation(
          state_, entry.first, entry.second->budget_revision, limit.unit);
      if (!allocated.ok)
        return {false, {}, allocated.error};
    }
  if (state_.schema_version == 2)
    for (const auto &entry : previous)
      for (const auto &category : entry.second->category_caps) {
        auto allocation = read_budget_category_allocation(
            state_, entry.first, entry.second->budget_revision, category.unit,
            category.purpose);
        if (!allocation.ok)
          return {false, {}, allocation.error};
      }
  return common_success();
}
Outcome<BudgetAuthorizationSnapshot>
PoliticalBudgetRead::read_budget_authorization(const StableId &id,
                                               Integer revision,
                                               const BudgetPeriod &period,
                                               const BudgetUnit &unit) const {
  auto valid = validate_sources();
  if (!valid.ok)
    return {false, {}, valid.error};
  const auto *s = find_budget_authorization(state_, id, revision);
  if (!s || !same_period(s->period, period))
    return {false, {}, common_failure("BudgetPeriodMismatch", id).error};
  auto allocation = read_budget_allocation(state_, id, revision, unit);
  if (!allocation.ok)
    return {false, {}, allocation.error};
  auto basis = read_registered_authority_basis(
      common_, authority_bases_, organization_, organization_catalog_,
      catalog_identity_, s->authority_basis_ref, *this, this);
  if (!basis.ok)
    return {false, {}, basis.error};
  return {
      true,
      {id, revision, period, *basis.value, unit, allocation.value.retained_cap},
      {}};
}
Outcome<std::optional<Integer>> PoliticalBudgetRead::read_budget_purpose_cap(
    const StableId &id, Integer revision, const BudgetPeriod &period,
    const BudgetUnit &unit, BudgetExecutionPurpose purpose) const {
  auto total = read_budget_authorization(id, revision, period, unit);
  if (!total.ok)
    return {false, {}, total.error};
  const auto &source = *find_budget_authorization(state_, id, revision);
  if (state_.schema_version==1 && std::find(source.purposes.begin(), source.purposes.end(), purpose) ==
      source.purposes.end())
    return {false, {}, common_failure("BudgetPurposeDenied", id).error};
  if (state_.schema_version == 2) {
    auto allocation =
        read_budget_category_allocation(state_, id, revision, unit, purpose);
    if (!allocation.ok)
      return {false, {}, allocation.error};
    return {true, allocation.value.retained_cap, {}};
  }
  for (const auto &category : source.category_caps)
    if (category.purpose == purpose && same_unit(category.unit, unit))
      return {true, category.cap, {}};
  return {true, std::nullopt, {}};
}
Outcome<BudgetBucketAuthorization> PoliticalBudgetRead::read_budget_bucket_authorization(
 const StableId&id,Integer revision,const BudgetPeriod&period,const BudgetUnit&unit,BudgetExecutionPurpose purpose)const {
 auto valid=validate_sources();if(!valid.ok)return {false,{},valid.error};
 auto members=read_public_budget_bucket_members(common_,state_,id,revision,period,unit,purpose);
 if(!members.ok)return {false,{},members.error};const auto&v=members.value;
 const auto*root=find_budget_authorization(state_,v.root.authorization_id,v.root.authorization_revision);
 if(!root||!same_source_ref(root->approval_source,v.root.approval_source))return {false,{},common_failure("PublicBudgetBucketRootSourceRequired",id).error};
 BudgetBucketAuthorization out;out.identity=common_.state().identity;out.principal_org_id=v.principal_org_id;
 out.current_root_ref={root->budget_id,root->budget_revision,root->policy_revision,root->approval_source.id};
 out.period=v.period;out.unit=v.unit;out.purpose=v.purpose;out.root_total_cap=v.authorized_root_total_cap;out.root_purpose_cap=v.authorized_root_purpose_cap;
 out.root_accounted_purposes=std::move(members.value.root_accounted_purposes);
 out.current_member_budget_ids=std::move(members.value.current_root_member_budget_ids);
 out.inherited_member_budget_ids=std::move(members.value.inherited_root_member_budget_ids);
 return {true,std::move(out),{}};
}
Status PoliticalBudgetRead::validate_budget_execution_adoption(
    const BudgetExecutionBinding &b, BudgetExecutionCheck check) const {
  auto valid = validate_sources();
  if (!valid.ok)
    return valid;
  const auto *s = find_budget_authorization(state_, b.execution_ref.budget_id,
                                            b.execution_ref.budget_revision);
  if (!s || s->policy_revision != b.execution_ref.policy_revision ||
      s->approval_source.id != b.execution_ref.approval_fact_id ||
      !same_period(s->period, b.period) || s->closed ||
      std::find(s->container_refs.begin(), s->container_refs.end(),
                b.payer_container_id) == s->container_refs.end())
    return common_failure("BudgetExecutionApprovalMismatch",
                          b.execution_ref.budget_id);
  if (std::find(s->purposes.begin(), s->purposes.end(), b.purpose) ==
      s->purposes.end())
    return common_failure("BudgetPurposeDenied", s->budget_id);
  const auto approval = ordinal(common_, s->approval_source),
             accepted = ordinal(common_, b.responsibility_ref);
  if (!approval || !accepted || *approval >= *accepted ||
      b.accepted_at.half_day_tick < s->period.period_from_tick ||
      b.accepted_at.half_day_tick >= s->period.period_until_tick)
    return common_failure("BudgetAcceptanceSourceMismatch", s->budget_id);
  bool unit_found = false;
  for (const auto &cap : s->total_caps)
    if (same_unit(cap.unit, b.unit))
      unit_found = true;
  if (!unit_found)
    return common_failure("BudgetUnitNotAuthorized", s->budget_id);
  for (const auto &change : state_.authorization_history)
    if (change.budget_id == s->budget_id &&
        change.budget_revision > s->budget_revision) {
      const auto changed = ordinal(common_, change.approval_source);
      if (!changed || *changed < *accepted)
        return common_failure("BudgetWasNotCurrentAtAcceptance", s->budget_id);
    }
  // A closed/narrowed ancestor cannot issue new responsibilities through a
  // still-open child. Resolve each ancestor at this actual acceptance cut;
  // later closure must not invalidate an earlier genuine obligation.
  if(state_.schema_version==2){
    const BudgetAuthorizationSource* ancestor=s;std::set<StableId> ancestry;
    while(ancestor->parent_budget_ref){
      const auto parent_id=ancestor->parent_budget_ref->authorization_id;
      if(!ancestry.insert(parent_id).second)return common_failure("BudgetParentCycle",s->budget_id);
      const BudgetAuthorizationSource* at_acceptance=nullptr;
      for(const auto&row:state_.authorization_history)if(row.budget_id==parent_id){
        const auto index=ordinal(common_,row.approval_source);
        if(!index)return common_failure("BudgetApprovalSourceMissing",parent_id);
        if(*index<*accepted&&(!at_acceptance||row.budget_revision>at_acceptance->budget_revision))at_acceptance=&row;
      }
      if(!at_acceptance||at_acceptance->closed||!same_period(at_acceptance->period,b.period)||
         std::find(at_acceptance->purposes.begin(),at_acceptance->purposes.end(),b.purpose)==at_acceptance->purposes.end())
        return common_failure("BudgetAncestorDeniedAtAcceptance",s->budget_id);
      ancestor=at_acceptance;
    }
  }
  if (check == BudgetExecutionCheck::new_adoption) {
    auto now = world_.actual_phase_moment();
    if (!now.ok)
      return {false, {}, now.error};
    auto same = compare_world_moments(now.value, b.accepted_at);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value != 0)
      return common_failure("BudgetAdoptionMustUseActualAcceptanceMoment",
                            s->budget_id);
    if(state_.schema_version==2){
    const BudgetAuthorizationSource* ancestor=s;
    std::set<StableId> seen;
    while(ancestor->parent_budget_ref){
      if(!seen.insert(ancestor->budget_id).second)return common_failure("BudgetParentCycle",s->budget_id);
      ancestor=current_budget_authorization(state_,ancestor->parent_budget_ref->authorization_id);
      if(!ancestor||ancestor->closed||!same_period(ancestor->period,b.period)||
         std::find(ancestor->purposes.begin(),ancestor->purposes.end(),b.purpose)==ancestor->purposes.end())
        return common_failure("BudgetAncestorClosedForNewAdoption",s->budget_id);
    }
    }
    if (current_budget_authorization(state_, s->budget_id) != s ||
        now.value.half_day_tick < s->period.period_from_tick ||
        now.value.half_day_tick >= s->period.period_until_tick)
      return common_failure("BudgetPeriodClosed", s->budget_id);
  } else if (check != BudgetExecutionCheck::existing_source)
    return common_failure("InvalidBudgetExecutionCheck", s->budget_id);
  return common_success();
}
} // namespace hanmo::resident
