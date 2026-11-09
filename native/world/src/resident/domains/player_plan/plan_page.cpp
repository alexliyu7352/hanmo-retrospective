#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
// 原生有限页面适配器只合成已知投影，不用未来人物副本获得远方报价。
#include "plan_page.hpp"
#include "resident/domains/player_plan/plan_views.hpp"
#include "resident/domains/player_plan/plan_sources.hpp"
#include "resident/domains/player_plan/plan_binding.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/continuation_snapshot.hpp"
namespace hanmo::resident {
namespace {
// 所有未知耗时显式保留原因，不能把被阻塞的原操作折算成零。
PlayerPlanEstimate unknown(const StableId& source,const StableId& reason){return {{},source,reason};}
// 当前行动的剩余时长来自唯一共同头；暂停和待输入没有确定的完成时刻。
PlayerPlanEstimate current_action_estimate(const WorldBundle& world,const StableId& actor_id,const StableId& action_id,const StableId& source,std::optional<RuleError>& fatal){
    const auto* actor=world.common.actor(actor_id);
    const auto* action=world.common.action(action_id);
    if(!actor||!action||action->actor_id!=actor_id||action->remaining_ticks<0)return unknown(source,"PlayerPlanActionReferenceMissing");
    // 店单只报已实际开工的那一个半日；原say会面可继续，别的手动决定仍未知。
    if(action->kind=="task_shop_order_deliver"){
        if(!player_plan_shop_order_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_task_shop_order_action(world.common,world.tasks,*world.tasks.shop_orders,actor_id,action_id);
        if(!original.ok||!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id)&&!task_shop_order_continuation_meeting_matches(*original.value,session))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind=="tactic_lesson"){
        if(!player_plan_tactic_lesson_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=read_growth_tactic_lesson_action_source(world.common,world.growth_tactic_lessons,actor_id,action_id);if(!original.ok)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!player_plan_tactic_lesson_meeting_matches(original.value,session))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind=="deliver_message"){
        if(!player_plan_personal_message_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_task_personal_message_action(world.common,world.tasks,*world.tasks.personal_messages,actor_id,action_id);if(!original.ok||!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        if(!player_plan_personal_message_meetings_match(*original.value,world.interaction.interactions))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind=="report_offer"){
        if(!player_plan_personal_report_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_task_personal_report_action(world.common,world.tasks,*world.tasks.personal_reports,actor_id,action_id);if(!original.ok||!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        if(!player_plan_personal_report_meetings_match(*original.value,world.interaction.interactions))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind=="accept_offer"){
        if(!player_plan_personal_acceptance_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_task_personal_acceptance_action(world.common,world.tasks,*world.tasks.personal_acceptances,actor_id,action_id);if(!original.ok||!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        if(!player_plan_personal_acceptance_meetings_match(*original.value,world.interaction.interactions))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind==task_message_return_collect_command){
        if(!player_plan_message_return_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_task_message_return_action(world.common,*world.tasks.message_returns,actor_id,action_id);if(!original.ok||!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id)&&!task_message_return_continuation_meeting_matches(*original.value,session))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind==visit_appointment_meeting_action_kind){
        auto supported=read_player_plan_visit_appointment_meeting_continuation(world,*action);if(!supported.ok){if(supported.error.kind!=ErrorKind::rule)fatal=supported.error;return unknown(source,supported.error.original.code);}if(!supported.value||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_visit_appointment_meeting_action_world(world,actor_id,action_id);if(!original.ok){if(original.error.kind!=ErrorKind::rule)fatal=original.error;return unknown(source,original.error.original.code);}if(!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id)&&!player_plan_visit_appointment_meeting_matches(*original.value,session))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind==contact_inquiry_action_kind){
        auto supported=read_player_plan_contact_inquiry_continuation(world,*action);if(!supported.ok){if(supported.error.kind!=ErrorKind::rule)fatal=supported.error;return unknown(source,supported.error.original.code);}if(!supported.value||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_contact_inquiry_action_world(world,actor_id,action_id);if(!original.ok){if(original.error.kind!=ErrorKind::rule)fatal=original.error;return unknown(source,original.error.original.code);}if(!original.value)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id)&&!player_plan_contact_inquiry_meeting_matches(*original.value,session))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(action->kind==personal_appointment_action_kind){
        auto supported=read_player_plan_appointment_continuation(world,*action);if(!supported.ok){if(supported.error.kind!=ErrorKind::rule)fatal=supported.error;return unknown(source,supported.error.original.code);}if(!supported.value||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        auto original=query_personal_appointment_action_world(world,actor_id,action_id);if(!original.ok){if(original.error.kind!=ErrorKind::rule)fatal=original.error;return unknown(source,original.error.original.code);}if(!original.value)return unknown(source,"PlayerPlanActionSourceRequired");const auto& c=original.value->contact;
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id){if(session.phase=="waiting"&&session.pending_action_id==action_id)continue;if(session.id==c.session_id&&session.actor_id==c.actor_id&&session.dialogue_id==c.dialogue_id&&session.node_id==c.node_id&&session.phase=="say"&&session.pending_action_id.empty()&&!session.pending_command&&session.context.provider_actor_id==c.provider_actor_id&&session.context.role_id==c.role_id&&session.context.place_id==c.place_id&&!session.context.kind&&!session.context.encounter_id&&!session.context.input_error)continue;return unknown(source,"PlayerPlanManualInputRequired");}
    }else if(action->kind=="service"&&world.personal_recovery_enabled()&&world.personal_recovery.services.find(action_id)){
        if(!player_plan_recovery_continuation(world,*action)||!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionSourceRequired");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id))return unknown(source,"PlayerPlanManualInputRequired");
    }else if(bounded_continuation_kind(action->kind)){
        ContinuationSnapshot context;context.actor_id=actor_id;context.action_id=action_id;context.action_kind=action->kind;context.action_start_tick=action->start_tick;context.wallet_id=actor->wallet_id;
        const auto stop=continuation_domain_stop(world,context);
        if(stop==RunStop::paused)return unknown(source,"PlayerPlanActionPaused");
        if(stop==RunStop::awaiting_input)return unknown(source,"PlayerPlanManualInputRequired");
        if(stop!=RunStop::awaiting_permit)return unknown(source,"PlayerPlanActionBlocked");
    }else{
        const auto* payload=world.growth.actions.find(action_id);
        if(action->kind!="service"||!payload||payload->request.activity!="work")return unknown(source,"PlayerPlanUnsupportedEstimate");
        if(!actor->alive||!actor->available||actor->action_id!=action_id)return unknown(source,"PlayerPlanActionBlocked");
        if(pending_encounter(world.interaction,actor_id)||world.services.in_open_battle(actor_id))return unknown(source,"PlayerPlanManualInputRequired");
        for(const auto& session:world.interaction.interactions)if(session.actor_id==actor_id&&!(session.phase=="waiting"&&session.pending_action_id==action_id))return unknown(source,"PlayerPlanManualInputRequired");
    }
    return {action->remaining_ticks,source,{}};
}
// 只能比较本人当前真位置，不能用前一步的目标替换实际场所进行后续报价。
bool already_at(const WorldBundle& world,const StableId& actor_id,const StableId& place_id){
    const auto position=world.common.resolved_location(actor_id);
    const auto* local=position.ok?std::get_if<SettlementLocation>(&position.value):nullptr;
    return local&&local->place_id==place_id;
}
// 来源适配器仅核既存来源身份；工作课程不重新查看远地提供者的忙闲或钱物。
PlayerPlanEstimate queued_estimate(const WorldBundle& world,const StableId& actor_id,const PlayerPlanStep& step,bool first,bool& origin_unknown,std::optional<RuleError>& fatal){
    if(step.phase==PlayerPlanStepPhase::blocked)return unknown("blocked_step",step.stop_reason.empty()?"PlayerPlanActionBlocked":step.stop_reason);
    const auto source=validate_player_plan_source(world,actor_id,step);
    if(!source.ok){if(source.error.kind!=ErrorKind::rule)fatal=source.error;return unknown("known_source",source.error.original.code);}
    if(const auto* move=std::get_if<PlayerPlanMove>(&step.data)){
        if(!already_at(world,actor_id,move->place_id))origin_unknown=true;
        return {0,"known_local_move",{}};
    }
    if(std::holds_alternative<PlayerPlanTravel>(step.data)){
        const bool can_quote=!origin_unknown;
        origin_unknown=true;
        if(!can_quote)return unknown("current_travel_preview","PlayerPlanProjectedOriginUnknown");
        const auto command=player_plan_domain_command(step.data);
        if(!command.ok){if(command.error.kind!=ErrorKind::rule)fatal=command.error;return unknown("current_travel_preview",command.error.original.code);}
        const auto prepared=prepare_command(world,actor_id,command.value);
        if(!prepared.ok){if(prepared.error.kind!=ErrorKind::rule)fatal=prepared.error;return unknown("current_travel_preview",prepared.error.original.code);}
        const auto* quote=prepared.value&&prepared.value->quote?std::get_if<TravelQuote>(&prepared.value->quote->data):nullptr;
        if(!quote||quote->duration<0)return unknown("current_travel_preview","PlayerPlanTravelPreviewUnavailable");
        return {quote->duration,"current_travel_preview",{}};
    }
    if(std::holds_alternative<PlayerPlanWork>(step.data)||std::holds_alternative<PlayerPlanCourse>(step.data)){
        const StableId captured=std::holds_alternative<PlayerPlanWork>(step.data)?"captured_work_quote":"captured_course_quote";
        if(!step.terms||step.terms->duration<1)return unknown(captured,"PlayerPlanCapturedQuoteMissing");
        return {step.terms->duration,captured,{}};
    }
    if(const auto* rest=std::get_if<PlayerPlanRest>(&step.data))return {rest->duration,"own_rest_duration",{}};
    if(const auto* continued=std::get_if<PlayerPlanContinue>(&step.data)){
        const auto* action=world.common.action(continued->action_id);
        if(action&&action->kind=="travel")origin_unknown=true;
        if(!first)return unknown("existing_action_remaining","PlayerPlanExistingActionTimingUnknown");
        return current_action_estimate(world,actor_id,continued->action_id,"existing_action_remaining",fatal);
    }
    if(std::holds_alternative<PlayerPlanTaskReminder>(step.data))return unknown("original_task_report","PlayerPlanTaskCompletionUnknown");
    return unknown("known_source","PlayerPlanUnsupportedEstimate");
}
}
// 页面始终使用当前玩家和所选情景开关，不让新能力改变旧profile的承诺读取。
Outcome<BoundaryValue> query_player_plan_page(const WorldBundle& world,const StableId& actor_id){
    if(actor_id!=world.common.state().metadata.player_actor_id||!world.common.actor(actor_id))return {false,{},common_failure("PlayerPlanActorRequired").error};
    if(!world.catalog->player_plans)return {true,boundary::obj({{"enabled",BoundaryValue{false}},{"plan_id",BoundaryValue{StableId{}}},{"steps",BoundaryValue{BoundaryValue::Array{}}},{"commitments",BoundaryValue{BoundaryValue::Array{}}},{"conflicts",BoundaryValue{BoundaryValue::Array{}}}}),{}};
    if(world.player_plan.current&&world.player_plan.current->actor_id!=actor_id)return {false,{},common_failure("PlayerPlanActorMismatch").error};
    const auto commitments=known_player_commitments(world.common,world.tasks,*world.catalog->tasks,*world.catalog->common,world.growth,actor_id);
    if(!commitments.ok)return {false,{},commitments.error};
    bool origin_unknown=false,first=true;std::optional<RuleError> fatal;
    const auto estimate=[&](const PlayerPlanStep& step){
        if(fatal)return unknown("known_source",fatal->original.code);
        const bool is_first=first;first=false;
        if(step.action_id&&!step.completed_tick){
            const auto* action=world.common.action(*step.action_id);
            if(action&&action->kind=="travel")origin_unknown=true;
            return current_action_estimate(world,actor_id,*step.action_id,"active_action_remaining",fatal);
        }
        return queued_estimate(world,actor_id,step,is_first,origin_unknown,fatal);
    };
    const auto projection=project_player_plan(world.player_plan,world.common.tick(),commitments.value,estimate);
    if(fatal)return {false,{},*fatal};
    if(!projection.ok)return {false,{},projection.error};
    return {true,encode_player_plan_view(world.player_plan,projection.value),{}};
}
} // 预计只表示当前已知条件下的时间，实际启动仍须原preview和明确执行许可。
