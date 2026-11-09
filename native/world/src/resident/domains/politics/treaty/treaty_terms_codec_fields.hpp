// 唯一五term字段表，JSON、checkpoint及冻结投影同用；不修改借读writer对象。
#pragma once
#include "treaty_terms_contract.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
#include "resident/domains/travel/passage_boundary_codec_fields.hpp"
namespace hanmo::resident::treaty_terms_codec_fields {
using organization_lifecycle_fields::enum_field;
using organization_lifecycle_fields::source;
using organization_lifecycle_fields::sources;
using organization_lifecycle_fields::strings;
using organization_lifecycle_fields::rule;
using organization_lifecycle_fields::table;
template<class C> Status subject(C& c,SubjectRef& r){static constexpr const char* tags[]={"actor","organization","cohort"};enum_field(c,"kind",r.kind,tags);return c.field("id",r.id);}
template<class C> Status scope(C& c,ScopeRef& r){static constexpr const char* tags[]={"site","region","force","account","activity"};enum_field(c,"kind",r.kind,tags);return c.field("id",r.id);}
template<class C> Status window(C& c,PassageWindow& r){c.field("from_tick",r.from_tick);return c.field("until_tick",r.until_tick);}
template<class C> Status budget(C& c,BudgetAuthorizationRef& r){c.field("authorization_id",r.authorization_id);c.field("authorization_revision",r.authorization_revision);return c.object("approval_source",[&](auto& v){return source(v,r.approval_source);});}
template<class C> Status boundaries(C& c,const StableId& name,std::vector<PassageBoundaryRef>& r){return c.sequence(name,r,[](auto& v,auto& x){return passage_boundary_codec_fields::fields(v,x);});}
template<class C,class E,std::size_t N> Status tags(C& c,const StableId& name,std::vector<E>& values,const char* const (&names)[N]){return c.sequence(name,values,[&](auto& v,auto& x){return enum_field(v,"value",x,names);});}
template<class C,class V,std::size_t N> Status variant_tag(C& c,V& value,const char* const (&names)[N],std::size_t& index){StableId name;if(!c.reading()){index=value.index();if(index>=N)return c.reject("InvalidTreatyVariant");name=names[index];}c.field("kind",name);if(!c.status().ok)return c.status();index=0;for(;index<N&&name!=names[index];++index){}return index<N?c.status():c.reject("UnknownTreatyVariant");}
template<class C> Status safe_exit(C& c,SafeExitTerms& r){
 c.field("grace_ticks",r.grace_ticks);c.field("max_exit_half_days",r.max_exit_half_days);
 c.sequence("paths",r.paths,[](auto& v,auto& p){v.field("path_id",p.path_id);v.object("inside_start_scope",[&](auto& a){return scope(a,p.inside_start_scope);});boundaries(v,"directed_boundary_refs",p.directed_boundary_refs);return v.field("outside_end_site",p.outside_end_site);});
 static constexpr const char* manifests[]={"entered_survivors_and_original_cargo","entered_survivors_and_lawful_trade_cargo"};enum_field(c,"eligible_manifest_policy",r.eligible_manifest_policy,manifests);
 c.field("include_birth_dependents",r.include_birth_dependents);
 static constexpr const char* actions[]={"exit_transit","necessary_wait","resupply_for_exit","self_defense"};tags(c,"allowed_actions",r.allowed_actions,actions);
 c.object("tariff_rule_ref",[&](auto& v){return rule(v,r.tariff_rule_ref);});
 static constexpr const char* fallback[]={"hold_and_request_permission"};return enum_field(c,"blocked_fallback",r.blocked_fallback,fallback);
}
template<class C> Status tariff(C& c,PassageTariff& r){c.object("source",[&](auto& v){return source(v,r.source);});static constexpr const char* kinds[]={"fixed_per_crossing","per_person","per_vessel","per_weight_unit"};enum_field(c,"kind",r.kind,kinds);c.field("currency_id",r.currency_id);c.field("payee_wallet_id",r.payee_wallet_id);c.field("amount_per_unit",r.amount_per_unit);return c.field("weight_unit",r.weight_unit);}
template<class C> Status truce(C& c,TreatyTruceTerm& r){
 c.sequence("organization_pairs",r.organization_pairs,[](auto& v,auto& p){v.field("first",p.first);return v.field("second",p.second);});
 strings(c,"jurisdiction_ids",r.jurisdiction_ids);boundaries(c,"boundary_refs",r.boundary_refs);
 static constexpr const char* actions[]={"attack","siege","raid","naval_attack","seize_public_assets"};tags(c,"prohibited_actions",r.prohibited_actions,actions);
 c.field("notice_ticks",r.notice_ticks);return c.object("withdrawal_route_policy",[&](auto& v){return rule(v,r.withdrawal_route_policy);});
}
template<class C> Status passage(C& c,TreatyPassageTerm& r){
 strings(c,"eligible_parties",r.eligible_parties);static constexpr const char* modes[]={"foot","army","boat"};tags(c,"allowed_modes",r.allowed_modes,modes);
 boundaries(c,"boundary_refs",r.boundary_refs);c.object("window",[&](auto& v){return window(v,r.window);});
 static constexpr const char* policies[]={"official_notice","present_document"};enum_field(c,"credential_policy",r.credential_policy,policies);
 c.optional("tariff",r.tariff,[](auto& v,auto& x){return tariff(v,x);});return c.object("safe_exit",[&](auto& v){return safe_exit(v,r.safe_exit);});
}
template<class C> Status defense(C& c,TreatyMutualDefenseTerm& r){
 strings(c,"protected_parties",r.protected_parties);strings(c,"covered_jurisdictions",r.covered_jurisdictions);
 static constexpr const char* triggers[]={"external_armed_attack"};enum_field(c,"trigger_kind",r.trigger_kind,triggers);
 strings(c,"eligible_force_pool_ids",r.eligible_force_pool_ids);c.field("max_dispatched_persons",r.max_dispatched_persons);c.field("max_concurrent_calls",r.max_concurrent_calls);c.field("dispatch_within_ticks",r.dispatch_within_ticks);c.field("max_service_ticks",r.max_service_ticks);
 sources(c,"supply_contract_refs",r.supply_contract_refs);c.object("budget_ref",[&](auto& v){return budget(v,r.budget_ref);});return c.object("return_policy",[&](auto& v){return rule(v,r.return_policy);});
}
template<class C> Status late_policy(C& c,TreatyAidLatePolicy& r){
 static constexpr const char* names[]={"record_breach","bounded_compensation"};std::size_t index=0;auto valid=variant_tag(c,r,names,index);if(!valid.ok)return valid;
 if(index==0){if(c.reading())r=TreatyRecordBreach{};return c.status();}
 if(c.reading())r=TreatyBoundedCompensation{};auto& x=std::get<TreatyBoundedCompensation>(r);c.field("currency_id",x.currency_id);c.field("maximum_amount",x.maximum_amount);return c.field("obligation_owner",x.obligation_owner);
}
template<class C> Status aid(C& c,TreatyMaterialAidTerm& r){
 c.field("source_party",r.source_party);c.object("beneficiary",[&](auto& v){return subject(v,r.beneficiary);});c.object("delivery_site",[&](auto& v){return source(v,r.delivery_site);});
 c.sequence("items",r.items,[](auto& v,auto& x){v.field("item_id",x.item_id);return v.field("quantity",x.quantity);});
 c.optional("money",r.money,[](auto& v,auto& x){v.field("currency_id",x.currency_id);return v.field("amount",x.amount);});
 static constexpr const char* schedules[]={"once","periodic"};enum_field(c,"schedule",r.schedule,schedules);
 c.optional("period_ticks",r.period_ticks,[](auto& v,auto& x){return v.field("value",x);});c.field("occurrences",r.occurrences);c.field("first_due_tick",r.first_due_tick);c.field("delivery_deadline_ticks",r.delivery_deadline_ticks);
 c.object("transport_cost_payer",[&](auto& v){return subject(v,r.transport_cost_payer);});c.object("budget_ref",[&](auto& v){return budget(v,r.budget_ref);});
 c.object("late_policy",[&](auto& v){return late_policy(v,r.late_policy);});static constexpr const char* gates[]={"at_due_tick"};return enum_field(c,"establishment_gate",r.establishment_gate,gates);
}
template<class C> Status submission(C& c,TreatySubmissionTerm& r){
 c.field("subordinate_org",r.subordinate_org);c.field("superior_org",r.superior_org);strings(c,"jurisdiction_ids",r.jurisdiction_ids);
 static constexpr const char* powers[]={"appoint_local_head","set_tax_ceiling","approve_budget","issue_strategic_order","admit_passage"};tags(c,"shared_powers",r.shared_powers,powers);
 c.field("retained_policy_id",r.retained_policy_id);sources(c,"command_bindings",r.command_bindings);strings(c,"tribute_term_ids",r.tribute_term_ids);
 static constexpr const char* exits[]={"mutual_release","notice_with_settlement"};enum_field(c,"exit_policy",r.exit_policy,exits);return c.field("notice_ticks",r.notice_ticks);
}
template<class C> Status term(C& c,TreatyTerm& r){
 c.field("term_id",r.term_id);return c.object("terms",[&](auto& v){static constexpr const char* names[]={"truce","passage","mutual_defense","material_aid","submission"};std::size_t index=0;auto valid=variant_tag(v,r.terms,names,index);if(!valid.ok)return valid;
 switch(index){case 0:if(v.reading())r.terms=TreatyTruceTerm{};return truce(v,std::get<TreatyTruceTerm>(r.terms));
 case 1:if(v.reading())r.terms=TreatyPassageTerm{};return passage(v,std::get<TreatyPassageTerm>(r.terms));
 case 2:if(v.reading())r.terms=TreatyMutualDefenseTerm{};return defense(v,std::get<TreatyMutualDefenseTerm>(r.terms));
 case 3:if(v.reading())r.terms=TreatyMaterialAidTerm{};return aid(v,std::get<TreatyMaterialAidTerm>(r.terms));
 case 4:if(v.reading())r.terms=TreatySubmissionTerm{};return submission(v,std::get<TreatySubmissionTerm>(r.terms));}
 return v.reject("UnknownTreatyTerm");});
}
template<class C> Status terms(C& c,TreatyTermsV1& r){
 c.field("treaty_kind",r.treaty_kind);strings(c,"parties",r.parties);c.sequence("terms",r.terms,[](auto& v,auto& x){return term(v,x);});c.object("effective_window",[&](auto& v){return window(v,r.effective_window);});
 static constexpr const char* ratification[]={"all_parties"};enum_field(c,"ratification_policy",r.ratification_policy,ratification);
 static constexpr const char* termination[]={"by_expiry","mutual_release","material_breach_after_notice"};tags(c,"termination_policy",r.termination_policy,termination);return c.field("rule_version",r.rule_version);
}
template<class C> Status catalog(C& c,PoliticalTreatyTermsCatalog& r){Integer schema=1;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=1)return c.reject("UnsupportedTreatyTermsCatalogSchema");return table(c,"definitions",r.definitions,[](auto& v,auto& x){v.field("id",x.id);return v.object("terms",[&](auto& a){return terms(a,x.terms);});});}
}
