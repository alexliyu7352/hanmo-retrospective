// 草案发行只登记原条款，真正资金预约由S3本人认领原采购合同执行。
#include "supply_policy_internal.hpp"
#include "supply_policy_codec.hpp"
namespace hanmo::resident {
using namespace economic_supply_detail;
namespace {
// 显式原址绑定保证S6纯草案读与新增Common头观察同一个候选。
Status same_candidate(CommonStore& c,EconomicSupplyPolicyState& s,const EconomicSupplyPolicyContext& x){if(&c!=&x.ledger.common||&s!=&x.ledger.state)return common_failure("EconomicSupplyCandidateMismatch");if(x.ledger.phase)return common_failure("EconomicSupplyRequiresCommittedClock");return common_success();}
bool command_equal(const IssueEconomicSupplyPolicyCommand& a,const IssueEconomicSupplyPolicyCommand& b){return authority(a.authority,b.authority)&&a.registration_fact_id==b.registration_fact_id&&a.author_policy_id==b.author_policy_id;}
// 唯一事实分配入口；不会先预造未来事实ID来证明批准或历史权源。
Outcome<StableId> append(CommonStore& c,const char* prefix,const char* kind){auto id=c.next_id(prefix);if(!id.ok)return id;auto added=c.append_fact({id.value,kind,economic_supply_policy_module});if(!added.ok)return {false,{},added.error};return id;}
Outcome<EconomicSupplyPolicyReceipt> issue_candidate(CommonStore& c,EconomicSupplyPolicyState& state,const EconomicSupplyPolicyContext& x,const StableId& actor,const IssueEconomicSupplyPolicyCommand& command,bool standalone){
 auto same=same_candidate(c,state,x);if(!same.ok)return {false,{},same.error};auto quote=preview_economic_supply_policy(x,actor,command);if(!quote.ok)return {false,{},quote.error};
 std::optional<local_economy_detail::PhaseSavepoint> transaction;if(standalone){transaction.emplace(c);if(!transaction->token().ok)return {false,{},transaction->token().error};}local_economy_detail::CandidateStateGuard<EconomicSupplyPolicyState> guard(state);
 auto id=append(c,economic_supply_policy_prefix,economic_supply_policy_issue_kind);if(!id.ok)return {false,{},id.error};const auto& q=quote.value;const auto& d=*x.ledger.catalog.policies.find(command.author_policy_id);auto p=freeze_economic_supply_need_policy(d,*x.ledger.economy_catalog.supply_sources.find(d.original_source_key),id.value);if(!p.ok)return {false,{},p.error};
 EconomicSupplyPolicyRecord r{id.value,d.id,command.registration_fact_id,d.original_source_key,q.proposed.original_source_fact_id,q.authorization.grant_id,command.authority,p.value,c.tick(),q.proposed.earliest_effective_tick,q.proposed.through_tick,d.target_amount,d.public_report};if(!state.policies.insert(id.value,std::move(r)))return {false,{},common_failure("DuplicateEconomicSupplyPolicy",id.value).error};
 auto checked=validate_economic_supply_policies(x);if(!checked.ok)return {false,{},checked.error};if(transaction){checked=transaction->commit();if(!checked.ok)return {false,{},checked.error};}guard.commit();return {true,{id.value,command.registration_fact_id,d.original_source_key},{}};
}
Outcome<std::vector<SupplyObservation>> refresh_candidate(CommonStore& c,EconomicSupplyPolicyState& state,const EconomicSupplyPolicyContext& x,bool standalone){
 auto valid=same_candidate(c,state,x);if(!valid.ok)return {false,{},valid.error};valid=validate_economic_supply_policies(x);if(!valid.ok)return {false,{},valid.error};
 std::optional<local_economy_detail::PhaseSavepoint> transaction;if(standalone){transaction.emplace(c);if(!transaction->token().ok)return {false,{},transaction->token().error};}local_economy_detail::CandidateStateGuard<EconomicSupplyPolicyState> guard(state);std::vector<SupplyObservation> out;
 // 同候选S6纯草案读可能在下一政策重验整账，先在原Guard下标定本次稳定观察边界。
 state.last_observation_tick=c.tick();
 for(const auto& row:state.policies.rows()){const auto& r=row.second;auto permitted=current(x,r);if(!permitted.ok)return {false,{},permitted.error};if(!permitted.value.new_contracts_allowed)continue;const auto* inventory=c.inventory(r.policy.inventory_id);auto stock=physical(x,r.policy);if(!stock.ok)return {false,{},stock.error};const auto* old=latest(x,r.id);
  if(old&&old->observation.tick==c.tick()&&old->observation.observation.physical_amount==stock.value&&old->observation.observation.source_revision==inventory->revision){out.push_back(old->observation.observation);continue;}
  auto id=append(c,economic_supply_observation_prefix,economic_supply_observation_kind);if(!id.ok)return {false,{},id.error};SupplyObservation observed{r.id,r.policy.organization_id,r.policy.site_id,r.policy.inventory_id,r.policy.item_id,id.value,r.target_amount,stock.value,inventory->revision};state.observations.insert(id.value,{r.id,{id.value,observed,c.tick(),r.public_report}});out.push_back(std::move(observed));
 }
 valid=validate_economic_supply_policies(x);if(!valid.ok)return {false,{},valid.error};if(transaction){valid=transaction->commit();if(!valid.ok)return {false,{},valid.error};}guard.commit();return {true,std::move(out),{}};
}
}
Outcome<EconomicSupplyPolicyQuote> preview_economic_supply_policy(const EconomicSupplyPolicyContext& x,const StableId& actor,const IssueEconomicSupplyPolicyCommand& command){
 if(x.ledger.phase)return {false,{},common_failure("EconomicSupplyRequiresCommittedClock").error};auto valid=validate_economic_supply_policies(x);if(!valid.ok)return {false,{},valid.error};const auto& c=x.ledger.common;
 if(actor.empty()||actor!=command.authority.actor_id)return {false,{},common_failure("EconomicSupplyIssuerMismatch",actor).error};const auto* d=x.ledger.catalog.policies.find(command.author_policy_id);if(!d)return {false,{},common_failure("UnknownEconomicSupplyPolicy",command.author_policy_id).error};if(c.tick()>INT64_MAX-d->valid_ticks)return {false,{},common_failure("EconomicSupplyTickOverflow",d->id).error};const auto* source=x.ledger.economy_catalog.supply_sources.find(d->original_source_key);auto frozen=freeze_economic_supply_need_policy(*d,*source,"economic_supply_policy:quote");if(!frozen.ok)return {false,{},frozen.error};auto m=mandate(command.registration_fact_id,d->original_source_key,frozen.value);if(!m.ok)return {false,{},m.error};auto permission=x.authority.political_supply_policy_authority(command.authority,m.value);if(!permission.ok)return {false,{},permission.error};valid=proof(x,permission.value,m.value,command.authority,{},c.tick());if(!valid.ok)return {false,{},valid.error};
 StableId registered;for(const auto& row:x.ledger.economy.supply_registrations.rows())if(row.second.source_key==source->source_key)registered=row.first;
 EconomicSupplyPolicyQuote quote;quote.source_identity=c.state().identity;quote.actor_id=actor;quote.command=command;quote.authorization=permission.value;quote.proposed={c.state().identity,{},d->id,command.registration_fact_id,d->original_source_key,registered,frozen.value,c.tick(),c.tick()+1,c.tick()+d->valid_ticks,d->target_amount,command.authority,permission.value.grant_id,m.value.maximum_contract_commitment};return {true,std::move(quote),{}};
}
Outcome<EconomicSupplyPolicyReceipt> issue_economic_supply_policy(CommonStore& c,EconomicSupplyPolicyState& s,const EconomicSupplyPolicyContext& x,const StableId& actor,const IssueEconomicSupplyPolicyCommand& command){return issue_candidate(c,s,x,actor,command,true);}
Outcome<EconomicSupplyPolicyReceipt> issue_economic_supply_policy_in_candidate(CommonStore& c,EconomicSupplyPolicyState& s,const EconomicSupplyPolicyContext& x,const StableId& actor,const IssueEconomicSupplyPolicyCommand& command,const EconomicSupplyPolicyQuote& quote){
 if(!identity(c.state().identity,quote.source_identity)||quote.actor_id!=actor||!command_equal(command,quote.command))return {false,{},common_failure("StaleEconomicSupplyPolicyQuote").error};auto fresh=preview_economic_supply_policy(x,actor,command);if(!fresh.ok)return {false,{},fresh.error};if(fresh.value.authorization.grant_id!=quote.authorization.grant_id||!mandate_equal(fresh.value.authorization.mandate,quote.authorization.mandate)||!compare_boundary(dynamic_economy_codec::encode(fresh.value.proposed.policy),dynamic_economy_codec::encode(quote.proposed.policy)).exact()||fresh.value.proposed.earliest_effective_tick!=quote.proposed.earliest_effective_tick||fresh.value.proposed.through_tick!=quote.proposed.through_tick)return {false,{},common_failure("EconomicSupplyPolicyQuoteChanged").error};return issue_candidate(c,s,x,actor,command,false);
}
Outcome<std::vector<SupplyObservation>> refresh_economic_supply_observations(CommonStore& c,EconomicSupplyPolicyState& s,const EconomicSupplyPolicyContext& x){return refresh_candidate(c,s,x,true);}
Outcome<std::vector<SupplyObservation>> refresh_economic_supply_observations_in_candidate(CommonStore& c,EconomicSupplyPolicyState& s,const EconomicSupplyPolicyContext& x){return refresh_candidate(c,s,x,false);}
}
