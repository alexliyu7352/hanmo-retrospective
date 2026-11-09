#include "treaty_ratification_authority.hpp"
#include <algorithm>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
bool actor(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch&&a.actor_id==b.actor_id;}
bool scope(const ScopeRef& a,const ScopeRef& b){return a.kind==b.kind&&a.id==b.id;}
bool resource(const ResourceKey& a,const ResourceKey& b){return a.kind==b.kind&&a.id==b.id;}
bool resources(const std::vector<ResourceKey>& a,const std::vector<ResourceKey>& b){
 if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!resource(a[i],b[i]))return false;return true;
}
bool request(const SingleGrantAuthorityRequest& a,const SingleGrantAuthorityRequest& b){
 return actor(a.acting,b.acting)&&a.command_kind==b.command_kind&&scope(a.scope,b.scope)&&a.amount==b.amount&&
  resources(a.resources,b.resources)&&a.requires_delegation==b.requires_delegation&&same_authority_amount_components(a.amount_components,b.amount_components);
}
bool has(const std::vector<StableId>& values,const StableId& id){return std::find(values.begin(),values.end(),id)!=values.end();}
std::vector<SingleGrantAuthorityRequest> requirements(const TreatyRatificationAuthorityRequirements& r,
 const StableId& party,const ActingAuthority& acting){
 if(!r.original_terms)fail("TreatyRatificationOriginalTermsRequired",party);
 take(validate_treaty_terms_shape(*r.original_terms));
 if(!has(r.original_terms->parties,party)||acting.organization_id!=party||!actor(r.primary.acting,acting)||
    r.primary.command_kind!="ratify_treaty"||r.term_requirements.empty())fail("TreatySignatoryScopeDenied",party);
 std::vector<SingleGrantAuthorityRequest> out;out.reserve(r.term_requirements.size());
 for(const auto& t:r.term_requirements){
  const auto term=std::find_if(r.original_terms->terms.begin(),r.original_terms->terms.end(),[&](const auto& x){return x.term_id==t.term_id;});
  if(term==r.original_terms->terms.end()||static_cast<std::size_t>(t.term_kind)!=term->terms.index()||
     !actor(t.request.acting,acting)||t.request.command_kind.empty()||t.request.scope.id.empty()||t.request.amount<0)
   fail("TreatyTermAuthorityRequirementMismatch",t.term_id);
  // The one persisted Basis must retain every actually requested resource;
// additional term checks are not an invisible unversioned resource back door.
  for(const auto& key:t.request.resources)if(std::none_of(r.primary.resources.begin(),r.primary.resources.end(),[&](const auto& p){return resource(key,p);}))
   fail("TreatyRatificationPrimaryResourceIncomplete",key.id);
  out.push_back(t.request);
 }
 for(const auto& term:r.original_terms->terms)if(std::none_of(r.term_requirements.begin(),r.term_requirements.end(),[&](const auto& t){return t.term_id==term.term_id;}))
  fail("TreatyTermSpecificAuthorityRequired",term.term_id);
 return out;
}
TreatyRatificationAuthorityRequirements original(const TreatyRatificationAuthorityContext& x,
 const SourceRef& terms,Integer revision,const StableId& party,const ActingAuthority& acting,const SourceRef* signature){
 if(!valid_source_ref(terms)||revision<1||party.empty()||acting.actor_id.empty()||acting.office_id.empty())fail("InvalidTreatyRatificationIdentity",party);
 return take(x.originals.read_original_ratification_requirements(terms,revision,party,acting,signature));
}
void covers(const TreatyRatificationAuthorityContext& x,const Grant& grant,const SingleGrantAuthorityRequest& r){
 if(grant.office_id!=r.acting.office_id||!has(grant.allowed_command_kinds,r.command_kind)||r.amount<0||r.amount>grant.amount_limit||
    (r.requires_delegation&&!grant.may_delegate)||std::none_of(grant.scopes.begin(),grant.scopes.end(),[&](const auto& s){return scope(s,r.scope);}))
  fail("TreatySignatoryScopeDenied",r.scope.id);
 for(const auto& key:r.resources){
  if(key.kind==ResourceKey::Kind::wallet){if(!has(grant.wallet_ids,key.id))fail("TreatySignatoryResourceDenied",key.id);}
  else if(key.kind==ResourceKey::Kind::inventory){if(!has(grant.inventory_ids,key.id))fail("TreatySignatoryResourceDenied",key.id);}
  else fail("TreatySignatoryResourceUnsupported",key.id);
 }
 if(!r.amount_components.empty()){
  if(r.amount!=0)fail("TreatyRatificationMixedAmountUnits");
  take(validate_authority_amount_components(x.common,r.amount_components,r.resources,grant.amount_limit));
 }
}
}
Outcome<TreatyRatificationAuthorityCapture> capture_treaty_ratification_authority(
 const TreatyRatificationAuthorityContext& x,const SourceRef& terms,Integer revision,const StableId& party,const ActingAuthority& acting){try{
 auto r=original(x,terms,revision,party,acting,nullptr);auto extra=requirements(r,party,acting);
 auto authority=take(check_current_grant_authority_requirements(x.common,x.organization,x.organization_catalog,x.identity,r.primary,extra,&x.originals));
 return {true,{std::move(r.primary),std::move(extra),std::move(authority)},{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_ratification_authority_capture(const TreatyRatificationAuthorityContext& x,
 const SourceRef& terms,Integer revision,const StableId& party,const ActingAuthority& acting,const TreatyRatificationAuthorityCapture& capture){try{
 auto r=original(x,terms,revision,party,acting,nullptr);auto extra=requirements(r,party,acting);
 if(!request(capture.primary,r.primary)||extra.size()!=capture.term_requirements.size())fail("TreatyRatificationAuthorityChanged",party);
 for(std::size_t i=0;i<extra.size();++i)if(!request(extra[i],capture.term_requirements[i]))fail("TreatyRatificationAuthorityChanged",party);
 take(validate_current_grant_authority_requirements(x.common,x.organization,x.organization_catalog,x.identity,r.primary,extra,capture.authority,&x.originals));
 return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<const AuthorityBasis*> read_treaty_ratification_authority(const TreatyRatificationAuthorityContext& x,const SourceRef& signature){try{
 auto signed_row=take(x.originals.read_actual_treaty_ratification(signature));
 if(!valid_source_ref(signature)||!same_source_ref(signature,signed_row.signature_source)||
    !same_source_ref(signature,signed_row.authority_basis.issued_source)||signed_row.authority_basis.source_schema!=1)
  fail("TreatyRatificationActualSourceMismatch",signature.id);
 const auto* basis=take(read_registered_authority_basis(x.common,x.authority_bases,x.organization,x.organization_catalog,x.identity,
  signed_row.authority_basis,x.originals,x.budgets));
 if(basis->issued_at.kind!=ActorFoundationAt::Kind::exact_moment||
    take(compare_world_moments(basis->issued_at.exact_moment,signed_row.occurred_at))!=0||
    basis->actor_id!=signed_row.acting.actor_id||basis->organization_id!=signed_row.party_id||
    basis->organization_id!=signed_row.acting.organization_id||basis->office_id!=signed_row.acting.office_id||basis->office_epoch!=signed_row.acting.authority_epoch)
  fail("TreatyRatificationAuthorityIdentityMismatch",signature.id);
 auto r=original(x,signed_row.terms_source,signed_row.terms_revision,signed_row.party_id,signed_row.acting,&signature);
 auto extra=requirements(r,signed_row.party_id,signed_row.acting);
 if(basis->command_kind!=r.primary.command_kind||!scope(basis->scope,r.primary.scope)||basis->amount!=r.primary.amount||
    !resources(basis->resources,r.primary.resources)||!same_authority_amount_components(basis->amount_components,r.primary.amount_components))
  fail("TreatyRatificationPrimaryBasisMismatch",signature.id);
 auto historical=take(read_historical_authority_grant(x.common,x.organization,x.organization_catalog,x.identity,*basis,x.originals,x.budgets));
 if(!historical.terms)fail("TreatyRatificationHistoricalGrantRequired",signature.id);
 covers(x,*historical.terms,r.primary);for(const auto& term:extra)covers(x,*historical.terms,term);
 return {true,basis,{}};
 }catch(const Failure& f){return {false,nullptr,f.error};}}
}
