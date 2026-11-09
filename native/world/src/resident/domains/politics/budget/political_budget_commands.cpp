#include "political_budget_commands.hpp"
#include "political_budget_bucket.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
#include <set>
#include <type_traits>
namespace hanmo::resident {
namespace {
class BudgetWorldClock final : public CurrentMomentRead {
public:
  explicit BudgetWorldClock(const WorldBundle &w) : world_(w) {}
  Outcome<WorldMoment> read_current_moment() const override {
    return world_.actual_phase_moment();
  }

private:
  const WorldBundle &world_;
};
bool same_unit(const BudgetUnit &a, const BudgetUnit &b) {
  if (a.index() != b.index())
    return false;
  const auto *x = std::get_if<BudgetItemQuantity>(&a);
  return !x || x->item_id == std::get<BudgetItemQuantity>(b).item_id;
}
const PublicBudgetLimit *
find_limit(const std::vector<PublicBudgetLimit> &limits,
           const BudgetUnit &unit) {
  for (const auto &x : limits)
    if (same_unit(x.unit, unit))
      return &x;
  return nullptr;
}
bool add(Integer &a, Integer b) {
  if (b < 0 || a > INT64_MAX - b)
    return false;
  a += b;
  return true;
}
PublicBudgetTerms terms_of(const BudgetAuthorizationSource &s) {
  return {s.policy_id,      s.policy_revision, s.period,     s.scope,
          s.container_refs, s.purposes,        s.total_caps, s.category_caps};
}
Outcome<Integer> root_inherited_total(const BudgetUsageRead&usage,const WorldIdentity&identity,const BudgetAuthorizationSource&root,const BudgetUnit&unit){
 // This selects only an identical total projection, never a spending purpose,
 // payer, policy or resource. Every category reports the same inherited total.
 for(const auto&category:root.category_caps)if(same_unit(category.unit,unit)){
  auto view=usage.read_budget_bucket_usage({root.budget_id,root.budget_revision,root.period,unit,identity,0,64},category.purpose);
  if(!view.ok)return {false,0,view.error};Integer amount=view.value.total_inherited_spent_gross;
  if(!add(amount,view.value.total_inherited_committed_unpaid))return {false,0,common_failure("BudgetCapacityOverflow",root.budget_id).error};
  return {true,amount,{}};
 }
 return {false,0,common_failure("BudgetRootCategoryRequired",root.budget_id).error};
}
const char *command_kind(PublicBudgetDecisionKind k) {
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
  return "";
}
const char *fact_kind(PublicBudgetDecisionKind k) {
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
  return "";
}
} // namespace
Outcome<BudgetAuthorizationRef> apply_public_budget_in_candidate(
    CommonStore &common, PoliticalBudgetState &state,
    const PublicBudgetWriteContext &context, const StableId &actor,
    const PublicBudgetCommand &command) {
  if (&context.world.common != &common ||
      &context.world.organization != &context.organization ||
      !context.policies.bound_to(context.world) ||
      !context.policies.bound_to_budget(state))
    return {false, {}, common_failure("BudgetWriteCandidateMismatch").error};
  if (state.schema_version != 2 || context.executions.schema_version != 2)
    return {
        false, {}, common_failure("PublicBudgetPairedVersion2Required").error};
  if (state.authorization_history.size() >= political_budget_history_limit)
    return {false,
            {},
            common_failure("CheckpointCollectionTooLarge",
                           "political_budgets/authorization_history")
                .error};
  PoliticalBudgetRead read(context.world, state, context.authority_bases,
                           context.organization, context.organization_catalog,
                           context.catalog_identity, &context.policies);
  auto valid = read.validate_sources();
  if (!valid.ok)
    return {false, {}, valid.error};
  BudgetWorldClock clock(context.world);
  BudgetUsageRead usage(common, context.executions, read,
                        context.responsibilities, &clock);
  return std::visit(
      [&](const auto &cmd) -> Outcome<BudgetAuthorizationRef> {
        using C = std::decay_t<decltype(cmd)>;
        if (actor.empty() || cmd.acting.actor_id != actor)
          return {
              false, {}, common_failure("BudgetCommandActorMismatch").error};
        constexpr auto kind = std::is_same_v<C, AuthorizePublicBudget>
                                  ? PublicBudgetDecisionKind::authorize
                              : std::is_same_v<C, DelegatePublicBudget>
                                  ? PublicBudgetDecisionKind::delegate
                              : std::is_same_v<C, AmendPublicBudget>
                                  ? PublicBudgetDecisionKind::amend
                                  : PublicBudgetDecisionKind::close;
        const BudgetAuthorizationSource *previous = nullptr;
        PublicBudgetTerms terms;
        std::optional<BudgetAuthorizationRef> parent;
        if constexpr (std::is_same_v<C, AuthorizePublicBudget> ||
                      std::is_same_v<C, DelegatePublicBudget>) {
          terms = cmd.terms;
          if constexpr (std::is_same_v<C, DelegatePublicBudget>)
            parent = cmd.parent;
        } else {
          previous = current_budget_authorization(state, cmd.budget_id);
          if (!previous || previous->budget_revision != cmd.expected_revision ||
              previous->budget_revision == INT64_MAX || previous->closed)
            return {false,
                    {},
                    common_failure("StaleBudgetAuthorization", cmd.budget_id)
                        .error};
          if (previous->principal_org_id != cmd.acting.organization_id)
            return {
                false,
                {},
                common_failure("BudgetPrincipalMismatch", cmd.budget_id).error};
          parent = previous->parent_budget_ref;
          if constexpr (std::is_same_v<C, AmendPublicBudget>)
            terms = cmd.terms;
          else
            terms = terms_of(*previous);
          if (terms.period.period_from_tick !=
                  previous->period.period_from_tick ||
              terms.period.period_until_tick !=
                  previous->period.period_until_tick)
            return {false,
                    {},
                    common_failure("BudgetPeriodRequiresNewAuthorization",
                                   cmd.budget_id)
                        .error};
          for (const auto &old : previous->total_caps)
            if (!find_limit(terms.total_caps, old.unit))
              return {false,
                      {},
                      common_failure("BudgetUnitHistoryCannotBeRemoved",
                                     cmd.budget_id)
                          .error};
          for (auto &cap : terms.total_caps) {
            if (!find_limit(previous->total_caps, cap.unit))
              continue;
            auto real = usage.read_budget_usage(
                {previous->budget_id, previous->budget_revision,
                 previous->period, cap.unit, common.state().identity, 0, 64});
            if (!real.ok)
              return {false, {}, real.error};
            auto allocated =
                read_budget_allocation(state, previous->budget_id,
                                       previous->budget_revision, cap.unit);
            if (!allocated.ok)
              return {false, {}, allocated.error};
            Integer occupied = real.value.spent_gross;
            if (!add(occupied, real.value.committed_unpaid) ||
                !add(occupied, allocated.value.delegated_cap))
              return {
                  false, {}, common_failure("BudgetCapacityOverflow").error};
            if(!previous->parent_budget_ref){auto inherited=root_inherited_total(usage,common.state().identity,*previous,cap.unit);if(!inherited.ok)return {false,{},inherited.error};if(!add(occupied,inherited.value))return {false,{},common_failure("BudgetCapacityOverflow",previous->budget_id).error};}
            if constexpr (std::is_same_v<C, ClosePublicBudget>)
              cap.cap = occupied;
            else if (cap.cap < occupied)
              return {false,
                      {},
                      common_failure("BudgetCapacityBelowExistingLiability",
                                     previous->budget_id)
                          .error};
          }
          // Preserve every historical category even when its purpose ceases
          // to accept new obligations. Its gross/delegations remain occupied.
          for(const auto&old:previous->category_caps)
            if(std::none_of(terms.category_caps.begin(),terms.category_caps.end(),[&](const auto&v){return v.purpose==old.purpose&&same_unit(v.unit,old.unit);})) {
              if(std::find(terms.purposes.begin(),terms.purposes.end(),old.purpose)!=terms.purposes.end())
                return {false,{},common_failure("BudgetActiveCategoryRequired",previous->budget_id).error};
              terms.category_caps.push_back(old);
            }
          for (auto &category : terms.category_caps) {
            const auto *total = find_limit(terms.total_caps, category.unit);
            if (!total)
              return {
                  false, {}, common_failure("BudgetCategoryUnitMissing").error};
            auto real = usage.read_budget_usage_for_purpose(
                {previous->budget_id, previous->budget_revision,
                 previous->period, category.unit, common.state().identity, 0,
                 64},
                category.purpose);
            if (!real.ok)
              return {false, {}, real.error};
            auto allocated = read_budget_category_allocation(
                state, previous->budget_id, previous->budget_revision,
                category.unit, category.purpose);
            if (!allocated.ok)
              return {false, {}, allocated.error};
            Integer occupied = real.value.spent_gross;
            if (!add(occupied, real.value.committed_unpaid) ||
                !add(occupied, allocated.value.delegated_cap))
              return {
                  false, {}, common_failure("BudgetCapacityOverflow").error};
            if(!previous->parent_budget_ref){
              auto inherited=usage.read_budget_bucket_usage({previous->budget_id,previous->budget_revision,previous->period,category.unit,common.state().identity,0,64},category.purpose);
              if(!inherited.ok)return {false,{},inherited.error};
              if(!add(occupied,inherited.value.inherited_spent_gross)||!add(occupied,inherited.value.inherited_committed_unpaid))return {false,{},common_failure("BudgetCapacityOverflow",previous->budget_id).error};
            }
            if constexpr (std::is_same_v<C, ClosePublicBudget>)
              category.cap = occupied;
            if (category.cap > total->cap || category.cap < occupied)
              return {false,
                      {},
                      common_failure("BudgetCategoryBelowExistingLiability",
                                     previous->budget_id)
                          .error};
          }
        }
        if constexpr(std::is_same_v<C,AuthorizePublicBudget>){
          auto roots=read_public_budget_reopening_roots(common,state,cmd.acting.organization_id,terms.period,terms.purposes);
          if(!roots.ok)return {false,{},roots.error};
          // Each purpose/unit uses its latest original closed root projection;
          // that projection already inherits earlier roots, so never sum those
          // historical snapshots again and double-count their actual legs.
          std::vector<std::pair<PublicBudgetCategoryLimit,BudgetAuthorizationRef>> retired;
          for(const auto&ref:roots.value){const auto*old=find_budget_authorization(state,ref.authorization_id,ref.authorization_revision);
            for(const auto&category:old->category_caps){
              if(!find_limit(terms.total_caps,category.unit))return {false,{},common_failure("BudgetReopeningUnitHistoryRequired",ref.authorization_id).error};
              if(std::find(terms.purposes.begin(),terms.purposes.end(),category.purpose)!=terms.purposes.end())continue;
              auto found=std::find_if(retired.begin(),retired.end(),[&](const auto&v){return v.first.purpose==category.purpose&&same_unit(v.first.unit,category.unit);});
              if(found==retired.end())retired.push_back({category,ref});else *found={category,ref};
            }
          }
          for(const auto&entry:retired){const auto&category=entry.first;const auto&ref=entry.second;
            auto occupied=usage.read_budget_bucket_usage({ref.authorization_id,ref.authorization_revision,terms.period,category.unit,common.state().identity,0,64},category.purpose);
            if(!occupied.ok)return {false,{},occupied.error};Integer retained=occupied.value.spent_gross;
            if(!add(retained,occupied.value.committed_unpaid))return {false,{},common_failure("BudgetCapacityOverflow",ref.authorization_id).error};
            auto found=std::find_if(terms.category_caps.begin(),terms.category_caps.end(),[&](const auto&v){return v.purpose==category.purpose&&same_unit(v.unit,category.unit);});
            if(found==terms.category_caps.end())terms.category_caps.push_back({category.purpose,category.unit,retained});
            else if(found->cap<retained)return {false,{},common_failure("BudgetRetiredPurposeBelowExistingLiability",ref.authorization_id).error};
          }
        }
        auto policy =
            context.policies.validate_approval_transition_terms(cmd.acting, kind, terms,previous);
        if (!policy.ok)
          return {false, {}, policy.error};
        if (terms.total_caps.empty() || terms.period.period_from_tick < 0 ||
            terms.period.period_until_tick <= terms.period.period_from_tick)
          return {false, {}, common_failure("InvalidBudgetTerms").error};
        if (parent) {
          const auto *p =
              current_budget_authorization(state, parent->authorization_id);
          if (!p || p->closed ||
              (!previous && (p->budget_revision != parent->authorization_revision ||
              !same_source_ref(p->approval_source, parent->approval_source))) ||
              p->period.period_from_tick != terms.period.period_from_tick ||
              p->period.period_until_tick != terms.period.period_until_tick)
            return {false,
                    {},
                    common_failure("StaleParentBudgetAuthorization").error};
          // Amendments keep the same parent identity, but pin the actual
          // current parent revision authorizing this new decision. Older
          // child revisions retain their original immutable historical pin.
          if(previous)parent=BudgetAuthorizationRef{p->budget_id,p->budget_revision,p->approval_source};
          for (const auto &cap : terms.total_caps) {
            Integer prior = 0;
            if (previous)
              if (const auto *old = find_limit(previous->total_caps, cap.unit))
                prior = old->cap;
            if (cap.cap <= prior)
              continue;
            auto available = usage.read_budget_usage(
                {p->budget_id, p->budget_revision, p->period, cap.unit,
                 common.state().identity, 0, 64});
            if (!available.ok)
              return {false, {}, available.error};
            Integer allowance=available.value.available_to_commit;
            if(!p->parent_budget_ref){auto inherited=root_inherited_total(usage,common.state().identity,*p,cap.unit);if(!inherited.ok)return {false,{},inherited.error};allowance=inherited.value>=allowance?0:allowance-inherited.value;}
            if (cap.cap - prior > allowance)
              return {false,
                      {},
                      common_failure("BudgetDelegationCapacityExceeded",
                                     p->budget_id)
                          .error};
          }
          for (const auto &category : terms.category_caps) {
            Integer prior = 0;
            if (previous)
              for (const auto &old : previous->category_caps)
                if (old.purpose == category.purpose &&
                    same_unit(old.unit, category.unit))
                  prior = old.cap;
            if (category.cap <= prior)
              continue;
            if(std::find(p->purposes.begin(),p->purposes.end(),category.purpose)==p->purposes.end())
              return {false,{},common_failure("BudgetParentPurposeClosedForIncrease",p->budget_id).error};
            auto available = usage.read_budget_usage_for_purpose(
                {p->budget_id, p->budget_revision, p->period, category.unit,
                 common.state().identity, 0, 64},
                category.purpose);
            if (!available.ok)
              return {false, {}, available.error};
            // Category allocation is an independent ceiling. Its free
            // capacity must not be replaced by min(total,purpose) spending
            // availability: an already reserved child's total need not grow
            // merely to revise its purpose distribution.
            auto category_room=read_budget_category_allocation(state,p->budget_id,p->budget_revision,category.unit,category.purpose);
            if(!category_room.ok)return {false,{},category_room.error};
            Integer own_occupied=available.value.spent_gross;if(!add(own_occupied,available.value.committed_unpaid))return {false,{},common_failure("BudgetCapacityOverflow",p->budget_id).error};
            Integer allowance=own_occupied>=category_room.value.retained_cap?0:category_room.value.retained_cap-own_occupied;
            if(!p->parent_budget_ref){auto inherited=usage.read_budget_bucket_usage({p->budget_id,p->budget_revision,p->period,category.unit,common.state().identity,0,64},category.purpose);if(!inherited.ok)return {false,{},inherited.error};Integer amount=inherited.value.inherited_spent_gross;if(!add(amount,inherited.value.inherited_committed_unpaid))return {false,{},common_failure("BudgetCapacityOverflow",p->budget_id).error};allowance=amount>=allowance?0:allowance-amount;}
            if (category.cap - prior > allowance)
              return {
                  false,
                  {},
                  common_failure("BudgetDelegationCategoryCapacityExceeded",
                                 budget_execution_purpose_key(category.purpose))
                      .error};
          }
        }
        SingleGrantAuthorityRequest request;
        request.acting = cmd.acting;
        request.command_kind = command_kind(kind);
        request.scope = terms.scope;
        request.requires_delegation =
            kind == PublicBudgetDecisionKind::delegate;
        std::set<std::pair<ResourceKey::Kind, StableId>> resources;
        for (const auto &cap : terms.total_caps) {
          if (std::holds_alternative<BudgetMoneyQian>(cap.unit)) {
            request.amount_components.push_back(
                MoneyAuthorityAmount{"qian", cap.container_refs, cap.cap});
            for (const auto &id : cap.container_refs)
              resources.emplace(ResourceKey::Kind::wallet, id);
          } else {
            request.amount_components.push_back(ItemAuthorityAmount{
                std::get<BudgetItemQuantity>(cap.unit).item_id,
                cap.container_refs, cap.cap});
            for (const auto &id : cap.container_refs)
              resources.emplace(ResourceKey::Kind::inventory, id);
          }
        }
        for (const auto &resource : resources)
          request.resources.push_back({resource.first, resource.second});
        auto authority = check_current_grant_authority(
            common, context.organization, context.organization_catalog,
            context.catalog_identity, request, &read);
        if (!authority.ok)
          return {false, {}, authority.error};
        auto at = context.world.actual_phase_moment();
        if (!at.ok)
          return {false, {}, at.error};
        StableId budget_id;
        Integer revision = 1;
        if (previous) {
          budget_id = previous->budget_id;
          revision = previous->budget_revision + 1;
        } else {
          auto id = common.next_id("public_budget");
          if (!id.ok)
            return {false, {}, id.error};
          budget_id = id.value;
        }
        auto fact = common.next_id("public_budget_decision");
        if (!fact.ok)
          return {false, {}, fact.error};
        BudgetAuthorizationSource source;
        source.identity = common.state().identity;
        source.budget_id = budget_id;
        source.budget_revision = revision;
        source.policy_id = terms.policy_id;
        source.policy_revision = terms.policy_revision;
        source.principal_org_id = cmd.acting.organization_id;
        source.period = terms.period;
        source.scope = terms.scope;
        source.container_refs = terms.container_refs;
        source.purposes = terms.purposes;
        source.total_caps = std::move(terms.total_caps);
        source.category_caps = std::move(terms.category_caps);
        source.parent_budget_ref = parent;
        source.approval_source = {"politics", fact_kind(kind), fact.value};
        source.approved_at = std::move(at.value);
        source.decision = kind;
        source.closed = kind == PublicBudgetDecisionKind::close;
        // builder反读的actual
        // issue仅借原command的真实actor与已写批准terms；不拿尚未生成Basis自证cap。
        source.issuer_actor_id = actor;
        source.authority_basis_ref = {source.approval_source, 1};
        auto appended =
            common.append_fact({fact.value, fact_kind(kind), "politics"});
        if (!appended.ok)
          return {false, {}, appended.error};
        state.authorization_history.push_back(std::move(source));
        auto &adopted = state.authorization_history.back();
        auto basis = register_authority_basis_for_fact(
            common, context.authority_bases, context.organization,
            context.organization_catalog, context.catalog_identity, request,
            authority.value, adopted.approval_source, read, parent, &read);
        if (!basis.ok)
          return {false, {}, basis.error};
        adopted.authority_basis_ref = std::move(basis.value);
        auto checked = read.validate_sources();
        if (!checked.ok)
          return {false, {}, checked.error};
        auto capacity=validate_public_budget_bucket_usage(common,state,usage);
        if(!capacity.ok)return {false,{},capacity.error};
        return {true, {budget_id, revision, adopted.approval_source}, {}};
      },
      command);
}
} // namespace hanmo::resident
