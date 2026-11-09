// 原作者Owner经真实占领链决定当时可否纳税；失产仅暂停，不把缴税责任转给现Owner。
#include "fiscal_taxpayer_continuity_internal.hpp"
#include <set>
namespace hanmo::resident::fiscal_taxpayer_continuity_detail {
namespace {
// 同tick捕获先后必须来自共同事实序，边界只认原财政三个固定事实种类。
bool before(const CommonStore& common,const StableId& first,const StableId& boundary){bool seen=false;for(const auto& row:common.state().facts.rows()){if(row.first==boundary)return seen;if(row.first==first)seen=true;}return false;}
Status boundary(const CommonStore& common,const StableId& id){const auto* header=common.state().facts.find(id);if(!header||header->id!=id||header->domain!="local_economy")return common_failure("InvalidFiscalTaxpayerBoundary",id);for(const auto* kind:{"fiscal_period_assessed","fiscal_source_assessed","fiscal_payment"})if(header->kind==kind&&id.rfind(StableId{kind}+":",0)==0)return common_success();return common_failure("InvalidFiscalTaxpayerBoundary",id);}
}
// ordinary cold只到current；半日调用显式from/current与to/current+1，不用max推断候选时间。
Status context_clock(const CommonStore& c,const FiscalSettlementState& fiscal,const FiscalTaxpayerContinuityContext& x){
 Integer horizon=c.tick();if(x.phase){if(c.tick()==INT64_MAX||x.phase->from_tick!=c.tick()||x.phase->to_tick!=c.tick()+1)return common_failure("FiscalTaxpayerContinuityPhaseMismatch");horizon=x.phase->to_tick;}
 if(x.state.version!=1||x.state.initialized_tick!=fiscal.initialized_tick||x.state.last_step_tick!=fiscal.last_step_tick||x.state.initialized_tick<0||x.state.initialized_tick>x.state.last_step_tick||x.state.last_step_tick<c.tick()||x.state.last_step_tick>horizon)return common_failure("FiscalTaxpayerContinuityClockMismatch");return common_success();
}
// 历史结果不与今天Owner等同；当前结果则逐容器核同Common，已收复也保完整真实源链。
Outcome<bool> available(const CommonStore& c,const FiscalTaxSource& source,const Owner& original,const FiscalTaxpayerContinuityContext& x,Integer tick,const std::optional<StableId>& boundary_id){
 auto fail=[&](const StableId& code)->Outcome<bool>{return {false,false,common_failure(code,source.id).error};};Integer horizon=c.tick();if(x.phase){if(c.tick()==INT64_MAX||x.phase->from_tick!=c.tick()||x.phase->to_tick!=c.tick()+1)return fail("FiscalTaxpayerContinuityPhaseMismatch");horizon=x.phase->to_tick;}
 if(tick<0||tick>horizon||(!boundary_id&&tick!=horizon))return fail("FiscalTaxpayerContinuityReadTickMismatch");if(boundary_id){auto checked=boundary(c,*boundary_id);if(!checked.ok)return {false,false,checked.error};}
 const auto* wallet=c.wallet(source.wallet_id);const auto* inventory=c.inventory(source.inventory_id);if(!wallet||!inventory)return fail("FiscalTaxpayerAssetsMissing");
 if(original.kind==Owner::Kind::household){if(source.taxpayer.kind!=SubjectKind::cohort||!owner_equal(wallet->owner,original)||!owner_equal(inventory->owner,original))return fail("FiscalTaxpayerHouseholdChanged");return {true,true,{}};}
 if(original.kind!=Owner::Kind::organization||source.taxpayer.kind!=SubjectKind::organization||source.taxpayer.id!=original.id)return fail("FiscalTaxpayerOriginalOwnerMismatch");
 auto actual=x.assets.occupation_fiscal_taxpayer_source(original,source.wallet_id,source.inventory_id,tick,boundary_id);if(!actual.ok)return {false,false,actual.error};const auto& value=actual.value;const auto& identity=c.state().identity;
 if(value.source_identity.world_id!=identity.world_id||value.source_identity.execution_epoch!=identity.execution_epoch||value.source_identity.revision!=identity.revision||value.source_identity.tick!=identity.tick||value.due_tick!=tick||!owner_equal(value.original_taxpayer,original)||value.wallet.id!=source.wallet_id||value.inventory.id!=source.inventory_id)return fail("FiscalTaxpayerControlIdentityMismatch");
 if(!boundary_id&&(!owner_equal(value.wallet.owner,wallet->owner)||!owner_equal(value.inventory.owner,inventory->owner)))return fail("FiscalTaxpayerCurrentOwnerMismatch");
 for(const auto* container:{&value.wallet,&value.inventory}){if(container->owner.kind!=Owner::Kind::organization||(!owner_equal(container->owner,original)&&container->occupation_fact_ids.empty()))return fail("FiscalTaxpayerOccupationRequired");std::set<StableId> seen;for(const auto& id:container->occupation_fact_ids){const auto* fact=c.state().facts.find(id);if(!fact||fact->kind!="military_occupation"||fact->domain!="military"||!seen.insert(id).second||(boundary_id&&!before(c,id,*boundary_id)))return fail("InvalidFiscalTaxpayerOccupationSource");}}
 return {true,owner_equal(value.wallet.owner,original)&&owner_equal(value.inventory.owner,original),{}};
}
// 整区新期在任一原税源失产时暂停，原空周期仍推进；不延后补征也不改变其余户的分数税权。
Outcome<std::vector<StableId>> blocked_sources(const CommonStore& c,const LocalEconomyState& economy,const FiscalDistrictProfile& district,const FiscalTaxpayerContinuityContext& x,Integer tick,const StableId& period){
 std::vector<StableId> out;for(const auto& row:district.sources.rows()){const auto& source=row.second;Owner owner;
  if(source.taxpayer.kind==SubjectKind::organization)owner={Owner::Kind::organization,source.taxpayer.id,{}};
  else if(source.taxpayer.kind==SubjectKind::cohort){const auto* cohort=economy.cohorts.find(source.taxpayer.id);const auto* household=cohort?economy.households.find(cohort->household_group_id):nullptr;if(!household||cohort->site_id!=district.site_id||household->wallet_id!=source.wallet_id||household->inventory_id!=source.inventory_id)return {false,{},common_failure("FiscalTaxpayerHouseholdSourceMismatch",source.id).error};owner={Owner::Kind::household,household->id,{}};}
  else return {false,{},common_failure("InvalidFiscalTaxpayer",source.id).error};
  auto can_pay=available(c,source,owner,x,tick,period);if(!can_pay.ok)return {false,{},can_pay.error};if(!can_pay.value)out.push_back(row.first);
 }return {true,std::move(out),{}};
}
// 附注以已有原周期fact为唯一键，绝不分配第二事实或改原财政visitor字节。
Status remember_pause(FiscalTaxpayerContinuityExecutionContext& x,const FiscalPeriodAssessment& period,const FiscalPolicySnapshot& policy,const std::vector<StableId>& blocked){
 if(blocked.empty()||period.id.empty()||policy.source_fact_id.empty()||x.state.suspended_periods.find(period.id))return common_failure("InvalidFiscalTaxpayerSuspension",period.id);
 if(!x.mutable_state.suspended_periods.insert(period.id,{period.id,period.district_id,policy.source_fact_id,period.due_tick,blocked}))return common_failure("DuplicateFiscalTaxpayerSuspension",period.id);return common_success();
}
// 过去失产必须在原空period前真实发生，回收后的当前Owner不能抹掉这份暂停原因。
Status validate_pause(const CommonStore& c,const LocalEconomyState& economy,const FiscalDistrictProfile& district,const FiscalPeriodAssessment& period,const FiscalPolicySnapshot& policy,const FiscalTaxpayerContinuityContext& x){
 const auto* pause=x.state.suspended_periods.find(period.id);if(!pause||pause->period_id!=period.id||pause->district_id!=period.district_id||pause->due_tick!=period.due_tick||pause->policy_source_fact_id!=policy.source_fact_id||period.policy||!period.source_assessment_ids.empty())return common_failure("FiscalTaxpayerSuspensionMismatch",period.id);
 auto blocked=blocked_sources(c,economy,district,x,period.due_tick,period.id);if(!blocked.ok)return {false,{},blocked.error};if(blocked.value.empty()||blocked.value!=pause->blocked_source_ids)return common_failure("FiscalTaxpayerSuspensionSourceMismatch",period.id);return common_success();
}
// 后验反向扫描禁止孤儿/重复附注；具体政策和占领链已由每个原周期验证点逐项核过。
Status validate_registry(const CommonStore& c,const FiscalSettlementState& fiscal,const FiscalTaxpayerContinuityContext& x){
 auto clock=context_clock(c,fiscal,x);if(!clock.ok)return clock;for(const auto& row:x.state.suspended_periods.rows()){const auto* period=fiscal.periods.find(row.first);if(row.first!=row.second.period_id||!period||period->policy||!period->source_assessment_ids.empty()||period->district_id!=row.second.district_id||period->due_tick!=row.second.due_tick)return common_failure("InvalidFiscalTaxpayerSuspensionRegistry",row.first);auto checked=boundary(c,row.first);if(!checked.ok)return checked;}return common_success();
}
}
