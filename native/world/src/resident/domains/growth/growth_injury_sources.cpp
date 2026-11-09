// 同一真实Common头和原tick界定伤因；数值ID仅验分配，前后次序取原插入顺序。
#include "growth_injury_sources.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
namespace hanmo::resident {namespace {
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool allocated(const CommonStore& c,const StableId& id,const StableId& prefix){const auto key=prefix+":";if(id.rfind(key,0)!=0)return false;const auto digits=id.substr(key.size());Integer n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);return r.ec==std::errc{}&&r.ptr==digits.data()+digits.size()&&n>0&&n<c.state().next_instance_id&&std::to_string(n)==digits;}
bool valid_effect(const GrowthInjuryCause& x){return x.kind==GrowthInjuryCauseKind::military_wound?x.magnitude==1:x.kind==GrowthInjuryCauseKind::personal_defeat&&x.magnitude>=1&&x.magnitude<=3;}
}
Status validate_growth_injury_cause(const CommonStore& c,const GrowthInjuryCause& x,const DynamicStepInput* phase){
    Integer through=c.tick();if(phase){if(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1)return common_failure("GrowthInjuryPhaseInvalid");through=phase->to_tick;}
    if(!same_identity(c.state().identity,x.source_identity)||!c.actor(x.actor_id)||x.tick<0||x.tick>through||!valid_effect(x))return common_failure("InvalidGrowthInjuryCause",x.source_fact_id);
    const bool military=x.kind==GrowthInjuryCauseKind::military_wound;const auto* fact=c.state().facts.find(x.source_fact_id);
    if(!allocated(c,x.source_fact_id,military?"military":"battle")||!fact||fact->id!=x.source_fact_id||fact->kind!=(military?"military_casualties":"personal_battle")||fact->domain!=(military?"military":"personal"))return common_failure("GrowthInjuryCauseSourceMismatch",x.source_fact_id);
    return common_success();
}
Status validate_growth_injury_cause_sequence(const CommonStore& c,const StableId& actor,const std::vector<GrowthInjuryCause>& values,const DynamicStepInput* phase){
    // 空集合仍须核时钟与人物；源读取失败不能被包装成合法empty。
    if(!c.actor(actor))return common_failure("UnknownRecoveryActor",actor);
    if(phase&&(c.tick()<0||c.tick()==std::numeric_limits<Integer>::max()||phase->from_tick!=c.tick()||phase->to_tick!=c.tick()+1))return common_failure("GrowthInjuryPhaseInvalid");
    const auto& order=c.state().facts.order();std::size_t previous=0;Integer tick=-1;bool first=true;
    for(const auto& value:values){auto valid=validate_growth_injury_cause(c,value,phase);if(!valid.ok)return valid;auto at=std::find(order.begin(),order.end(),value.source_fact_id);const auto index=std::size_t(at-order.begin());if(value.actor_id!=actor||at==order.end()||(!first&&index<=previous)||value.tick<tick)return common_failure("GrowthInjuryCauseSequenceInvalid",value.source_fact_id);first=false;previous=index;tick=value.tick;}
    return common_success();
}
Outcome<GrowthRecoveryProgress> apply_growth_injury_cause(const GrowthRecoveryProgress& before,const GrowthInjuryCause& x){
    if(!valid_effect(x)||before.injury<0||before.injury>3)return {false,{},common_failure("InvalidGrowthInjuryCause",x.source_fact_id).error};
    const auto injury=x.kind==GrowthInjuryCauseKind::military_wound?std::max<Integer>(1,before.injury):std::min<Integer>(3,before.injury+x.magnitude);
    // 真正非零的新伤原因打断本级未完成疗程，即使原伤级已封顶或max(1)未再升档。
    auto changed=apply_growth_injury_increase(before,injury);if(changed.ok)changed.value.points=0;return changed;
}
}
