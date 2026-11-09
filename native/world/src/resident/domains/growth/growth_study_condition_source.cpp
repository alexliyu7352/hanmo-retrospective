// 原暂停记录给出精确课时钟，原Growth自身study_fatigue给出基线；不复制XP或疲劳算法。
#include "growth_study_condition_source.hpp"
#include "growth_internal.hpp"
#include <algorithm>
#include <charconv>
namespace hanmo::resident {namespace {
constexpr Integer maximum=9007199254740991LL;
bool allocated(const CommonState& c,const StableId& id){if(id.rfind("action:",0)!=0)return false;const auto digits=id.substr(7);Integer n=0;auto r=std::from_chars(digits.data(),digits.data()+digits.size(),n);return r.ec==std::errc{}&&r.ptr==digits.data()+digits.size()&&n>0&&n<c.next_instance_id&&std::to_string(n)==digits;}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
Outcome<Integer> through(const CommonState& c,const DynamicStepInput* phase){if(c.identity.tick<0||c.identity.tick>maximum)return {false,{},common_failure("GrowthStudyConditionClockInvalid").error};if(!phase)return {true,c.identity.tick,{}};if(c.identity.tick==maximum||phase->from_tick!=c.identity.tick||phase->to_tick!=c.identity.tick+1)return {false,{},common_failure("GrowthStudyConditionPhaseInvalid").error};return {true,phase->to_tick,{}};}
Outcome<Integer> lesson_tick(const StudyFact& f,Integer ordinal,Integer now){
    auto fail=[]()->Outcome<Integer>{return {false,{},common_failure("GrowthStudyConditionPauseInvalid").error};};
    if(f.start_tick<0||ordinal<1||ordinal>Integer(f.lessons.size())||f.start_tick>maximum-ordinal)return fail();
    Integer tick=f.start_tick+ordinal,elapsed=0,last_lessons=0;
    if(f.pauses){if(f.pauses->empty()||!growth::study_pause_supported(f))return fail();
        for(std::size_t n=0;n<f.pauses->size();++n){const auto& pause=(*f.pauses)[n];
            if(pause.after_lessons<last_lessons||pause.after_lessons<0||pause.after_lessons>Integer(f.lessons.size())||pause.after_lessons>=f.terms.duration||pause.start_tick<f.start_tick||pause.start_tick>now||pause.start_tick-f.start_tick!=pause.after_lessons+elapsed)return fail();
            if(pause.end_tick==-1){if(n+1!=f.pauses->size()||f.phase!="started"||pause.after_lessons!=Integer(f.lessons.size())||pause.after_lessons<ordinal)return fail();}
            else{if(pause.end_tick<pause.start_tick||pause.end_tick>now||pause.student_fatigue<0||pause.student_fatigue>100||pause.teacher_fatigue<0||pause.teacher_fatigue>100)return fail();const auto spent=pause.end_tick-pause.start_tick;if(spent>maximum-elapsed)return fail();elapsed+=spent;if(pause.after_lessons<ordinal){if(spent>maximum-tick)return fail();tick+=spent;}}
            last_lessons=pause.after_lessons;
        }
    }
    if(tick>now)return fail();return {true,tick,{}};
}
}
Outcome<GrowthPersonalConditionCause> read_growth_study_condition_cause(const CommonState& c,const GrowthState& s,const StableId& id,Integer ordinal,const DynamicStepInput* phase){
    auto fail=[&](const StableId& code)->Outcome<GrowthPersonalConditionCause>{return {false,{},common_failure(code,id).error};};auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};
    const auto* head=c.facts.find(id);const auto* data=s.facts.find(id);const auto* f=data?std::get_if<StudyFact>(data):nullptr;
    if(!allocated(c,id)||!head||head->id!=id||head->kind!="skill_study"||head->domain!="cultivation"||!f)return fail("GrowthStudyConditionSourceRequired");
    const auto& t=f->terms;if((f->phase!="started"&&f->phase!="completed")||t.duration<2||t.duration>6||f->lessons.size()>std::size_t(t.duration)||ordinal<1||ordinal>Integer(f->lessons.size())||f->actor_id!=t.student_id||t.student_id.empty()||t.teacher_id.empty()||t.student_id==t.teacher_id||!c.actors.find(t.student_id)||!c.actors.find(t.teacher_id)||t.student_fatigue<0||t.student_fatigue>100||t.teacher_fatigue<0||t.teacher_fatigue>100)return fail("GrowthStudyConditionSourceInvalid");
    auto tick=lesson_tick(*f,ordinal,clock.value);if(!tick.ok)return {false,{},tick.error};
    const auto& lesson=f->lessons[std::size_t(ordinal-1)];const bool segmented=growth::study_segmented(*f);
    if(bool(lesson.segment)!=segmented||(lesson.segment&&(lesson.segment->ordinal!=ordinal||lesson.segment->settled_tick!=tick.value))||lesson.fatigue_before!=growth::study_fatigue(*f,ordinal-1))return fail("GrowthStudyConditionLessonMismatch");
    if(tick.value>c.identity.tick&&(!phase||std::find(phase->existing_action_ids.begin(),phase->existing_action_ids.end(),id)==phase->existing_action_ids.end()))return fail("GrowthStudyConditionPhaseActionRequired");
    GrowthPersonalConditionCause out;out.source_identity=c.identity;out.business={GrowthPersonalConditionBusiness::study_lesson,id,tick.value,ordinal};out.actual_fact_id=id;out.actor_ids={t.student_id,t.teacher_id};return {true,std::move(out),{}};
}
Status validate_growth_study_condition_effects(const CommonState& c,const GrowthState& s,const GrowthPersonalConditionCause& cause,const std::vector<GrowthPersonalConditionActorEffect>& effects,const DynamicStepInput* phase){
    auto expected=read_growth_study_condition_cause(c,s,cause.actual_fact_id,cause.business.ordinal,phase);if(!expected.ok)return {false,{},expected.error};const auto& e=expected.value;
    if(!identity(cause.source_identity,e.source_identity)||cause.business.kind!=e.business.kind||cause.business.original_business_key!=e.business.original_business_key||cause.business.effective_tick!=e.business.effective_tick||!cause.actual_completion_id.empty()||cause.actor_ids!=e.actor_ids||effects.size()!=2)return common_failure("GrowthStudyConditionCauseMismatch",cause.actual_fact_id);
    const auto& f=std::get<StudyFact>(*s.facts.find(e.actual_fact_id));
    // 原Study只保存学生本次疲劳；教师实际before必须来自完整条件重放，不能由最初条款倒填。
    for(std::size_t n=0;n<2;++n){const auto& row=effects[n];const Integer delta=n==0?6:2;
        if(row.actor_id!=e.actor_ids[n]||row.before.hp||row.after.hp||!row.before.fatigue||!row.after.fatigue||*row.before.fatigue<0||*row.before.fatigue>100||(n==0&&*row.before.fatigue!=f.lessons[std::size_t(e.business.ordinal-1)].fatigue_before)||*row.after.fatigue!=std::min<Integer>(100,*row.before.fatigue+delta))return common_failure("GrowthStudyConditionEffectMismatch",row.actor_id);
    }return common_success();
}
Outcome<std::vector<GrowthPersonalConditionCause>> read_growth_study_condition_causes(const CommonState& c,const GrowthState& s,const DynamicStepInput* phase){
    auto clock=through(c,phase);if(!clock.ok)return {false,{},clock.error};
    for(const auto& row:c.facts.rows())if(row.second.kind=="skill_study"){const auto* f=s.facts.find(row.first);if(!f||!std::get_if<StudyFact>(f))return {false,{},common_failure("GrowthStudyConditionSourceRequired",row.first).error};}
    std::vector<GrowthPersonalConditionCause> out;
    for(const auto& row:s.facts.rows())if(const auto* f=std::get_if<StudyFact>(&row.second))for(Integer n=1;n<=Integer(f->lessons.size());++n){auto source=read_growth_study_condition_cause(c,s,row.first,n,phase);if(!source.ok)return {false,{},source.error};out.push_back(std::move(source.value));}
    const auto& order=c.facts.order();std::sort(out.begin(),out.end(),[&](const auto& a,const auto& b){auto left=std::find(order.begin(),order.end(),a.actual_fact_id),right=std::find(order.begin(),order.end(),b.actual_fact_id);return left==right?a.business.ordinal<b.business.ordinal:left<right;});return {true,std::move(out),{}};
}
}
