// 生活意向沿原公开来源生成固定命令，重新报价只发生在当场接受边界。
#include "plan_sources.hpp"
#include "plan_binding.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/kernel/continuation_snapshot.hpp"
#include <algorithm>
namespace hanmo::resident {
namespace {
// 普通来源失败保明确领域码，调用方不得把失败当作可自动接受的空报价。
template<class T> Outcome<T> fail(const StableId& code){return {false,{},common_failure(code).error};}
// 普通课程和短工没有叙事、路遇或畸形输入上下文。
bool ordinary_context(const InteractionContext& c){return !c.place_id.empty()&&!c.provider_actor_id.empty()&&!c.role_id.empty()&&!c.kind&&!c.encounter_id&&!c.input_error;}
// 字段顺序只是冷格式元信息，提供者、地点和上下文存在位才是授权身份。
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
// 接受委托可知的办理地点来自原合同，不查询远方人物当前位置。
bool task_place(const WorldBundle& w,const TaskRecord& task,const StableId& place){const auto* d=resolve_task_definition(w.tasks,*w.catalog->tasks,task);if(!d)return false;if(place==d->goal.destination_place_id&&!place.empty())return true;for(const auto& role:{d->report_role,d->goal.receiver_role}){const auto* r=w.catalog->common->roles.find(role);if(r&&r->place_id==place)return true;}return false;}
// 已关闭责任不能重新生成一个看似仍待履约的办理目标。
bool active_task(const TaskRecord& t){return t.status=="active"||t.status=="accepted"||t.status=="in_progress"||t.status=="ready"||t.status=="settling";}
// 命名地点和途经城镇已由原世界地图Canvas公开；坐标探点不属于首批授权来源。
Status public_travel(const WorldBundle& w,const PlayerPlanTravel& travel){const auto* id=std::get_if<StableId>(&travel.request.destination);if(!id||!travel.request.has_destination||!w.catalog->travel->settlements.find(*id)||travel.request.via.size()>player_plan_max_steps)return common_failure("PlayerPlanUnknownTravelSource");if(!w.catalog->travel->world_map_id.empty()&&!travel.request.via.empty())return common_failure("UnsupportedPlayerPlanTravelVia");for(const auto& via:travel.request.via)if(!w.catalog->travel->settlements.find(via))return common_failure("PlayerPlanUnknownTravelSource");return common_success();}
// 当前继续只复用已实现的有限行动，普通短工另核原request，不能接入交易。
Outcome<bool> supported_action(const WorldBundle& w,const ActionCoreView& action){if(action.kind=="tactic_lesson")return {true,player_plan_tactic_lesson_continuation(w,action),{}};if(action.kind=="service"&&w.personal_recovery_enabled()&&w.personal_recovery.services.find(action.id))return {true,player_plan_recovery_continuation(w,action),{}};if(action.kind=="deliver_message")return {true,player_plan_personal_message_continuation(w,action),{}};if(action.kind=="report_offer")return {true,player_plan_personal_report_continuation(w,action),{}};if(action.kind=="accept_offer")return {true,player_plan_personal_acceptance_continuation(w,action),{}};if(action.kind==task_message_return_collect_command)return {true,player_plan_message_return_continuation(w,action),{}};if(action.kind==visit_appointment_meeting_action_kind)return read_player_plan_visit_appointment_meeting_continuation(w,action);if(action.kind==contact_inquiry_action_kind)return read_player_plan_contact_inquiry_continuation(w,action);if(action.kind==personal_appointment_action_kind)return read_player_plan_appointment_continuation(w,action);if(action.kind=="task_shop_order_deliver")return {true,player_plan_shop_order_continuation(w,action),{}};if(action.kind=="family_child_care")return {true,player_plan_child_care_continuation(w,action),{}};if(action.kind=="family_child_care_relief")return {true,player_plan_child_care_relief_continuation(w,action),{}};if(action.kind=="naval_repair")return {true,player_plan_naval_repair_continuation(w,action),{}};if(bounded_continuation_kind(action.kind))return {true,true,{}};const auto* p=w.growth.actions.find(action.id);return {true,action.kind=="service"&&p&&p->request.kind=="service"&&p->request.activity=="work",{}};}
}
Outcome<PlayerPlanTerms> freeze_player_plan_terms(const GrowthQuote& q,const StableId& wallet){
    if(wallet.empty()||!q.context||!ordinary_context(*q.context)||!q.command.context||!same_context(*q.context,*q.command.context))return fail<PlayerPlanTerms>("PlayerPlanInvalidSourceContext");
    PlayerPlanTerms t;t.provider_actor_id=q.context->provider_actor_id;t.role_id=q.context->role_id;t.place_id=q.context->place_id;t.payer_wallet_id=wallet;t.duration=q.duration;t.cost=q.cost;t.wage=q.wage;
    if(q.command.kind=="service"&&q.command.activity=="work"){
        const auto* source=std::get_if<ServiceTerms>(&q.data);if(!source||source->duration!=q.duration||source->cost!=q.cost||source->wage!=q.wage||source->wallet_id.empty())return fail<PlayerPlanTerms>("PlayerPlanInvalidWorkTerms");
        t.recipient_wallet_id=source->wallet_id;t.hp_gain=source->hp_gain;t.fatigue_delta=source->fatigue_delta;t.mode="work";t.qualification_key="ordinary_work_v1";
    }else if(q.command.kind=="study_skill"){
        const auto* source=std::get_if<StudyTerms>(&q.data);if(!source||source->duration!=q.duration||source->duration!=q.command.ticks||source->fee!=q.cost||source->skill_id!=q.command.skill_id||source->teacher_id!=t.provider_actor_id||source->payer_wallet_id!=wallet||!same_context(source->context,*q.context)||source->recipient_wallet_id.empty())return fail<PlayerPlanTerms>("PlayerPlanInvalidCourseTerms");
        t.recipient_wallet_id=source->recipient_wallet_id;t.mode=source->mode;t.skill_id=source->skill_id;t.xp_limit=source->xp_limit;t.target_level=q.target_level;t.settlement_policy=source->settlement_policy;t.qualification_key="study_skill_v1";t.challenge_mode=q.command.challenge_mode;
        if(source->challenge){const auto& challenge=*source->challenge;if(challenge.mode!=q.command.challenge_mode)return fail<PlayerPlanTerms>("PlayerPlanInvalidCourseTerms");t.challenge_kind=challenge.kind;t.challenge_id=challenge.challenge_id;t.challenge_source_sha256=challenge.source_sha256;
            // 题目预算/数量是作者实质条款；当前能力快照不参与键，交易报价绑定不进入首批课程。
            t.qualification_key+="/"+std::to_string(challenge.field.size())+":"+challenge.field;
            if(challenge.context){if(challenge.context->quote)return fail<PlayerPlanTerms>("PlayerPlanTradeQuoteNotSupported");t.qualification_key+="/context:"+std::to_string(challenge.context->budget)+":"+std::to_string(challenge.context->quantity)+":"+std::to_string(challenge.context->relation_discount_bp);}else t.qualification_key+="/no_context";
        }
    }else return fail<PlayerPlanTerms>("PlayerPlanUnsupportedSource");
    if(t.duration<1||t.cost<0||t.wage<0)return fail<PlayerPlanTerms>("PlayerPlanInvalidTerms");
    return {true,std::move(t),{}};
}
Status match_player_plan_terms(const PlayerPlanTerms& authorized,const GrowthQuote& current,const StableId& wallet){auto fresh=freeze_player_plan_terms(current,wallet);if(!fresh.ok)return {false,{},fresh.error};return same_player_plan_terms(authorized,fresh.value)?common_success():common_failure("PlayerPlanTermsChanged");}
Outcome<CommandPayload> player_plan_domain_command(const PlayerPlanStepData& data){
    if(const auto* p=std::get_if<PlayerPlanMove>(&data))return {true,{"move_local",TravelCommandPayload{MoveLocalCommand{p->place_id}}},{}};
    if(const auto* p=std::get_if<PlayerPlanTravel>(&data))return {true,{"travel",TravelCommandPayload{p->request}},{}};
    GrowthCommand c;
    if(const auto* p=std::get_if<PlayerPlanWork>(&data)){c.kind="service";c.activity="work";c.context=p->context;}
    else if(const auto* p=std::get_if<PlayerPlanCourse>(&data)){c.kind="study_skill";c.context=p->context;c.skill_id=p->skill_id;c.ticks=p->ticks;c.challenge_mode=p->challenge_mode;}
    else if(const auto* p=std::get_if<PlayerPlanRest>(&data)){c.kind="long_rest";c.estate_id=p->estate_id;c.duration=p->duration;}
    else return fail<CommandPayload>("PlayerPlanNoStartCommand");
    return {true,{c.kind,std::move(c)},{}};
}
Outcome<PlayerPlanAppend> prepare_player_plan_source(const WorldBundle& w,const StableId& id,const PlayerPlanStepData& data,bool authorized){
    if(id!=w.common.state().metadata.player_actor_id||!w.common.actor(id))return fail<PlayerPlanAppend>("PlayerPlanPlayerRequired");
    PlayerPlanAppend result;result.data=data;result.authorized=authorized;result.source.observed_tick=w.common.tick();const auto* actor=w.common.actor(id);
    auto location=w.common.resolved_location(id);if(!location.ok)return {false,{},location.error};const auto* at=std::get_if<SettlementLocation>(&location.value);
    if(const auto* p=std::get_if<PlayerPlanMove>(&data)){
        const auto* place=at?w.catalog->common->places.find(at->place_id):nullptr;if(!place||(p->place_id!=place->id&&std::find(place->passages.begin(),place->passages.end(),p->place_id)==place->passages.end()))return fail<PlayerPlanAppend>("PlayerPlanUnknownLocalSource");result.source={PlayerPlanSourceKind::local_observation,place->id,w.common.tick()};
    }else if(const auto* p=std::get_if<PlayerPlanTravel>(&data)){auto valid=public_travel(w,*p);if(!valid.ok)return {false,{},valid.error};result.source={PlayerPlanSourceKind::authored,"world_map:"+w.common.state().metadata.scenario_id,w.common.tick()};}
    else if(std::holds_alternative<PlayerPlanWork>(data)||std::holds_alternative<PlayerPlanCourse>(data)){
        auto command=player_plan_domain_command(data);if(!command.ok)return {false,{},command.error};auto quote=prepare_command(w,id,command.value);if(!quote.ok)return {false,{},quote.error};const auto* growth=quote.value&&quote.value->quote?std::get_if<GrowthQuote>(&quote.value->quote->data):nullptr;if(!growth)return fail<PlayerPlanAppend>("PlayerPlanMissingSourceQuote");auto terms=freeze_player_plan_terms(*growth,actor->wallet_id);if(!terms.ok)return {false,{},terms.error};result.source={PlayerPlanSourceKind::local_observation,terms.value.role_id,w.common.tick()};result.terms=std::move(terms.value);
    }else if(const auto* p=std::get_if<PlayerPlanRest>(&data)){
        const auto* estate=w.estate.estates.find(p->estate_id);if(!estate||estate->owner_actor_id!=id||!w.catalog->estate->estates.find(p->estate_id)||p->duration<1||p->duration>720)return fail<PlayerPlanAppend>("PlayerPlanRestSourceRequired");result.source={PlayerPlanSourceKind::own_estate,p->estate_id,w.common.tick()};
    }else if(const auto* p=std::get_if<PlayerPlanContinue>(&data)){
        const auto* action=w.common.action(p->action_id);if(!action||action->actor_id!=id||actor->action_id!=action->id)return fail<PlayerPlanAppend>("PlayerPlanActionSourceRequired");auto supported=supported_action(w,*action);if(!supported.ok)return {false,{},std::move(supported.error)};if(!supported.value)return fail<PlayerPlanAppend>("PlayerPlanActionSourceRequired");result.source={PlayerPlanSourceKind::existing_action,p->action_id,w.common.tick()};
    }else if(const auto* p=std::get_if<PlayerPlanTaskReminder>(&data)){
        const auto* task=w.tasks.tasks.find(p->task_id);if(!task||task->assignee!=id||!active_task(*task)||!task_place(w,*task,p->place_id))return fail<PlayerPlanAppend>("PlayerPlanTaskSourceRequired");result.source={PlayerPlanSourceKind::accepted_task,p->task_id,w.common.tick()};
    }else return fail<PlayerPlanAppend>("PlayerPlanUnsupportedSource");
    // 只有课程和短工具有“到场后接受”授权，其他固定步骤没有隐含合同接受位。
    if(authorized&&!result.terms)return fail<PlayerPlanAppend>("PlayerPlanAuthorizationNotApplicable");
    return {true,std::move(result),{}};
}
Status validate_player_plan_source(const WorldBundle& w,const StableId& id,const PlayerPlanStep& step){
    if(id!=w.common.state().metadata.player_actor_id||step.source.observed_tick<0||step.source.observed_tick>w.common.tick())return common_failure("PlayerPlanSourceMismatch");
    if(const auto* p=std::get_if<PlayerPlanMove>(&step.data)){const auto* source=w.catalog->common->places.find(step.source.source_id);if(step.source.kind!=PlayerPlanSourceKind::local_observation||!source||(source->id!=p->place_id&&std::find(source->passages.begin(),source->passages.end(),p->place_id)==source->passages.end()))return common_failure("PlayerPlanSourceMismatch");return common_success();}
    if(const auto* p=std::get_if<PlayerPlanTravel>(&step.data)){if(step.source.kind!=PlayerPlanSourceKind::authored||step.source.source_id!="world_map:"+w.common.state().metadata.scenario_id)return common_failure("PlayerPlanSourceMismatch");return public_travel(w,*p);}
    const InteractionContext* context=nullptr;if(const auto* p=std::get_if<PlayerPlanWork>(&step.data))context=&p->context;if(const auto* p=std::get_if<PlayerPlanCourse>(&step.data))context=&p->context;
    if(context){if(!ordinary_context(*context)||step.source.kind!=PlayerPlanSourceKind::local_observation||step.source.source_id!=context->role_id||!step.terms||step.terms->provider_actor_id!=context->provider_actor_id||step.terms->role_id!=context->role_id||step.terms->place_id!=context->place_id)return common_failure("PlayerPlanSourceMismatch");return common_success();}
    if(const auto* p=std::get_if<PlayerPlanRest>(&step.data)){const auto* estate=w.estate.estates.find(p->estate_id);return step.source.kind==PlayerPlanSourceKind::own_estate&&step.source.source_id==p->estate_id&&estate&&estate->owner_actor_id==id?common_success():common_failure("PlayerPlanRestSourceRequired");}
    if(const auto* p=std::get_if<PlayerPlanContinue>(&step.data)){const auto* action=w.common.action(p->action_id);if(step.source.kind!=PlayerPlanSourceKind::existing_action||step.source.source_id!=p->action_id||!action||action->actor_id!=id)return common_failure("PlayerPlanActionSourceRequired");auto supported=supported_action(w,*action);if(!supported.ok)return {false,{},std::move(supported.error)};return supported.value?common_success():common_failure("PlayerPlanActionSourceRequired");}
    if(const auto* p=std::get_if<PlayerPlanTaskReminder>(&step.data)){const auto* task=w.tasks.tasks.find(p->task_id);return step.source.kind==PlayerPlanSourceKind::accepted_task&&step.source.source_id==p->task_id&&task&&task->assignee==id&&active_task(*task)&&task_place(w,*task,p->place_id)?common_success():common_failure("PlayerPlanTaskSourceRequired");}
    return common_failure("PlayerPlanUnsupportedSource");
}
} // 公共地图只有命名目的地，远地可知来源不会携带实时人物、工资余额或库存。
