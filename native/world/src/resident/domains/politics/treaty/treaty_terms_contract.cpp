#include "resident/kernel/catalog_section_observation.hpp"
#include "treaty_terms_contract.hpp"
#include <algorithm>
#include <limits>
#include <type_traits>
namespace hanmo::resident {namespace {
struct Failure {RuleError error;};
[[noreturn]]void fail(const char* code,const StableId& id={}){throw Failure{common_failure(code,id).error};}
void take(Status s){if(!s.ok)throw Failure{std::move(s.error)};}
template<class T>T take(Outcome<T> s){if(!s.ok)throw Failure{std::move(s.error)};return std::move(s.value);}
template<class F>Status guard(F&&f){try{f();return common_success();}catch(const Failure&e){return {false,{},e.error};}}
template<class E>void tag(E value,E maximum){if(static_cast<unsigned>(value)>static_cast<unsigned>(maximum))fail("UnknownTreatyTermTag");}
void id(const StableId& v){if(v.empty())fail("TreatyTermsIdentityRequired");}
void rule(const ActorFoundationRuleRef&r){if(r.id.empty()||r.version<1||r.catalog_digest.empty())fail("TreatyOriginalRuleRequired",r.id);}
void source(const SourceRef&s){if(!valid_source_ref(s))fail("TreatyOriginalSourceRequired",s.id);}
void window(const PassageWindow&w){if(w.from_tick<0||w.until_tick<=w.from_tick)fail("InvalidTreatyCalendarWindow");}
template<class T,class Same>void distinct(const std::vector<T>&v,Same same){for(std::size_t i=0;i<v.size();++i)for(std::size_t j=0;j<i;++j)if(same(v[i],v[j]))fail("DuplicateTreatyTermsValue");}
void ids(const std::vector<StableId>&v,bool empty=false){if(v.empty()&&!empty)fail("TreatyTermsIdentitiesRequired");for(const auto&s:v)id(s);distinct(v,[](const auto&a,const auto&b){return a==b;});}
void sources(const std::vector<SourceRef>&v,bool empty=false){if(v.empty()&&!empty)fail("TreatyOriginalSourcesRequired");for(const auto&s:v)source(s);distinct(v,[](const auto&a,const auto&b){return same_source_ref(a,b);});}
template<class E>void tags(const std::vector<E>&v,E max){if(v.empty())fail("TreatyTermTagsRequired");for(auto e:v)tag(e,max);distinct(v,[](E a,E b){return a==b;});}
void scope(const ScopeRef&s){tag(s.kind,ScopeRef::Kind::activity);id(s.id);}
void subject(const SubjectRef&s){tag(s.kind,SubjectKind::cohort);id(s.id);}
void budget(const BudgetAuthorizationRef&r){if(r.authorization_id.empty()||r.authorization_revision<1)fail("TreatyBudgetAuthorizationRequired",r.authorization_id);source(r.approval_source);}
bool same_boundary(const PassageBoundaryRef&a,const PassageBoundaryRef&b){return a.kind==b.kind&&a.id==b.id&&a.map_id==b.map_id&&a.from_site_id==b.from_site_id&&a.to_site_id==b.to_site_id;}
void boundary(const PassageBoundaryRef&b){tag(b.kind,PassageBoundaryKind::port);id(b.id);id(b.map_id);id(b.from_site_id);id(b.to_site_id);if(b.from_site_id==b.to_site_id)fail("TreatyDirectedBoundaryRequired",b.id);}
void boundaries(const std::vector<PassageBoundaryRef>&v,bool empty=false){if(v.empty()&&!empty)fail("TreatyBoundariesRequired");for(const auto&b:v)boundary(b);distinct(v,same_boundary);}
void subset(const std::vector<StableId>&values,const std::vector<StableId>&parties){ids(values);for(const auto&v:values)if(std::find(parties.begin(),parties.end(),v)==parties.end())fail("TreatyPartyOutsideTerms",v);}
void exit_shape(const SafeExitTerms&x){
 if(x.grace_ticks<=0||x.max_exit_half_days<=0||x.paths.empty())fail("InvalidSafeExitWindow");
 tag(x.eligible_manifest_policy,SafeExitManifestPolicy::entered_survivors_and_lawful_trade_cargo);tag(x.blocked_fallback,SafeExitBlockedFallback::hold_and_request_permission);
 tags(x.allowed_actions,SafeExitAction::self_defense);rule(x.tariff_rule_ref);
 distinct(x.paths,[](const auto&a,const auto&b){return a.path_id==b.path_id;});
 for(const auto&p:x.paths){id(p.path_id);scope(p.inside_start_scope);id(p.outside_end_site);
  if(p.directed_boundary_refs.empty()||p.directed_boundary_refs.size()>64)fail("SafeExitPathBoundaryLimit",p.path_id);
  boundaries(p.directed_boundary_refs);for(std::size_t i=1;i<p.directed_boundary_refs.size();++i)if(p.directed_boundary_refs[i-1].to_site_id!=p.directed_boundary_refs[i].from_site_id)fail("SafeExitPathNotContiguous",p.path_id);
  if(p.directed_boundary_refs.back().to_site_id!=p.outside_end_site)fail("SafeExitOutsideEndpointMismatch",p.path_id);
 }
}
Integer due(const TreatyMaterialAidTerm&t,Integer n){
 tag(t.schedule,TreatyAidScheduleKind::periodic);tag(t.establishment_gate,TreatyAidEstablishmentGate::at_due_tick);
 if(t.occurrences<=0||n<0||n>=t.occurrences||t.first_due_tick<0||t.delivery_deadline_ticks<0)fail("InvalidTreatyAidOccurrence");
 if(t.schedule==TreatyAidScheduleKind::once){if(t.period_ticks||t.occurrences!=1)fail("InvalidTreatyOnceAidSchedule");return t.first_due_tick;}
 if(!t.period_ticks||*t.period_ticks<=0)fail("InvalidTreatyPeriodicAidSchedule");
 if(n>(std::numeric_limits<Integer>::max()-t.first_due_tick)/ *t.period_ticks)fail("TreatyAidDueOverflow");return t.first_due_tick+n* *t.period_ticks;
}
void shape(const TreatyTermsV1&t){
 id(t.treaty_kind);id(t.rule_version);ids(t.parties);if(t.parties.size()<2||t.terms.empty())fail("TreatyPartiesAndTermsRequired");window(t.effective_window);
 tag(t.ratification_policy,TreatyRatificationPolicy::all_parties);tags(t.termination_policy,TreatyTerminationMethod::material_breach_after_notice);
 distinct(t.terms,[](const auto&a,const auto&b){return a.term_id==b.term_id;});
 for(const auto&term:t.terms){id(term.term_id);if(term.terms.valueless_by_exception())fail("InvalidTreatyTermUnion",term.term_id);
  std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;
   if constexpr(std::is_same_v<T,TreatyTruceTerm>){
    if(x.organization_pairs.empty()||(x.jurisdiction_ids.empty()&&x.boundary_refs.empty())||x.notice_ticks<0)fail("InvalidTruceScope",term.term_id);
    for(const auto&p:x.organization_pairs){subset({p.first,p.second},t.parties);if(p.first==p.second)fail("InvalidTrucePartyPair",term.term_id);}
    distinct(x.organization_pairs,[](const auto&a,const auto&b){return (a.first==b.first&&a.second==b.second)||(a.first==b.second&&a.second==b.first);});
    ids(x.jurisdiction_ids,true);boundaries(x.boundary_refs,true);tags(x.prohibited_actions,TreatyProhibitedAction::seize_public_assets);rule(x.withdrawal_route_policy);
   }else if constexpr(std::is_same_v<T,TreatyPassageTerm>){
    subset(x.eligible_parties,t.parties);tags(x.allowed_modes,TreatyPassageMode::boat);boundaries(x.boundary_refs);window(x.window);
    if(x.window.from_tick<t.effective_window.from_tick||x.window.until_tick>t.effective_window.until_tick)fail("PassageOutsideTreatyWindow",term.term_id);
    tag(x.credential_policy,TreatyCredentialPolicy::present_document);exit_shape(x.safe_exit);
    if(x.tariff){const auto&f=*x.tariff;source(f.source);tag(f.kind,PassageTariffKind::per_weight_unit);id(f.currency_id);id(f.payee_wallet_id);if(f.amount_per_unit<0||(f.kind==PassageTariffKind::per_weight_unit?f.weight_unit<=0:f.weight_unit!=0))fail("InvalidTreatyPassageTariff",term.term_id);}
   }else if constexpr(std::is_same_v<T,TreatyMutualDefenseTerm>){
    subset(x.protected_parties,t.parties);ids(x.covered_jurisdictions);ids(x.eligible_force_pool_ids);tag(x.trigger_kind,TreatyDefenseTrigger::external_armed_attack);
    if(x.max_dispatched_persons<=0||x.max_concurrent_calls<=0||x.dispatch_within_ticks<=0||x.max_service_ticks<=0)fail("InvalidTreatyDefenseLimit",term.term_id);
    sources(x.supply_contract_refs);budget(x.budget_ref);rule(x.return_policy);
   }else if constexpr(std::is_same_v<T,TreatyMaterialAidTerm>){
    subset({x.source_party},t.parties);subject(x.beneficiary);subject(x.transport_cost_payer);source(x.delivery_site);budget(x.budget_ref);
    if(x.items.empty()&&!x.money)fail("TreatyAidActualContentRequired",term.term_id);for(const auto&i:x.items){id(i.item_id);if(i.quantity<=0)fail("InvalidTreatyAidQuantity",term.term_id);}distinct(x.items,[](const auto&a,const auto&b){return a.item_id==b.item_id;});
    if(x.money){id(x.money->currency_id);if(x.money->amount<=0)fail("InvalidTreatyAidMoney",term.term_id);}
    if(x.occurrences<=0)fail("InvalidTreatyAidOccurrence",term.term_id);const auto last=due(x,x.occurrences-1);if(x.first_due_tick<t.effective_window.from_tick||last>std::numeric_limits<Integer>::max()-x.delivery_deadline_ticks)fail("TreatyAidWindowOverflow",term.term_id);
    if(x.late_policy.valueless_by_exception())fail("InvalidTreatyAidLatePolicy",term.term_id);
    if(const auto*penalty=std::get_if<TreatyBoundedCompensation>(&x.late_policy)){id(penalty->currency_id);id(penalty->obligation_owner);if(penalty->maximum_amount<0)fail("InvalidTreatyCompensationCap",term.term_id);}
   }else{
    subset({x.subordinate_org,x.superior_org},t.parties);if(x.subordinate_org==x.superior_org||x.notice_ticks<0)fail("InvalidTreatySubmission",term.term_id);
    ids(x.jurisdiction_ids);tags(x.shared_powers,TreatySharedPower::admit_passage);id(x.retained_policy_id);sources(x.command_bindings,true);ids(x.tribute_term_ids,true);tag(x.exit_policy,TreatySubmissionExit::notice_with_settlement);
    if(std::find(x.shared_powers.begin(),x.shared_powers.end(),TreatySharedPower::issue_strategic_order)!=x.shared_powers.end()&&x.command_bindings.empty())fail("TreatyCommandBindingRequired",term.term_id);
    for(const auto&id:x.tribute_term_ids){const TreatyMaterialAidTerm*aid=nullptr;for(const auto&other:t.terms)if(other.term_id==id)aid=std::get_if<TreatyMaterialAidTerm>(&other.terms);if(!aid||aid->source_party!=x.subordinate_org||aid->beneficiary.kind!=SubjectKind::organization||aid->beneficiary.id!=x.superior_org)fail("TreatyTributeTermMismatch",id);}
   }
  },term.terms);
 }
 // 新条款自身不能含相互隶属环；现存跨条约关系由原author/activation reader继续核。
 for(const auto&term:t.terms)if(const auto*start=std::get_if<TreatySubmissionTerm>(&term.terms)){
  std::vector<StableId> pending{start->superior_org};
  for(std::size_t n=0;n<pending.size();++n){const auto next=pending[n];if(next==start->subordinate_org)fail("TreatySubmissionCycle",term.term_id);
   for(const auto&other:t.terms)if(const auto*x=std::get_if<TreatySubmissionTerm>(&other.terms);x&&x->subordinate_org==next&&std::find(pending.begin(),pending.end(),x->superior_org)==pending.end())pending.push_back(x->superior_org);
  }
 }
}
}
Status validate_safe_exit_terms_shape(const SafeExitTerms&t){return guard([&]{exit_shape(t);});}
Status validate_treaty_terms_shape(const TreatyTermsV1&t){return guard([&]{shape(t);});}
Status validate_political_treaty_terms_catalog(const PoliticalTreatyTermsCatalog&c,const TreatyTermsOriginalRead&r){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 116, "PoliticalTreatyTermsCatalog", "validate_political_treaty_terms_catalog", &c);
return guard([&]{
 if(c.identity.content_digest.empty())fail("TreatyCatalogIdentityRequired");for(const auto&row:c.definitions.rows()){if(row.first!=row.second.id||row.first.empty())fail("InvalidTreatyTermsDefinition",row.first);const auto&t=row.second.terms;shape(t);take(r.validate_treaty_rule(t.treaty_kind,t.rule_version));for(const auto&party:t.parties)take(r.validate_treaty_party(party));for(const auto&term:t.terms)std::visit([&](const auto&x){using T=std::decay_t<decltype(x)>;if constexpr(std::is_same_v<T,TreatyTruceTerm>)take(r.validate_truce_term(x));else if constexpr(std::is_same_v<T,TreatyPassageTerm>)take(r.validate_passage_term(x));else if constexpr(std::is_same_v<T,TreatyMutualDefenseTerm>)take(r.validate_mutual_defense_term(x));else if constexpr(std::is_same_v<T,TreatyMaterialAidTerm>)take(r.validate_material_aid_term(x));else take(r.validate_submission_term(x));},term.terms);}
});}
Outcome<bool> treaty_window_contains(const PassageWindow&w,const WorldMoment&at){try{window(w);take(encode_world_moment(at));return {true,take(compare_world_moments(WorldMoment{w.from_tick,{}},at))<=0&&take(compare_world_moments(at,WorldMoment{w.until_tick,{}}))<0,{}};}catch(const Failure&e){return {false,false,e.error};}}
Outcome<Integer> material_aid_occurrence_due(const TreatyMaterialAidTerm&t,Integer n){try{return {true,due(t,n),{}};}catch(const Failure&e){return {false,0,e.error};}}
}
