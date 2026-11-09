#include "resident/domains/military/military_model.hpp"
#include "resident/domains/organization/office_authority.hpp"
// 新经营责任必须显式选已发行政策，原供给源/受益主体与旧Need不被改写。
#include "politics_supply_adoptions_detail.hpp"
namespace hanmo::resident::supply_adoption_detail {
Outcome<EconomicSupplyPolicyTermsSource> terms(const PoliticalTakeoverContext& x,const PoliticalTakeoverRegistration& r,const PoliticalOriginalResponsibility& source,const StableId& id,const std::optional<StableId>& approval){
 using namespace takeover_detail;const auto fail=[&](){return Outcome<EconomicSupplyPolicyTermsSource>{false,{},common_failure("PoliticalSupplyPolicyMismatch",id).error};};
 if(!x.supply_adoptions||!x.supply_policies||x.supply_adoptions->version!=1)return {false,{},common_failure("PoliticalSupplySuccessorPolicyRequired").error};
 auto read=x.supply_policies->economic_supply_policy_terms(id,approval);if(!read.ok)return {false,{},read.error};if(!read.value)return fail();auto out=std::move(*read.value);const auto* rule=x.catalog.rules.find(r.rule_id);
 const auto* fact=x.common.state().facts.find(id);const auto sequence=organization_instance_sequence(id,"economic_supply_policy");const auto registration_sequence=organization_instance_sequence(r.id,"political_takeover_pending");
 if(!rule||!same_identity(out.source_identity,x.common.state().identity)||out.policy_fact_id!=id||out.registration_fact_id!=r.id||out.original_source_key!=source.responsibility.responsibility_id||source.responsibility.kind!=PoliticalResponsibilityKind::supply_source||out.policy.organization_id!=r.successor_organization_id||out.policy.site_id!=rule->site_id||!has(r.public_inventory_ids,out.policy.inventory_id)||!fact||fact->id!=id||fact->domain!="economic_supply_policies"||fact->kind!="economic_supply_policy_issued"||!sequence||!registration_sequence||*sequence<=*registration_sequence||*sequence>=x.common.state().next_instance_id||!supply_authority_detail::before(x.common,r.id,id))return fail();
 if(std::none_of(source.containers.begin(),source.containers.end(),[&](const auto& c){return c.inventory&&c.selected&&c.id==out.policy.inventory_id;})||out.policy.funding_wallet_id.empty()||out.policy.reward_wallet_id.empty()||out.maximum_contract_commitment<0||out.issued_tick<r.tick||out.issued_tick>x.common.tick()||out.issued_tick==INT64_MAX||out.earliest_effective_tick!=out.issued_tick+1||out.through_tick<out.earliest_effective_tick)return fail();
 if(approval){const auto before=organization_instance_sequence(*approval,"political_takeover");if(!before||*sequence>=*before||!supply_authority_detail::before(x.common,id,*approval))return fail();}
 return {true,std::move(out),{}};
}
Outcome<bool> active(const PoliticalTakeoverContext& x,const PoliticalTakeoverRegistration& r,const EconomicSupplyPolicyTermsSource& terms,Integer tick,Integer sequence,const StableId& boundary){
 using namespace takeover_detail;using namespace supply_authority_detail;const auto* rule=x.catalog.rules.find(r.rule_id);const auto& acting=terms.issuing_authority;
 if(!rule||acting.organization_id!=r.successor_organization_id||acting.office_id!=rule->successor_office_id)return {false,{},common_failure("PoliticalSupplyPolicyIssuerMismatch",terms.policy_fact_id).error};
 const auto* office=x.organization_catalog.offices.find(acting.office_id);const auto* grant=x.organization_catalog.grants.find(terms.issuing_grant_id);
 if(!office||!grant||!has(office->grant_ids,grant->id))return {false,{},common_failure("PoliticalSupplyGrantMismatch",terms.policy_fact_id).error};
 PoliticalSupplyPolicyMandate mandate{r.id,terms.original_source_key,terms.policy.organization_id,terms.policy.site_id,terms.policy.inventory_id,terms.policy.funding_wallet_id,terms.policy.reward_wallet_id,terms.maximum_contract_commitment};
 if(!covers(*grant,*rule,mandate,tick))return {true,false,{}};
 auto control=control_before(x,r,*rule,tick,sequence,boundary);if(!control.ok){if(control.error.original.code=="PoliticalSupplyControlChanged")return {true,false,{}};return {false,{},control.error};}
 if(boundary.empty()){
   auto authority = check_authority_with_single_grant(
       x.common, x.organizations, x.organization_catalog, acting,
       political_supply_policy_grant, {ScopeRef::Kind::site, rule->site_id},
       mandate.maximum_contract_commitment, *grant, nullptr);
   return {true, authority.ok, {}};
 }
 auto signer=validate_historical_office_signer(x.common,x.organizations,x.organization_catalog,acting,tick,sequence);if(!signer.ok)return {true,false,{}};
 for(const auto& id:grant->wallet_ids){auto owner=asset_owner_before(x,id,false,tick,sequence,boundary);if(!owner.ok)return {false,{},owner.error};if(!owner_is(owner.value,r.successor_organization_id))return {true,false,{}};}
 for(const auto& id:grant->inventory_ids){auto owner=asset_owner_before(x,id,true,tick,sequence,boundary);if(!owner.ok)return {false,{},owner.error};if(!owner_is(owner.value,r.successor_organization_id))return {true,false,{}};}
 return {true,true,{}};
}
}
namespace hanmo::resident {
Outcome<PoliticalResponsibilityDecision> derive_political_supply_decision(const PoliticalTakeoverContext& x,const PoliticalTakeoverRegistration& r,const PoliticalOriginalResponsibility& original,const PoliticalResponsibilityRequest& request,const std::optional<StableId>& approval){
 const bool adopting=request.responsibility.kind==PoliticalResponsibilityKind::supply_source&&request.disposition==PoliticalResponsibilityDisposition::assume_by_successor;
 if(!adopting){if(request.supply_policy_fact_id)return {false,{},common_failure("UnexpectedPoliticalSupplyPolicy").error};return takeover_detail::derive(original,request,r.successor_organization_id,r.former_organization_id);}
 if(!request.supply_policy_fact_id||request.supply_policy_fact_id->empty())return {false,{},common_failure("PoliticalSupplySuccessorPolicyRequired").error};
 if(request.payment_wallet_id||request.payment_inventory_id||!request.production_asset_ids.empty())return {false,{},common_failure("UnexpectedPoliticalExecutionAccounts").error};
 auto terms=supply_adoption_detail::terms(x,r,original,*request.supply_policy_fact_id,approval);if(!terms.ok)return {false,{},terms.error};Integer tick=x.common.tick(),sequence=x.common.state().next_instance_id;
 if(approval){const auto* saved=x.state.approvals.find(*approval);if(!saved)return {false,{},common_failure("UnknownPoliticalTakeoverApproval",*approval).error};tick=saved->tick;const auto number=organization_instance_sequence(*approval,"political_takeover");if(!number)return {false,{},common_failure("InvalidPoliticalTakeoverState",*approval).error};sequence=*number;}
 if(tick>terms.value.through_tick)return {false,{},common_failure("PoliticalSupplyPolicyExpired").error};
 // 批准只收紧派生状态；首个新观察必须等下一半日，最后有效当期已无可执行边界。
 if(tick==terms.value.through_tick)return {false,{},common_failure("PoliticalSupplyPolicyNoExecutableTick").error};auto allowed=supply_adoption_detail::active(x,r,terms.value,tick,sequence,approval.value_or(StableId{}));if(!allowed.ok)return {false,{},allowed.error};if(!allowed.value)return {false,{},common_failure("PoliticalSupplyPolicyAuthorityChanged").error};
 auto pending=request;pending.disposition=PoliticalResponsibilityDisposition::suspend;pending.supply_policy_fact_id.reset();auto out=takeover_detail::derive(original,pending,r.successor_organization_id,r.former_organization_id);if(!out.ok)return out;
 out.value.disposition=PoliticalResponsibilityDisposition::assume_by_successor;out.value.execution_principal=SubjectRef{SubjectKind::organization,r.successor_organization_id};return out;
}
Status validate_political_supply_adoption_bindings(const PoliticalTakeoverContext& x){
 if(bool(x.supply_adoptions)!=bool(x.supply_policies))return common_failure("MissingSupplyPolicyPair");if(!x.supply_adoptions)return common_success();if(x.supply_adoptions->version!=1)return common_failure("UnsupportedPoliticalSupplyAdoptionVersion");
 std::set<StableId> expected;
 for(const auto& row:x.state.decisions.rows())if(row.second.decision.responsibility.kind==PoliticalResponsibilityKind::supply_source&&row.second.decision.disposition==PoliticalResponsibilityDisposition::assume_by_successor){expected.insert(row.first);const auto* binding=x.supply_adoptions->bindings.find(row.first);if(!binding||binding->decision_fact_id!=row.first||binding->policy_issue_fact_id.empty())return common_failure("MissingPoliticalSupplyAdoption",row.first);}
 for(const auto& row:x.supply_adoptions->bindings.rows())if(row.first!=row.second.decision_fact_id||!expected.count(row.first))return common_failure("OrphanPoliticalSupplyAdoption",row.first);
 return common_success();
}
}
