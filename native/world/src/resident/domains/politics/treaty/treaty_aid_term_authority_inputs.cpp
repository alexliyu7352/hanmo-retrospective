#include "treaty_aid_term_authority_inputs.hpp"
#include "treaty_aid_source_exposure.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace hanmo::resident { namespace {
struct Failure { RuleError error; };
[[noreturn]] void fail(const char *code, const StableId &id = {}) { throw Failure{common_failure(code,id).error}; }
template<class T> T take(Outcome<T> r) { if(!r.ok) throw Failure{std::move(r.error)}; return std::move(r.value); }
void take(Status r) { if(!r.ok) throw Failure{std::move(r.error)}; }
bool same_actor(const ActingAuthority &a,const ActingAuthority &b) {
  return a.actor_id==b.actor_id && a.organization_id==b.organization_id && a.office_id==b.office_id && a.authority_epoch==b.authority_epoch;
}
bool same_revision(const TreatyRevisionView &a,const TreatyRevisionView &b) {
  return a.definition==b.definition && a.treaty_id==b.treaty_id && a.terms_revision==b.terms_revision && same_source_ref(a.source,b.source);
}
void resource(std::vector<ResourceKey> &out,const ResourceKey &r) {
  if(std::none_of(out.begin(),out.end(),[&](const auto &v){return v.kind==r.kind && v.id==r.id;})) out.push_back(r);
}
void merge(std::vector<AuthorityAmountComponent> &out,const AuthorityAmountComponent &add) {
  std::visit([&](const auto &row) {
    using T=std::decay_t<decltype(row)>;
    const auto &unit=[&]() -> const StableId& { if constexpr(std::is_same_v<T,MoneyAuthorityAmount>)return row.currency_id;else return row.item_id; }();
    T *target=nullptr;
    for(auto &v:out) if(auto *existing=std::get_if<T>(&v)) {
      const auto &key=[&]() -> const StableId& { if constexpr(std::is_same_v<T,MoneyAuthorityAmount>)return existing->currency_id;else return existing->item_id; }();
      if(key==unit){target=existing;break;}
    }
    if(!target){out.push_back(row);return;}
    if(row.amount<0 || target->amount>std::numeric_limits<Integer>::max()-row.amount)fail("TreatyAidCombinedExposureOverflow",unit);
    target->amount+=row.amount;
    auto &ids=[&]() -> std::vector<StableId>& {if constexpr(std::is_same_v<T,MoneyAuthorityAmount>)return target->wallet_ids;else return target->inventory_ids;}();
    const auto &added=[&]() -> const std::vector<StableId>& {if constexpr(std::is_same_v<T,MoneyAuthorityAmount>)return row.wallet_ids;else return row.inventory_ids;}();
    for(const auto &id:added)if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);
    std::sort(ids.begin(),ids.end());
  },add);
}
} // namespace
Outcome<TreatyAidTermAuthorityInputs> read_treaty_aid_term_authority_inputs(
    const TreatyResourceSourceLinks2 &x,const TreatyAuthorityMappingCatalog &mapping,
    const TreatyRevisionView &revision,const StableId &party,const ActingAuthority &acting,
    const SourceRef *boundary) {
  try {
    const auto &r=x.revisions;
    if(acting.actor_id.empty() || acting.office_id.empty() || acting.organization_id!=party)
      fail("TreatyAidTermAuthorityPartyMismatch",party);
    auto original=take(read_treaty_revision_source(r,revision.source,1));
    if(!same_revision(original,revision) || !original.definition)fail("TreatyAidTermAuthorityOriginalRevisionRequired",revision.treaty_id);
    auto author=take(read_treaty_authored_authority_mapping(mapping,r.terms_catalog,original,party));
    TreatyAidTermAuthorityInputs out;
    if(boundary) {
      out.actual_signature=take(read_treaty_resource_signature2_links(x,{*boundary,2}));
      const auto &s=out.actual_signature->signature;
      const auto *direct=std::get_if<TreatyDirectOfficeSigner>(&s.signer);
      if(!direct || !same_actor(direct->authority,acting) || s.party_id!=party || s.treaty_id!=original.treaty_id || s.terms_revision!=original.terms_revision || !same_source_ref(s.terms_source,original.source) || !out.actual_signature->actual_aid_resource_proposal)
        fail("TreatyAidTermAuthoritySignatureMismatch",boundary->id);
      out.proposal=take(read_treaty_aid_resource_proposal2_links(x,*out.actual_signature->actual_aid_resource_proposal));
      out.authority_at=s.signed_at;
    } else {
      auto current=take(read_current_treaty_revision(r,original.treaty_id));
      if(!same_revision(current,original))fail("TreatyTermsChanged",original.treaty_id);
      for(const auto &entry:x.proposals.rows())if(entry.second.treaty_id==original.treaty_id && entry.second.terms_revision==original.terms_revision) {
        if(out.proposal)fail("DuplicateTreatyAidResourceProposal",original.treaty_id);
        out.proposal=take(read_treaty_aid_resource_proposal2_links(x,{treaty_aid_resource_proposal2_source(entry.second),2}));
      }
      if(!out.proposal)fail("TreatyAidOriginalSelectionRequired",original.treaty_id);
      out.authority_at=take(r.clock.read_organization_authority_now());
      if(take(compare_world_moments(out.proposal->proposed_at,out.authority_at))>0)fail("TreatyAidProposalInFuture",out.proposal->id);
    }
    take(validate_treaty_aid_resource_proposal2_terms(*out.proposal,original));
    auto &required=out.requirements;required.original_terms=&original.definition->terms;
    required.primary.acting=acting;required.primary.command_kind="ratify_treaty";required.primary.scope=author->ratification_scope;
    for(const auto &term:original.definition->terms.terms) {
      const auto *aid=std::get_if<TreatyMaterialAidTerm>(&term.terms);
      if(!aid)fail("TreatyAidAuthorityTermSelectionUnsupported",term.term_id);
      const auto a=std::find_if(author->terms.begin(),author->terms.end(),[&](const auto &v){return v.term_id==term.term_id;});
      if(a==author->terms.end() || a->term_kind!=TreatyTermKind::material_aid)fail("TreatyAidAuthoredTermRequired",term.term_id);
      const auto choice=std::find_if(out.proposal->selections.begin(),out.proposal->selections.end(),[&](const auto &v){return v.aid.term_id==term.term_id;});
      if(choice==out.proposal->selections.end())fail("TreatyAidOriginalSelectionRequired",term.term_id);
      TreatyTermAuthorityRequirement item;item.term_id=term.term_id;item.term_kind=TreatyTermKind::material_aid;
      item.request.acting=acting;item.request.command_kind=a->command_kind;item.request.scope=a->scope;
      if(aid->source_party==party) {
        auto exposure=take(read_treaty_aid_source_exposure(r.common,term,*choice));
        item.request.resources=std::move(exposure.source_resources);item.request.amount_components=std::move(exposure.amount_components);
        for(const auto &v:item.request.resources)resource(required.primary.resources,v);
        for(const auto &v:item.request.amount_components)merge(required.primary.amount_components,v);
      }
      required.term_requirements.push_back(std::move(item));
    }
    std::sort(required.primary.resources.begin(),required.primary.resources.end(),[](const auto&a,const auto&b){return a.kind!=b.kind?a.kind<b.kind:a.id<b.id;});
    std::sort(required.primary.amount_components.begin(),required.primary.amount_components.end(),[](const auto&a,const auto&b){
      if(a.index()!=b.index())return a.index()<b.index();
      if(const auto*m=std::get_if<MoneyAuthorityAmount>(&a))return m->currency_id<std::get<MoneyAuthorityAmount>(b).currency_id;
      return std::get<ItemAuthorityAmount>(a).item_id<std::get<ItemAuthorityAmount>(b).item_id;
    });
    if(!required.primary.amount_components.empty())take(validate_authority_amount_components(r.common,required.primary.amount_components,required.primary.resources,std::numeric_limits<Integer>::max()));
    return {true,std::move(out),{}};
  } catch(const Failure &error){return {false,{},error.error};}
    catch(const std::bad_alloc &){return {false,{},common_failure("TreatyAidAuthorityResourceExhausted").error};}
    catch(const std::length_error &){return {false,{},common_failure("TreatyAidAuthorityResourceExhausted").error};}
}
} // namespace hanmo::resident
