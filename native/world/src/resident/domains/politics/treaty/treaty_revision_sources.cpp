#include "treaty_revision_sources.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
bool same_actor(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch&&a.actor_id==b.actor_id;}
bool actor_shape(const ActingAuthority& a){return !a.actor_id.empty()&&!a.organization_id.empty()&&!a.office_id.empty()&&a.authority_epoch>=0;}
std::size_t position(const CommonStore& c,const SourceRef& ref){
 const auto* head=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();const auto i=std::find(order.begin(),order.end(),ref.id);
 if(!head||head->domain!=ref.domain||head->kind!=ref.kind||i==order.end())fail("TreatyRevisionOriginalFactMissing",ref.id);
 return static_cast<std::size_t>(i-order.begin());
}
const TreatyTermsDefinition& definition(const TreatyRevisionContext& x,const StableId& id){
 const auto* r=x.terms_catalog.definitions.find(id);if(!r||r->id!=id)fail("TreatyTermsDefinitionMissing",id);
 take(validate_treaty_terms_shape(r->terms));return *r;
}
void source_time(const TreatyRevisionContext& x,const WorldMoment& at){take(encode_world_moment(at));if(compare(at,take(x.clock.read_organization_authority_now()))>0)fail("TreatyRevisionSourceInFuture");}
TreatyRevisionView raw(const TreatyRevisionContext& x,const SourceRef& ref){
 if(!valid_source_ref(ref)||ref.domain!="politics")fail("TreatyRevisionSourceUnsupported",ref.id);
 TreatyRevisionView out;out.source=ref;
 if(ref.kind=="treaty_terms_offered"){
  const auto* r=x.offers.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyOfferSourceMissing",ref.id);take(validate_treaty_offer_source_shape(*r));
  out.treaty_id=r->id;out.terms_revision=1;out.definition=&definition(x,r->terms_definition_id);out.issuer=&r->issuer;out.issued_at=r->issued_at;
  if(r->offering_party!=r->issuer.organization_id)fail("TreatyRevisionPartyMismatch",r->id);
 }else if(ref.kind=="treaty_terms_counteroffered"){
  const auto* r=x.counteroffers.find(ref.id);if(!r||r->id!=ref.id)fail("TreatyCounterofferSourceMissing",ref.id);take(validate_treaty_counteroffer_source_shape(*r));
  out.treaty_id=r->treaty_id;out.terms_revision=r->terms_revision;out.definition=&definition(x,r->terms_definition_id);out.issuer=&r->issuer;out.issued_at=r->issued_at;
  if(r->offering_party!=r->issuer.organization_id)fail("TreatyRevisionPartyMismatch",r->id);
 }else fail("TreatyRevisionSourceUnsupported",ref.id);
 position(x.common,ref);source_time(x,out.issued_at);
 if(std::find(out.definition->terms.parties.begin(),out.definition->terms.parties.end(),out.issuer->organization_id)==out.definition->terms.parties.end())fail("TreatyRevisionPartyNotInTerms",ref.id);
 return out;
}
SingleGrantAuthorityRequest request(const TreatyRevisionContext& x,const TreatyRevisionView& r,const SourceRef* boundary){
 const bool counter=r.terms_revision>1;auto q=take(x.originals.read_treaty_revision_authority_request(*r.definition,r.issuer->organization_id,*r.issuer,counter,boundary));
 if(!same_actor(q.acting,*r.issuer)||q.command_kind!=(counter?"counter_treaty_terms":"propose_treaty_terms"))fail("TreatyRevisionOriginalAuthorityMismatch",r.source.id);
 return q;
}
void basis(const TreatyRevisionContext& x,const TreatyRevisionView& r){
 NativeTreatyRevisionIssueRead source(x);auto q=request(x,r,&r.source);
 const auto* b=take(read_registered_authority_basis(x.common,x.authority_bases,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,{r.source,1},source,x.budgets));
 if(b->actor_id!=r.issuer->actor_id||b->organization_id!=r.issuer->organization_id||b->office_id!=r.issuer->office_id||b->office_epoch!=r.issuer->authority_epoch||b->command_kind!=q.command_kind||b->scope.kind!=q.scope.kind||b->scope.id!=q.scope.id||b->amount!=q.amount||b->resources.size()!=q.resources.size()||!same_authority_amount_components(b->amount_components,q.amount_components)||b->issued_at.kind!=ActorFoundationAt::Kind::exact_moment||compare(b->issued_at.exact_moment,r.issued_at))fail("TreatyRevisionAuthorityBasisMismatch",r.source.id);
 for(std::size_t i=0;i<q.resources.size();++i)if(q.resources[i].kind!=b->resources[i].kind||q.resources[i].id!=b->resources[i].id)fail("TreatyRevisionAuthorityResourceMismatch",r.source.id);
 if(q.requires_delegation){auto grant=take(read_historical_authority_grant(x.common,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,*b,source,x.budgets));if(!grant.terms||!grant.terms->may_delegate)fail("TreatyRevisionDelegationDenied",r.source.id);}
}
std::vector<TreatyRevisionView> chain(const TreatyRevisionContext& x,const StableId& treaty){
 const auto* offer=x.offers.find(treaty);if(!offer)fail("TreatyOfferSourceMissing",treaty);
 std::vector<TreatyRevisionView> out;out.push_back(raw(x,treaty_offer_source(*offer)));basis(x,out.back());
 std::vector<const TreatyCounterofferSource*> rows;for(const auto& row:x.counteroffers.rows())if(row.second.treaty_id==treaty){if(row.first!=row.second.id)fail("TreatyCounterofferIdentityMismatch",row.first);rows.push_back(&row.second);}
 std::sort(rows.begin(),rows.end(),[&](const auto* a,const auto* b){return position(x.common,treaty_counteroffer_source(*a))<position(x.common,treaty_counteroffer_source(*b));});
 for(const auto* row:rows){auto next=raw(x,treaty_counteroffer_source(*row));const auto& previous=out.back();
  if(previous.terms_revision==std::numeric_limits<Integer>::max()||next.terms_revision!=previous.terms_revision+1||!same_source_ref(row->previous_terms_source,previous.source)||position(x.common,previous.source)>=position(x.common,next.source)||compare(previous.issued_at,next.issued_at)>0)fail("TreatyCounterofferRevisionChainMismatch",row->id);
  take(x.originals.validate_counteroffer_transition(previous,*next.definition,row->offering_party,row->issuer,row->issued_at,&next.source));basis(x,next);out.push_back(std::move(next));
 }
 return out;
}
void candidate(CommonStore& c,AuthorityBasisRegistry& bases,const TreatyRevisionContext& x){
 if(&c!=&x.common||&bases!=&x.authority_bases||!x.organization_catalog.authority_lifecycle_enabled||!x.organization_catalog.exact_fact_clock_enabled)fail("TreatyRevisionCandidateMismatch");
 take(validate_political_treaty_terms_catalog(x.terms_catalog,x.term_sources));
}
}
SourceRef treaty_offer_source(const TreatyOfferSource& r){return {"politics","treaty_terms_offered",r.id};}
SourceRef treaty_counteroffer_source(const TreatyCounterofferSource& r){return {"politics","treaty_terms_counteroffered",r.id};}
Status validate_treaty_offer_source_shape(const TreatyOfferSource& r){if(r.id.empty()||r.terms_definition_id.empty()||r.offering_party.empty()||r.offering_party!=r.issuer.organization_id||!actor_shape(r.issuer))return common_failure("InvalidTreatyOfferSource",r.id);auto m=encode_world_moment(r.issued_at);return m.ok?common_success():Status{false,{},m.error};}
Status validate_treaty_counteroffer_source_shape(const TreatyCounterofferSource& r){if(r.id.empty()||r.treaty_id.empty()||r.terms_revision<2||r.terms_definition_id.empty()||r.offering_party.empty()||r.offering_party!=r.issuer.organization_id||!actor_shape(r.issuer)||!valid_source_ref(r.previous_terms_source)||r.previous_terms_source.domain!="politics"||(r.previous_terms_source.kind!="treaty_terms_offered"&&r.previous_terms_source.kind!="treaty_terms_counteroffered"))return common_failure("InvalidTreatyCounterofferSource",r.id);auto m=encode_world_moment(r.issued_at);return m.ok?common_success():Status{false,{},m.error};}
Outcome<WorldMoment> NativeTreatyRevisionIssueRead::read_organization_authority_now()const{return context_.clock.read_organization_authority_now();}
Outcome<AuthorityIssueSourceView> NativeTreatyRevisionIssueRead::read_authority_issue_source(const SourceRef& ref)const{try{auto r=raw(context_,ref);auto q=request(context_,r,&ref);ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=r.issued_at;return {true,{ref,r.issuer->actor_id,q.command_kind,std::move(at),std::move(q.amount_components)},{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyRevisionView> read_treaty_revision_source(const TreatyRevisionContext& x,const SourceRef& ref,Integer schema){try{if(schema!=1)fail("TreatyRevisionSchemaUnsupported",ref.id);auto r=raw(x,ref);auto history=chain(x,r.treaty_id);for(auto& old:history)if(same_source_ref(old.source,ref))return {true,std::move(old),{}};fail("TreatyRevisionNotInOriginalChain",ref.id);}catch(const Failure& f){return {false,{},f.error};}}
Outcome<TreatyRevisionView> read_current_treaty_revision(const TreatyRevisionContext& x,const StableId& treaty){try{auto history=chain(x,treaty);return {true,std::move(history.back()),{}};}catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_revision_sources(const TreatyRevisionContext& x){try{
 take(validate_political_treaty_terms_catalog(x.terms_catalog,x.term_sources));for(const auto& row:x.offers.rows()){if(row.first!=row.second.id)fail("TreatyOfferIdentityMismatch",row.first);chain(x,row.first);}
 for(const auto& row:x.counteroffers.rows())if(!x.offers.find(row.second.treaty_id))fail("TreatyCounterofferWithoutOriginal",row.first);
 for(const auto& row:x.common.state().facts.rows())if(row.second.domain=="politics"){
  if(row.second.kind=="treaty_terms_offered"&&!x.offers.find(row.first))fail("TreatyOfferOrphanFact",row.first);
  if(row.second.kind=="treaty_terms_counteroffered"&&!x.counteroffers.find(row.first))fail("TreatyCounterofferOrphanFact",row.first);
 }return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> propose_treaty_terms_in_candidate(CommonStore& c,OrderedTable<TreatyOfferSource>& rows,AuthorityBasisRegistry& bases,const TreatyRevisionContext& x,const ProposeTreatyTermsCommand& command){try{
 candidate(c,bases,x);if(&rows!=&x.offers)fail("TreatyRevisionCandidateMismatch");const auto& def=definition(x,command.terms_definition_id);const auto at=take(x.clock.read_organization_authority_now());
 if(std::find(def.terms.parties.begin(),def.terms.parties.end(),command.authority.organization_id)==def.terms.parties.end())fail("TreatyRevisionPartyNotInTerms");
 TreatyRevisionView pending;pending.terms_revision=1;pending.definition=&def;pending.issuer=&command.authority;pending.issued_at=at;auto q=request(x,pending,nullptr);
 auto captured=take(check_current_grant_authority(c,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,q,&x.clock));
 TreatyOfferSource row;row.id=take(c.next_id("treaty_offer"));row.terms_definition_id=def.id;row.offering_party=command.authority.organization_id;row.issuer=command.authority;row.issued_at=at;take(validate_treaty_offer_source_shape(row));const auto ref=treaty_offer_source(row);
 take(c.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatyOfferSource",ref.id);
 NativeTreatyRevisionIssueRead actual(x);take(register_authority_basis_for_fact(c,bases,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,q,captured,ref,actual,{},x.budgets));
 take(read_treaty_revision_source(x,ref,1));return {true,ref,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> counter_treaty_terms_in_candidate(CommonStore& c,OrderedTable<TreatyCounterofferSource>& rows,AuthorityBasisRegistry& bases,const TreatyRevisionContext& x,const CounterTreatyTermsCommand& command){try{
 candidate(c,bases,x);if(&rows!=&x.counteroffers)fail("TreatyRevisionCandidateMismatch");auto previous=take(read_current_treaty_revision(x,command.treaty_id));
 if(previous.terms_revision!=command.expected_terms_revision||!same_source_ref(previous.source,command.expected_terms_source))fail("TreatyTermsChanged",command.treaty_id);
 if(previous.terms_revision==std::numeric_limits<Integer>::max())fail("TreatyRevisionOverflow",command.treaty_id);
 const auto& def=definition(x,command.terms_definition_id);const auto at=take(x.clock.read_organization_authority_now());
 take(x.originals.validate_counteroffer_transition(previous,def,command.authority.organization_id,command.authority,at,nullptr));
 TreatyRevisionView pending;pending.treaty_id=command.treaty_id;pending.terms_revision=previous.terms_revision+1;pending.definition=&def;pending.issuer=&command.authority;pending.issued_at=at;auto q=request(x,pending,nullptr);
 auto captured=take(check_current_grant_authority(c,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,q,&x.clock));
 TreatyCounterofferSource row;row.id=take(c.next_id("treaty_counteroffer"));row.treaty_id=command.treaty_id;row.terms_revision=pending.terms_revision;row.previous_terms_source=previous.source;row.terms_definition_id=def.id;row.offering_party=command.authority.organization_id;row.issuer=command.authority;row.issued_at=at;take(validate_treaty_counteroffer_source_shape(row));const auto ref=treaty_counteroffer_source(row);
 take(c.append_fact({ref.id,ref.kind,ref.domain}));if(!rows.insert(ref.id,std::move(row)))fail("DuplicateTreatyCounterofferSource",ref.id);
 NativeTreatyRevisionIssueRead actual(x);take(register_authority_basis_for_fact(c,bases,x.organization,x.organization_catalog,x.organization_catalog.authority_catalog_identity,q,captured,ref,actual,{},x.budgets));
 take(read_treaty_revision_source(x,ref,1));return {true,ref,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
