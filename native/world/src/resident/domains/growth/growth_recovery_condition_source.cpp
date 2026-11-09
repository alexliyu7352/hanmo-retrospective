#include "resident/domains/growth/growth_model.hpp"
// 原credit头早于Actor写，但仅真实捕获序可证明写入顺序；本读不从头序制造操作。
#include "growth_recovery_condition_source.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
constexpr Integer maximum=9007199254740991LL;
Integer allocated(const CommonState& c,const StableId& id,const StableId& prefix){const auto key=prefix+":";if(id.rfind(key,0)!=0)return -1;const auto digits=id.substr(key.size());Integer n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);return r.ec==std::errc{}&&r.ptr==digits.data()+digits.size()&&n>0&&n<c.next_instance_id&&std::to_string(n)==digits?n:-1;}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Outcome<Integer> through(const CommonState& c,const DynamicStepInput* phase){if(c.identity.tick<0||c.identity.tick>maximum)return {false,{},common_failure("GrowthRecoveryConditionClockInvalid").error};if(!phase)return {true,c.identity.tick,{}};if(c.identity.tick==maximum||phase->from_tick!=c.identity.tick||phase->to_tick!=c.identity.tick+1)return {false,{},common_failure("GrowthRecoveryConditionPhaseInvalid").error};return {true,phase->to_tick,{}};}
}
Outcome<GrowthPersonalConditionCause> read_growth_recovery_condition_cause(const CommonState& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& state,const StableId& id,const DynamicStepInput* phase){
    auto fail=[&](const StableId& code)->Outcome<GrowthPersonalConditionCause>{return {false,{},common_failure(code,id).error};};auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};
    const auto* head=c.facts.find(id);const auto* f=state.credits.find(id);
    if(allocated(c,id,"recovery")<0||!head||head->id!=id||head->kind!="personal_recovery_credit"||head->domain!="personal_injury_recovery"||!f||f->kind!=head->kind)return fail("GrowthRecoveryConditionSourceRequired");
    const auto* service=state.services.find(f->action_id);const auto* original=growth.life_activities.find(f->action_id);const auto* action_head=c.facts.find(f->action_id);const auto action_number=allocated(c,f->action_id,"action");
    if(!service||!original||!action_head||action_head->id!=f->action_id||action_head->kind!="life_activity"||action_head->domain!="growth"||action_number<0||action_number>=allocated(c,id,"recovery"))return fail("GrowthRecoveryConditionServiceRequired");
    const auto& order=c.facts.order();if(std::find(order.begin(),order.end(),f->action_id)>=std::find(order.begin(),order.end(),id))return fail("GrowthRecoveryConditionSourceOrder");
    const auto& v=original->view;const auto* terms=std::get_if<ServiceTerms>(&original->terms);const auto* selected=catalog.service_roles.find(service->role_id);
    if(!selected||*selected!=service->method||!terms||v.kind!=LifeActivityKind::service||v.activity!="rest"||v.fact_id!=f->action_id||v.action_id!=f->action_id||v.actor_id!=service->actor_id||v.provider_actor_id!=service->provider_actor_id||original->context.role_id!=service->role_id||original->context.place_id!=service->place_id||original->context.provider_actor_id!=service->provider_actor_id||service->actor_id.empty()||service->actor_id==service->provider_actor_id||!c.actors.find(service->actor_id)||!c.actors.find(service->provider_actor_id)||service->duration<1||service->duration>720||terms->duration!=service->duration||v.start_tick!=service->start_tick||service->start_tick<0||service->start_tick>maximum-service->duration||f->ordinal<1||f->ordinal>service->duration||f->tick!=service->start_tick+f->ordinal||f->tick>clock.value)return fail("GrowthRecoveryConditionSourceInvalid");
    if(f->tick>c.identity.tick&&(!phase||std::find(phase->existing_action_ids.begin(),phase->existing_action_ids.end(),f->action_id)==phase->existing_action_ids.end()))return fail("GrowthRecoveryConditionPhaseActionRequired");
    if(f->offered_points!=growth_recovery_service_points(service->method))return fail("GrowthRecoveryConditionPointsMismatch");auto math=apply_growth_recovery_points(f->before,f->offered_points);if(!math.ok||math.value.after.injury!=f->after.injury||math.value.after.points!=f->after.points||math.value.used_points!=f->used_points)return fail("GrowthRecoveryConditionPointsMismatch");
    for(const auto& row:state.credits.rows())if(row.first!=id&&row.second.action_id==f->action_id&&row.second.ordinal==f->ordinal)return fail("GrowthRecoveryConditionDuplicatePeriod");
    GrowthPersonalConditionCause out;out.source_identity=c.identity;out.business={GrowthPersonalConditionBusiness::recovery_credit,f->action_id,f->tick,f->ordinal};out.actual_fact_id=id;out.actor_ids={service->actor_id};return {true,std::move(out),{}};
}
Status validate_growth_recovery_condition_effects(const CommonState& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& state,const GrowthPersonalConditionCause& cause,const std::vector<GrowthPersonalConditionActorEffect>& effects,const DynamicStepInput* phase){
    auto exact=read_growth_recovery_condition_cause(c,growth,catalog,state,cause.actual_fact_id,phase);if(!exact.ok)return {false,{},exact.error};const auto& e=exact.value;
    if(!identity(cause.source_identity,e.source_identity)||cause.business.kind!=e.business.kind||cause.business.original_business_key!=e.business.original_business_key||cause.business.effective_tick!=e.business.effective_tick||cause.business.ordinal!=e.business.ordinal||!cause.actual_completion_id.empty()||cause.actor_ids!=e.actor_ids||effects.size()!=1)return common_failure("GrowthRecoveryConditionCauseMismatch",cause.actual_fact_id);
    const auto& row=effects.front();const auto* credit=state.credits.find(e.actual_fact_id);const auto limit=growth_injury_hp_limit(credit->after.injury);
    if(row.actor_id!=e.actor_ids.front()||row.before.fatigue||row.after.fatigue||!row.before.hp||!row.after.hp||*row.before.hp<0||*row.before.hp>100||*row.after.hp!=std::min<Integer>(*row.before.hp,limit))return common_failure("GrowthRecoveryConditionEffectMismatch",row.actor_id);return common_success();
}
Outcome<std::vector<StableId>> read_growth_recovery_condition_cause_ids(const CommonState& c,const GrowthState& growth,const GrowthRecoveryCatalog& catalog,const GrowthRecoveryState& state,const DynamicStepInput* phase){
    auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};for(const auto& row:c.facts.rows())if(row.second.kind=="personal_recovery_credit"&&!state.credits.find(row.first))return {false,{},common_failure("GrowthRecoveryConditionSourceRequired",row.first).error};
    std::vector<StableId> out;for(const auto& row:state.credits.rows()){auto source=read_growth_recovery_condition_cause(c,growth,catalog,state,row.first,phase);if(!source.ok)return {false,{},source.error};out.push_back(row.first);}const auto& order=c.facts.order();std::sort(out.begin(),out.end(),[&](const auto& a,const auto& b){return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);});return {true,std::move(out),{}};
}
}
