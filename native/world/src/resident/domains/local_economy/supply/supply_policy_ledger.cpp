// 只核新经营账与原供给事实；此层不回调政治批准，供双方完整冷验建立无环前置。
#include "supply_policy_codec.hpp"
#include "resident/domains/affairs/affairs_policy_validation.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using Positions=std::map<StableId,std::size_t>;
// 精确实例前缀及十进制编号同时核对，不允许别域事实冒充本账来源。
std::optional<Integer> sequence(const CommonStore& c,const StableId& id,const StableId& prefix){
 const auto start=prefix+":";if(id.rfind(start,0)!=0)return {};const auto digits=id.substr(start.size());Integer n=0;
 const auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);
 if(r.ec!=std::errc{}||r.ptr!=digits.data()+digits.size()||n<=0||n>=c.state().next_instance_id||std::to_string(n)!=digits)return {};return n;
}
bool header(const CommonStore& c,const StableId& id,const char* prefix,const char* kind,const char* domain){const auto* f=c.state().facts.find(id);return f&&f->id==id&&f->kind==kind&&f->domain==domain&&sequence(c,id,prefix).has_value();}
Positions positions(const CommonStore& c){Positions p;std::size_t i=0;for(const auto& r:c.state().facts.rows())p.emplace(r.first,i++);return p;}
bool before(const Positions& p,const StableId& a,const StableId& b){auto x=p.find(a),y=p.find(b);return x!=p.end()&&y!=p.end()&&x->second<y->second;}
bool same_policy(const NeedPolicy& a,const NeedPolicy& b){return compare_boundary(dynamic_economy_codec::encode(a),dynamic_economy_codec::encode(b)).exact();}
// 当前只允许原clock及其真实单半日显式阶段；不会从某条未来记录推导观察时钟。
Status phase_valid(const EconomicSupplyPendingContext& x){const auto tick=x.common.tick();if(tick<0||(x.phase&&(tick==INT64_MAX||x.phase->from_tick!=tick||x.phase->to_tick!=tick+1)))return common_failure("EconomicSupplyPhaseBoundaryMismatch");return common_success();}
Outcome<const LocalSupplyRegistration*> registration(const EconomicSupplyPendingContext& x,const LocalSupplySource& source){
 const LocalSupplyRegistration* found=nullptr;
 for(const auto& row:x.economy.supply_registrations.rows())if(row.second.source_key==source.source_key){const auto& r=row.second;
  if(found||row.first!=r.id||r.organization_id!=source.organization_id||r.site_id!=source.site_id||r.inventory_id!=source.inventory_id||r.item_id!=source.item_id||r.tick<0||r.tick>x.common.tick()||!header(x.common,r.id,"local_supply","local_supply_source","local_economy"))return {false,nullptr,common_failure("InvalidEconomicSupplyOriginalRegistration",row.first).error};found=&r;}
 if(!found)return {false,nullptr,common_failure("MissingEconomicSupplyOriginalRegistration",source.source_key).error};return {true,found,{}};
}
EconomicSupplyPolicyTermsSource terms(const EconomicSupplyPendingContext& x,const EconomicSupplyPolicyRecord& r){return {x.common.state().identity,r.id,r.author_policy_id,r.registration_fact_id,r.original_source_key,r.original_source_fact_id,r.policy,r.issued_tick,r.earliest_effective_tick,r.through_tick,r.target_amount,r.authority,r.grant_id,quote_need_policy_commitment(r.policy).value.total};}
// 历史批准必须已有真实三元头；不接受随意before_fact来跳过后来发行。
Status approval_boundary(const EconomicSupplyPendingContext& x,const std::optional<StableId>& id){if(id&&!header(x.common,*id,"political_takeover","political_public_responsibilities_assumed","politics"))return common_failure("InvalidEconomicSupplyApprovalBoundary",*id);return common_success();}
}
Outcome<NeedPolicy> freeze_economic_supply_need_policy(const EconomicSupplyPolicyDefinition& d,const LocalSupplySource& source,const StableId& id){
 if(d.id.empty()||d.original_source_key!=source.source_key||id.empty()||d.target_amount<=0||d.valid_ticks<1||d.valid_ticks>720)return {false,{},common_failure("InvalidEconomicSupplyDefinition",d.id).error};
 NeedPolicy p;p.id=p.source_key=p.semantic_key=id;p.organization_id=d.organization_id;p.site_id=source.site_id;p.inventory_id=source.inventory_id;p.item_id=source.item_id;p.unit=d.unit;p.template_id=d.template_id;p.terms_version=d.terms_version;p.funding_wallet_id=d.funding_wallet_id;p.reward_wallet_id=d.reward_wallet_id;p.measure=d.measure;p.max_claim_quantity=d.max_claim_quantity;p.purchase_budget_per_unit=d.purchase_budget_per_unit;p.reward_per_unit=d.reward_per_unit;p.reward_total=d.reward_total;p.deadline_ticks=d.deadline_ticks;p.accept_role=d.accept_role;p.report_role=d.report_role;
 auto valid=validate_need_policy(p);if(!valid.ok)return {false,{},valid.error};return {true,std::move(p),{}};
}
Status validate_economic_supply_policy_ledger(const EconomicSupplyPendingContext& x){
 auto phase=phase_valid(x);if(!phase.ok)return phase;const auto& c=x.common;const auto& s=x.state;
 if(x.catalog.version!=1||s.version!=1||!x.economy_catalog.enabled)return common_failure("UnsupportedEconomicSupplyPolicyVersion");
 if(s.initialized_tick<0||s.initialized_tick>c.tick()||s.last_observation_tick<s.initialized_tick||s.last_observation_tick>c.tick())return common_failure("InvalidEconomicSupplyClock");
 const auto pos=positions(c);
 for(const auto& row:x.catalog.policies.rows()){const auto& d=row.second;const auto* source=x.economy_catalog.supply_sources.find(d.original_source_key);
  if(row.first!=d.id||!source||source->source_key!=d.original_source_key||!c.inventory(source->inventory_id)||!c.item_definition(source->item_id)||!x.common_catalog.places.find(source->site_id)||std::find(x.common_catalog.organization_ids.begin(),x.common_catalog.organization_ids.end(),d.organization_id)==x.common_catalog.organization_ids.end()||!c.wallet(d.funding_wallet_id)||!c.wallet(d.reward_wallet_id)||!x.common_catalog.roles.find(d.accept_role)||!x.common_catalog.roles.find(d.report_role))return common_failure("InvalidEconomicSupplyCatalog",row.first);
  auto frozen=freeze_economic_supply_need_policy(d,*source,"definition");if(!frozen.ok)return {false,{},frozen.error};auto original=registration(x,*source);if(!original.ok)return {false,{},original.error};
 }
 for(const auto& row:s.policies.rows()){const auto& r=row.second;const auto* d=x.catalog.policies.find(r.author_policy_id);const auto* source=d?x.economy_catalog.supply_sources.find(d->original_source_key):nullptr;
  if(row.first!=r.id||!d||!source||r.original_source_key!=source->source_key||r.grant_id.empty()||r.authority.organization_id!=d->organization_id||r.authority.office_id.empty()||r.authority.authority_epoch<0||!c.actor(r.authority.actor_id)||!header(c,r.id,economic_supply_policy_prefix,economic_supply_policy_issue_kind,economic_supply_policy_module)||!header(c,r.registration_fact_id,"political_takeover_pending","political_public_responsibilities_pending","politics")||!before(pos,r.registration_fact_id,r.id))return common_failure("InvalidEconomicSupplyPolicyRecord",row.first);
  auto registered=registration(x,*source);if(!registered.ok)return {false,{},registered.error};
  if(r.original_source_fact_id!=registered.value->id||!before(pos,r.original_source_fact_id,r.registration_fact_id)||r.issued_tick<s.initialized_tick||r.issued_tick>c.tick()||r.issued_tick>INT64_MAX-d->valid_ticks||r.earliest_effective_tick!=r.issued_tick+1||r.through_tick!=r.issued_tick+d->valid_ticks||r.target_amount!=d->target_amount||r.public_report!=d->public_report)return common_failure("InvalidEconomicSupplyPolicySource",r.id);
  auto frozen=freeze_economic_supply_need_policy(*d,*source,r.id);if(!frozen.ok)return {false,{},frozen.error};if(!same_policy(r.policy,frozen.value))return common_failure("EconomicSupplyFrozenTermsChanged",r.id);
 }
 std::map<StableId,std::pair<Integer,std::size_t>> latest;
 for(const auto& row:s.observations.rows()){const auto& r=row.second;const auto& value=r.observation;const auto& o=value.observation;const auto* p=s.policies.find(r.policy_fact_id);
  if(row.first!=value.id||!p||!header(c,value.id,economic_supply_observation_prefix,economic_supply_observation_kind,economic_supply_policy_module)||!before(pos,p->id,value.id)||value.tick<p->earliest_effective_tick||value.tick>p->through_tick||value.tick>s.last_observation_tick||value.publicly_visible!=p->public_report||o.source_key!=p->id||o.organization_id!=p->policy.organization_id||o.site_id!=p->policy.site_id||o.inventory_id!=p->policy.inventory_id||o.item_id!=p->policy.item_id||o.source_fact_id!=value.id||o.target_amount!=p->target_amount||o.physical_amount<0||o.source_revision<0)return common_failure("InvalidEconomicSupplyObservation",row.first);
  const auto rank=pos.at(value.id);auto prior=latest.find(p->id);if(prior!=latest.end()&&(prior->second.first>value.tick||prior->second.second>=rank))return common_failure("InvalidEconomicSupplyObservationOrder",row.first);latest[p->id]={value.tick,rank};
 }
 // 所有本模块头都必须有唯一完整载荷，漏账不能被枚举空集合视作没有责任。
 for(const auto& row:c.state().facts.rows()){const auto& f=row.second;const bool issue=row.first.rfind(StableId(economic_supply_policy_prefix)+":",0)==0;const bool observation=row.first.rfind(StableId(economic_supply_observation_prefix)+":",0)==0;
  if(f.domain==economic_supply_policy_module||issue||observation){if(f.domain!=economic_supply_policy_module||(!issue&&!observation)||(issue&&!s.policies.find(row.first))||(observation&&!s.observations.find(row.first)))return common_failure("EconomicSupplyHistoryUnavailable",row.first);}
 }
 return common_success();
}
Outcome<std::optional<EconomicSupplyPolicyTermsSource>> EconomicSupplyPendingAccess::economic_supply_policy_terms(const StableId& id,const std::optional<StableId>& boundary)const{
 auto valid=validate_economic_supply_policy_ledger(context_);if(!valid.ok)return {false,{},valid.error};valid=approval_boundary(context_,boundary);if(!valid.ok)return {false,{},valid.error};const auto* r=context_.state.policies.find(id);if(!r)return {true,{}, {}};if(boundary&&!before(positions(context_.common),id,*boundary))return {true,{}, {}};return {true,terms(context_,*r),{}};
}
Outcome<std::optional<EconomicSupplyPolicyTermsSource>> EconomicSupplyPendingAccess::economic_supply_pending_policy(const StableId& registered,const StableId& source,const std::optional<StableId>& boundary)const{
 auto valid=validate_economic_supply_policy_ledger(context_);if(!valid.ok)return {false,{},valid.error};valid=approval_boundary(context_,boundary);if(!valid.ok)return {false,{},valid.error};const auto pos=positions(context_.common);const EconomicSupplyPolicyRecord* selected=nullptr;
 for(const auto& row:context_.state.policies.rows()){const auto& r=row.second;if(r.registration_fact_id!=registered||r.original_source_key!=source||(boundary&&!before(pos,r.id,*boundary)))continue;if(!selected||pos.at(selected->id)<pos.at(r.id))selected=&r;}
 if(!selected)return {true,{}, {}};return {true,terms(context_,*selected),{}};
}
}
