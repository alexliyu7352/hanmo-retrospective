// 原委任签发、历史Basis与同版使用反核；所有副作用留在同一未发布候选。
#include "treaty_mandate_sources.hpp"
#include <algorithm>
#include <limits>
#include <iterator>
namespace hanmo::resident {namespace {
struct Failure{RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
template<class T>T take(Outcome<T> r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
int compare(const WorldMoment& a,const WorldMoment& b){return take(compare_world_moments(a,b));}
bool acting(const ActingAuthority& a,const ActingAuthority& b){return a.actor_id==b.actor_id&&a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.authority_epoch==b.authority_epoch;}
bool scope(const ScopeRef& a,const ScopeRef& b){return a.kind==b.kind&&a.id==b.id;}
std::size_t position(const CommonStore& c,const SourceRef& ref){const auto* f=c.state().facts.find(ref.id);const auto& order=c.state().facts.order();auto p=std::find(order.begin(),order.end(),ref.id);if(!f||f->domain!=ref.domain||f->kind!=ref.kind||p==order.end())fail("TreatyMandateActualFactRequired",ref.id);return static_cast<std::size_t>(p-order.begin());}
// 原精确签发只读腿；同M仍按真正Common追加切点，不按ID文字排序。
const TreatyMandateSource& raw(const TreatyMandateContext& x,const SourceRef& ref){
 if(!valid_source_ref(ref)||ref.domain!="politics"||ref.kind!="treaty_negotiation_mandated")fail("TreatyMandateSourceUnsupported",ref.id);
 const auto* p=x.mandates.find(ref.id);if(!p||p->id!=ref.id)fail("TreatyMandateSourceMissing",ref.id);take(validate_treaty_mandate_source_shape(*p));
 auto original=take(read_treaty_revision_source(x.revisions,p->original_terms_source,1));
 if(original.treaty_id!=p->treaty_id||original.terms_revision!=p->original_terms_revision||!original.definition||
    std::find(original.definition->terms.parties.begin(),original.definition->terms.parties.end(),p->issuer.organization_id)==original.definition->terms.parties.end()||
    position(x.revisions.common,original.source)>=position(x.revisions.common,ref)||compare(original.issued_at,p->issued_at)>0||
    compare(p->issued_at,take(x.revisions.clock.read_organization_authority_now()))>0||
    compare(p->issued_at,WorldMoment{p->mandate.window.until_tick,{}})>=0)fail("TreatyMandateOriginalRevisionMismatch",ref.id);
 for(const auto& row:x.revisions.counteroffers.rows())if(row.second.treaty_id==p->treaty_id&&row.second.terms_revision>p->original_terms_revision&&position(x.revisions.common,treaty_counteroffer_source(row.second))<position(x.revisions.common,ref))fail("TreatyMandateIssuedForSupersededTerms",ref.id);
 take(x.originals.validate_negotiation_mandate_issue_inputs(original,p->issuer,p->envoy_actor_id,p->mandate,p->issued_at,&ref));return *p;
}
SingleGrantAuthorityRequest request(const TreatyMandateContext& x,const TreatyMandateSource& row,const SourceRef* cut){
 auto original=take(read_treaty_revision_source(x.revisions,row.original_terms_source,1));auto q=take(x.originals.read_negotiation_mandate_issue_authority(original,row.issuer,row.envoy_actor_id,row.mandate,cut));
 if(!acting(q.acting,row.issuer)||q.command_kind!="diplomatic_commission_envoy"||!q.requires_delegation||!scope(q.scope,row.mandate.scope))fail("TreatyMandateSpecificDelegationRequired",row.id);return q;
}
// 同一原Basis逐字段反验，仍只用原历史Grant；使者并不获得一份新Grant。
void authority(const TreatyMandateContext& x,const TreatyMandateSource& row){
 const auto& r=x.revisions;const auto ref=treaty_mandate_source(row);auto q=request(x,row,&ref);NativeTreatyMandateIssueRead source(x);
 const auto* b=take(read_registered_authority_basis(r.common,r.authority_bases,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,{ref,1},source,r.budgets));
 if(b->actor_id!=row.issuer.actor_id||b->organization_id!=row.issuer.organization_id||b->office_id!=row.issuer.office_id||b->office_epoch!=row.issuer.authority_epoch||b->command_kind!=q.command_kind||!scope(b->scope,q.scope)||b->amount!=q.amount||b->resources.size()!=q.resources.size()||!same_authority_amount_components(b->amount_components,q.amount_components)||b->issued_at.kind!=ActorFoundationAt::Kind::exact_moment||compare(b->issued_at.exact_moment,row.issued_at))fail("TreatyMandateAuthorityBasisMismatch",row.id);
 for(std::size_t i=0;i<q.resources.size();++i)if(q.resources[i].kind!=b->resources[i].kind||q.resources[i].id!=b->resources[i].id)fail("TreatyMandateAuthorityResourceMismatch",row.id);
 auto history=take(read_historical_authority_grant(r.common,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,*b,source,r.budgets));if(!history.terms||!history.terms->may_delegate)fail("TreatyMandateDelegationDenied",row.id);
}
// 有限钱物上限只覆盖其原量纲；不能因缺某项就默认无限，也不能混币/混物加总。
void bounded_financial_terms(const NegotiationMandate& mandate,const TreatyTermsV1& terms,const StableId& party){
 Integer cash=0;StableId currency;std::vector<std::pair<StableId,Integer>> items;
 auto add=[&](Integer& target,Integer amount){if(amount<0||target>std::numeric_limits<Integer>::max()-amount)fail("TreatyMandateExposureOverflow");target+=amount;};
 for(const auto& term:terms.terms){const auto kind=static_cast<TreatyTermKind>(term.terms.index());if(std::find(mandate.allowed_term_kinds.begin(),mandate.allowed_term_kinds.end(),kind)==mandate.allowed_term_kinds.end())fail("TreatyTermOutsideMandate",term.term_id);
  const auto* aid=std::get_if<TreatyMaterialAidTerm>(&term.terms);if(!aid||aid->source_party!=party)continue;
  // 原承诺总暴露按有限实际期数，单期量不冒整个委任上限。
  auto total=[&](Integer amount){if(amount<0||aid->occurrences<1||amount>std::numeric_limits<Integer>::max()/aid->occurrences)fail("TreatyMandateExposureOverflow",term.term_id);return amount*aid->occurrences;};
  if(aid->money){if(!currency.empty()&&currency!=aid->money->currency_id)fail("TreatyMandateCurrencyMismatch",term.term_id);currency=aid->money->currency_id;add(cash,total(aid->money->amount));}
  for(const auto& amount:aid->items){auto p=std::find_if(items.begin(),items.end(),[&](const auto& item){return item.first==amount.item_id;});if(p==items.end()){items.emplace_back(amount.item_id,0);p=std::prev(items.end());}add(p->second,total(amount.quantity));}
  // 违约补偿、运输等其它实际暴露仍须原专项request完整核，不能在这里拿max或预算cap填实际费用。
 }
 if(cash&&(!mandate.money_cap||mandate.money_cap->currency_id!=currency||cash>mandate.money_cap->amount))fail("TreatyMoneyOutsideMandate");
 for(const auto& item:items){auto cap=std::find_if(mandate.quantity_caps.begin(),mandate.quantity_caps.end(),[&](const auto& v){return v.item_id==item.first;});if(cap==mandate.quantity_caps.end()||item.second>cap->quantity)fail("TreatyQuantityOutsideMandate",item.first);}
}
}
SourceRef treaty_mandate_source(const TreatyMandateSource& r){return {"politics","treaty_negotiation_mandated",r.id};}
Status validate_negotiation_mandate_shape(const NegotiationMandate& m){
 if(m.allowed_term_kinds.empty()||m.scope.id.empty()||static_cast<Integer>(m.scope.kind)<0||static_cast<Integer>(m.scope.kind)>static_cast<Integer>(ScopeRef::Kind::activity)||m.window.from_tick<0||m.window.until_tick<=m.window.from_tick)return common_failure("InvalidNegotiationMandate");
 for(std::size_t i=0;i<m.allowed_term_kinds.size();++i){const auto kind=m.allowed_term_kinds[i];if(static_cast<Integer>(kind)<0||static_cast<Integer>(kind)>static_cast<Integer>(TreatyTermKind::submission))return common_failure("InvalidNegotiationMandateTerm");for(std::size_t j=0;j<i;++j)if(m.allowed_term_kinds[j]==kind)return common_failure("DuplicateNegotiationMandateTerm");}
 for(std::size_t i=0;i<m.quantity_caps.size();++i){const auto& cap=m.quantity_caps[i];if(cap.item_id.empty()||cap.quantity<0)return common_failure("InvalidNegotiationMandateQuantityCap");for(std::size_t j=0;j<i;++j)if(m.quantity_caps[j].item_id==cap.item_id)return common_failure("DuplicateNegotiationMandateQuantityCap",cap.item_id);}
 if(m.money_cap&&(m.money_cap->currency_id.empty()||m.money_cap->amount<0))return common_failure("InvalidNegotiationMandateMoneyCap");return common_success();
}
Status validate_treaty_mandate_source_shape(const TreatyMandateSource& r){
 if(r.id.empty()||r.treaty_id.empty()||r.original_terms_revision<1||r.issuer.actor_id.empty()||r.issuer.office_id.empty()||r.issuer.organization_id.empty()||r.issuer.authority_epoch<0||r.envoy_actor_id.empty()||r.envoy_actor_id==r.issuer.actor_id||!valid_source_ref(r.original_terms_source)||r.original_terms_source.domain!="politics"||(r.original_terms_source.kind!="treaty_terms_offered"&&r.original_terms_source.kind!="treaty_terms_counteroffered"))return common_failure("InvalidTreatyMandateSource",r.id);
 auto valid=validate_negotiation_mandate_shape(r.mandate);if(!valid.ok)return valid;auto moment=encode_world_moment(r.issued_at);return moment.ok?common_success():Status{false,{},moment.error};
}
Outcome<WorldMoment> NativeTreatyMandateIssueRead::read_organization_authority_now()const{return x_.revisions.clock.read_organization_authority_now();}
Outcome<AuthorityIssueSourceView> NativeTreatyMandateIssueRead::read_authority_issue_source(const SourceRef& ref)const{try{const auto& r=raw(x_,ref);auto q=request(x_,r,&ref);ActorFoundationAt at;at.kind=ActorFoundationAt::Kind::exact_moment;at.exact_moment=r.issued_at;return {true,{ref,r.issuer.actor_id,q.command_kind,std::move(at),std::move(q.amount_components)},{}};}catch(const Failure& f){return {false,{},f.error};}}
Outcome<const TreatyMandateSource*> read_treaty_mandate_source(const TreatyMandateContext& x,const SourceRef& ref,Integer schema){try{if(schema!=1)fail("TreatyMandateSchemaUnsupported",ref.id);const auto& row=raw(x,ref);authority(x,row);return {true,&row,{}};}catch(const Failure& f){return {false,nullptr,f.error};}}
Status validate_treaty_mandate_sources(const TreatyMandateContext& x){try{for(const auto& row:x.mandates.rows()){if(row.first!=row.second.id)fail("TreatyMandateIdentityMismatch",row.first);take(read_treaty_mandate_source(x,treaty_mandate_source(row.second),1));}for(const auto& row:x.revisions.common.state().facts.rows())if(row.second.domain=="politics"&&row.second.kind=="treaty_negotiation_mandated"&&!x.mandates.find(row.first))fail("TreatyMandateOrphanFact",row.first);return common_success();}catch(const Failure& f){return {false,{},f.error};}}
Status validate_treaty_mandate_use(const TreatyMandateContext& x,const SourceRef& ref,Integer schema,const StableId& envoy,const SourceRef& terms,Integer revision,const WorldMoment& at,bool signing,const SourceRef* cut){try{
 const auto* row=take(read_treaty_mandate_source(x,ref,schema));auto actual=take(read_treaty_revision_source(x.revisions,terms,1));
 if(row->envoy_actor_id!=envoy||actual.treaty_id!=row->treaty_id||actual.terms_revision!=revision||!actual.definition||revision<row->original_terms_revision||std::find(actual.definition->terms.parties.begin(),actual.definition->terms.parties.end(),row->issuer.organization_id)==actual.definition->terms.parties.end()||compare(at,WorldMoment{actual.definition->terms.effective_window.until_tick,{}})>=0||compare(row->issued_at,at)>0||compare(actual.issued_at,at)>0||compare(at,take(x.revisions.clock.read_organization_authority_now()))>0||!take(treaty_window_contains(row->mandate.window,at))||(signing&&!row->mandate.may_sign))fail("TreatyUseOutsideMandate",ref.id);
 if(cut){if(position(x.revisions.common,ref)>=position(x.revisions.common,*cut)||position(x.revisions.common,terms)>=position(x.revisions.common,*cut))fail("TreatyMandateUsedBeforeIssue",ref.id);}else if(compare(at,take(x.revisions.clock.read_organization_authority_now())))fail("TreatyMandateCurrentUseClockRequired",ref.id);
 for(const auto& newer:x.revisions.counteroffers.rows())if(newer.second.treaty_id==row->treaty_id&&newer.second.terms_revision>revision&&(!cut||position(x.revisions.common,treaty_counteroffer_source(newer.second))<position(x.revisions.common,*cut)))fail("TreatyMandateUsedForSupersededTerms",ref.id);
 bounded_financial_terms(row->mandate,actual.definition->terms,row->issuer.organization_id);take(x.originals.validate_negotiation_mandate_use(*row,actual,at,cut));return common_success();
 }catch(const Failure& f){return {false,{},f.error};}}
Outcome<SourceRef> issue_treaty_mandate_in_candidate(CommonStore& common,OrderedTable<TreatyMandateSource>& rows,AuthorityBasisRegistry& bases,const TreatyMandateContext& x,const IssueTreatyMandateCommand& command){try{
 const auto& r=x.revisions;if(&common!=&r.common||&rows!=&x.mandates||&bases!=&r.authority_bases||!r.organization_catalog.authority_lifecycle_enabled||!r.organization_catalog.exact_fact_clock_enabled)fail("TreatyMandateCandidateMismatch");
 auto current=take(read_current_treaty_revision(r,command.treaty_id));if(current.terms_revision!=command.expected_terms_revision||!same_source_ref(current.source,command.expected_terms_source))fail("TreatyTermsChanged",command.treaty_id);
 take(validate_negotiation_mandate_shape(command.mandate));TreatyMandateSource row;row.treaty_id=command.treaty_id;row.original_terms_revision=current.terms_revision;row.original_terms_source=current.source;row.issuer=command.authority;row.envoy_actor_id=command.envoy_actor_id;row.mandate=command.mandate;row.issued_at=take(r.clock.read_organization_authority_now());
 if(compare(row.issued_at,WorldMoment{row.mandate.window.until_tick,{}})>=0)fail("TreatyMandateWindowExpired");take(x.originals.validate_negotiation_mandate_issue_inputs(current,row.issuer,row.envoy_actor_id,row.mandate,row.issued_at,nullptr));
 auto q=request(x,row,nullptr);auto captured=take(check_current_grant_authority(common,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,q,&r.clock));
 row.id=take(common.next_id("treaty_mandate"));take(validate_treaty_mandate_source_shape(row));const auto source=treaty_mandate_source(row);take(common.append_fact({source.id,source.kind,source.domain}));if(!rows.insert(source.id,std::move(row)))fail("DuplicateTreatyMandateSource",source.id);
 NativeTreatyMandateIssueRead actual(x);take(register_authority_basis_for_fact(common,bases,r.organization,r.organization_catalog,r.organization_catalog.authority_catalog_identity,q,captured,source,actual,{},r.budgets));take(read_treaty_mandate_source(x,source,1));return {true,source,{}};
 }catch(const Failure& f){return {false,{},f.error};}}
}
