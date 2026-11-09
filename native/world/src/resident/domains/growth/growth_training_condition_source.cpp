#include "resident/domains/growth/growth_model.hpp"
// 此来源只借唯一TrainingFact，不保存第二份疲劳，不用今日Actor值反推之前的delta。
#include "growth_training_condition_source.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
bool allocated(const CommonState& c,const StableId& id){if(id.rfind("action:",0)!=0)return false;const auto value=id.substr(7);Integer n=0;auto p=std::from_chars(value.data(),value.data()+value.size(),n);return p.ec==std::errc{}&&p.ptr==value.data()+value.size()&&n>0&&n<c.next_instance_id&&std::to_string(n)==value;}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Outcome<Integer> through(const CommonState& c,const DynamicStepInput* phase){if(c.identity.tick<0||c.identity.tick>9007199254740991LL)return {false,{},common_failure("GrowthTrainingConditionClockInvalid").error};if(!phase)return {true,c.identity.tick,{}};if(c.identity.tick==9007199254740991LL||phase->from_tick!=c.identity.tick||phase->to_tick!=c.identity.tick+1)return {false,{},common_failure("GrowthTrainingConditionPhaseInvalid").error};return {true,phase->to_tick,{}};}
}
Outcome<GrowthPersonalConditionCause> read_growth_training_condition_cause(const CommonState& c,const GrowthState& s,const StableId& id,const DynamicStepInput* phase){
    auto fail=[&](const StableId& code)->Outcome<GrowthPersonalConditionCause>{return {false,{},common_failure(code,id).error};};auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};
    const auto* head=c.facts.find(id);const auto* payload=s.facts.find(id);const auto* f=payload?std::get_if<TrainingFact>(payload):nullptr;
    if(!allocated(c,id)||!head||head->id!=id||head->kind!="training"||head->domain!="cultivation"||!f)return fail("GrowthTrainingConditionSourceRequired");
    const auto& t=f->training;const bool practice=f->action_kind=="practice_skill",teach=f->action_kind=="teach_skill";
    if(!practice&&!teach&&f->action_kind!="learn_skill")return fail("GrowthTrainingConditionKindInvalid");
    if(f->phase!="completed"||!f->success)return fail("GrowthTrainingConditionNoActorEffect");
    if(t.duration<1||t.duration>30||f->start_tick<0||f->start_tick>9007199254740991LL-t.duration||f->completed_tick!=f->start_tick+t.duration||f->completed_tick>clock.value||f->actor_id!=(teach?t.teacher_id:t.student_id)||t.student_id.empty()||!c.actors.find(t.student_id))return fail("GrowthTrainingConditionSourceInvalid");
    if(f->completed_tick>c.identity.tick&&(!phase||std::find(phase->existing_action_ids.begin(),phase->existing_action_ids.end(),id)==phase->existing_action_ids.end()))return fail("GrowthTrainingConditionPhaseActionRequired");
    if(practice){if(!t.teacher_id.empty()||t.duration>8||t.student_fatigue!=t.duration*6||t.teacher_fatigue!=0)return fail("GrowthTrainingConditionTermsInvalid");}
    else if(t.teacher_id.empty()||t.student_id==t.teacher_id||!c.actors.find(t.teacher_id)||t.student_fatigue!=t.duration*4||t.teacher_fatigue!=t.duration*2)return fail("GrowthTrainingConditionTermsInvalid");
    GrowthPersonalConditionCause out;out.source_identity=c.identity;out.business={GrowthPersonalConditionBusiness::training_completion,id,f->completed_tick,1};out.actual_fact_id=id;out.actor_ids.push_back(t.student_id);if(!t.teacher_id.empty())out.actor_ids.push_back(t.teacher_id);return {true,std::move(out),{}};
}
Status validate_growth_training_condition_effects(const CommonState& c,const GrowthState& s,const GrowthPersonalConditionCause& cause,const std::vector<GrowthPersonalConditionActorEffect>& actors,const DynamicStepInput* phase){
    auto exact=read_growth_training_condition_cause(c,s,cause.actual_fact_id,phase);if(!exact.ok)return {false,{},exact.error};const auto& e=exact.value;
    if(!identity(cause.source_identity,e.source_identity)||cause.business.kind!=e.business.kind||cause.business.original_business_key!=e.business.original_business_key||cause.business.effective_tick!=e.business.effective_tick||cause.business.ordinal!=e.business.ordinal||!cause.actual_completion_id.empty()||cause.actor_ids!=e.actor_ids||actors.size()!=e.actor_ids.size())return common_failure("GrowthTrainingConditionCauseMismatch",cause.actual_fact_id);
    const auto& t=std::get<TrainingFact>(*s.facts.find(e.actual_fact_id)).training;
    for(std::size_t n=0;n<actors.size();++n){const auto& row=actors[n];const auto delta=n==0?t.student_fatigue:t.teacher_fatigue;
        if(row.actor_id!=e.actor_ids[n]||row.before.hp||row.after.hp||!row.before.fatigue||!row.after.fatigue||*row.before.fatigue<0||*row.before.fatigue>100||*row.after.fatigue!=std::min<Integer>(100,*row.before.fatigue+delta))return common_failure("GrowthTrainingConditionEffectMismatch",row.actor_id);
    }
    return common_success();
}
Outcome<std::vector<StableId>> read_growth_training_condition_cause_ids(const CommonState& c,const GrowthState& s,const DynamicStepInput* phase){
    auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};
    // 原头缺payload不能因为本表无法枚举而被默默省略。
    for(const auto& row:c.facts.rows())if(row.second.kind=="training"){const auto* p=s.facts.find(row.first);if(!p||!std::get_if<TrainingFact>(p))return {false,{},common_failure("GrowthTrainingConditionSourceRequired",row.first).error};}
    std::vector<StableId> out;
    for(const auto& row:s.facts.rows())if(const auto* f=std::get_if<TrainingFact>(&row.second)){
        if(f->phase=="completed"&&f->success){auto source=read_growth_training_condition_cause(c,s,row.first,phase);if(!source.ok)return {false,{},source.error};out.push_back(row.first);}
    }
    const auto& order=c.facts.order();std::sort(out.begin(),out.end(),[&](const StableId& a,const StableId& b){return std::find(order.begin(),order.end(),a)<std::find(order.begin(),order.end(),b);});return {true,std::move(out),{}};
}
}
