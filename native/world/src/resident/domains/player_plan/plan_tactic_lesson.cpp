// 计划只能延续本人已接受的原教学义务；不得自动报名、选择战法或替教师声明。
#include "plan_binding.hpp"
#include "resident/kernel/world_bundle.hpp"
#include <algorithm>
namespace hanmo::resident {
bool player_plan_tactic_lesson_action(const WorldBundle& w,const StableId& action){
    return w.growth_tactic_lessons_enabled()&&std::any_of(w.growth_tactic_lessons.starts.rows().begin(),w.growth_tactic_lessons.starts.rows().end(),[&](const auto& row){return row.second.action_id==action;});
}
bool player_plan_tactic_lesson_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(action.kind!="tactic_lesson"||!player_plan_tactic_lesson_action(w,action.id))return false;
    auto original=read_growth_tactic_lesson_action_source(w.common,w.growth_tactic_lessons,action.actor_id,action.id);
    return original.ok&&!original.value.completion&&original.value.agreement.student_id==action.actor_id&&original.value.agreement.tick==action.start_tick&&action.remaining_ticks>0;
}
bool player_plan_tactic_lesson_meeting_matches(const GrowthTacticLessonActionSource& source,const InteractionSession& session){
    const auto& a=source.agreement;const auto& c=a.context;
    return !source.completion&&session.id==a.session_id&&session.actor_id==a.student_id&&session.dialogue_id==a.dialogue_id&&session.node_id==a.node_id&&session.phase=="say"&&session.pending_action_id.empty()&&!session.pending_command&&session.context.provider_actor_id==c.provider_actor_id&&session.context.provider_actor_id==a.teacher_id&&session.context.role_id==c.role_id&&session.context.place_id==c.place_id&&session.context.place_id==a.place_id&&session.context.kind==c.kind&&session.context.encounter_id==c.encounter_id&&session.context.input_error==c.input_error;
}
Outcome<std::optional<GrowthTacticLessonCompletion>> player_plan_tactic_lesson_completion(const CommonStore& common,const GrowthTacticLessonState& lessons,const StableId& actor,const StableId& action,const CommitResult& actual){
    const bool known=std::any_of(lessons.starts.rows().begin(),lessons.starts.rows().end(),[&](const auto& row){return row.second.action_id==action;});
    if(!known)return {true,std::nullopt,{}};
    auto original=read_growth_tactic_lesson_action_source(common,lessons,actor,action);if(!original.ok)return {false,{},original.error};
    auto bound=read_growth_tactic_lesson_completions_for_result(common,lessons,actual);if(!bound.ok)return {false,{},bound.error};
    std::optional<GrowthTacticLessonCompletion> exact;
    for(const auto& source:bound.value)if(source.action_id==action){
        if(exact||!original.value.completion||source.source_fact_id!=original.value.completion->source_fact_id||source.actor_id!=actor||source.tick!=actual.tick||common.action(action)||!actual.details)return {false,{},common_failure("PlayerPlanTacticLessonMismatch",action).error};
        exact=source;
    }
    return {true,std::move(exact),{}};
}
Outcome<std::optional<GrowthTacticLessonCompletion>> player_plan_tactic_lesson_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const CommitResult& actual){
    if(!w.growth_tactic_lessons_enabled())return {true,std::nullopt,{}};
    return player_plan_tactic_lesson_completion(w.common,w.growth_tactic_lessons,actor,action,actual);
}
}
