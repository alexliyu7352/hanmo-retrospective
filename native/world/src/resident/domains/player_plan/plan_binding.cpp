#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/contact_inquiries/contact_inquiry_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
// 参数绑定借原行动与开工冻结报价；后来的资格、价格或余额变化不篡改历史授权。
#include "plan_binding.hpp"
#include "plan_sources.hpp"
#include "resident/kernel/world_bundle.hpp"
#include "resident/domains/military/naval_journey_termination.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/domains/military/military_journey_termination.hpp"
namespace hanmo::resident {
namespace {
// 命名目标和坐标都逐值核对，不能用到达城镇替代原坐标目标。
bool same_destination(const TravelDestination& a,const TravelDestination& b){if(a.index()!=b.index())return false;if(const auto* id=std::get_if<StableId>(&a))return *id==std::get<StableId>(b);const auto& x=std::get<PointI>(a);const auto& y=std::get<PointI>(b);return x.x==y.x&&x.y==y.y;}
// 冷字段顺序不影响授权，上下文的存在位与原提供者必须一致。
bool same_context(const InteractionContext& a,const InteractionContext& b){return a.place_id==b.place_id&&a.provider_actor_id==b.provider_actor_id&&a.role_id==b.role_id&&a.kind==b.kind&&a.encounter_id==b.encounter_id&&a.input_error==b.input_error;}
// 比较的是领域保存的开工报价，而不是可能已经改变的当前目录报价。
bool same_terms(const WorldBundle& w,const PlayerPlanStep& step,const GrowthAction& payload,const StableId& actor){const auto* person=w.common.actor(actor);if(!person||!step.terms)return false;auto original=freeze_player_plan_terms(payload.quote,person->wallet_id);return original.ok&&same_player_plan_terms(*step.terms,original.value);}
// 已接行动继续的能力名单闭合，不能以历史action_id自动接入交易或任职。
Outcome<bool> supported_continuation(const WorldBundle& w,const ActionCoreView& action){if(action.kind=="tactic_lesson")return {true,player_plan_tactic_lesson_continuation(w,action),{}};if(action.kind=="service"&&w.personal_recovery_enabled()&&w.personal_recovery.services.find(action.id))return {true,player_plan_recovery_continuation(w,action),{}};if(action.kind=="deliver_message")return {true,player_plan_personal_message_continuation(w,action),{}};if(action.kind=="report_offer")return {true,player_plan_personal_report_continuation(w,action),{}};if(action.kind=="accept_offer")return {true,player_plan_personal_acceptance_continuation(w,action),{}};if(action.kind==task_message_return_collect_command)return {true,player_plan_message_return_continuation(w,action),{}};if(action.kind==visit_appointment_meeting_action_kind)return read_player_plan_visit_appointment_meeting_continuation(w,action);if(action.kind==contact_inquiry_action_kind)return read_player_plan_contact_inquiry_continuation(w,action);if(action.kind==personal_appointment_action_kind)return read_player_plan_appointment_continuation(w,action);if(action.kind=="task_shop_order_deliver")return {true,player_plan_shop_order_continuation(w,action),{}};if(action.kind=="family_child_care")return {true,player_plan_child_care_continuation(w,action),{}};if(action.kind=="family_child_care_relief")return {true,player_plan_child_care_relief_continuation(w,action),{}};if(action.kind=="naval_repair")return {true,player_plan_naval_repair_continuation(w,action),{}};if(action.kind=="travel"||action.kind=="study_skill"||action.kind=="long_rest")return {true,true,{}};const auto* payload=w.growth.actions.find(action.id);return {true,action.kind=="service"&&payload&&payload->request.kind=="service"&&payload->request.activity=="work",{}};}
}
// 同一行动只允许一个真实海军终止；原零时回执精确选source，半日回执从唯一永久来源重读。
Outcome<std::optional<StableId>> player_plan_naval_termination_reason(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.naval_combat_enabled())return {false,{},common_failure("NavalCombatProfileRequired").error};
    if(w.common.action(action)||tick<0||tick>w.common.tick())return {false,{},common_failure("PlayerPlanNavalTerminationMismatch",action).error};
    std::optional<StableId> reason;
    auto read=[&](const StableId& source)->Status{
        auto result=query_naval_journey_termination(w.common,w.military,w.naval,source,action,actor);
        if(!result.ok)return {false,{},result.error};if(!result.value)return common_success();
        const auto& value=*result.value;
        if(value.tick!=tick)return common_success();
        if(value.source_fact_id!=source||value.action_id!=action||value.actor_id!=actor||reason||(value.reason!="NavalCommanderKilled"&&value.reason!="NavalCommanderCaptured"&&value.reason!="NavalTravelUnserviceable"))return common_failure("PlayerPlanNavalTerminationMismatch",action);
        reason=value.reason;return common_success();
    };
    if(const auto* naval=std::get_if<NavalBattleCommandReceipt>(&receipt.data)){
        if((naval->kind!="naval_capture"&&naval->kind!="naval_battle_continue")||naval->actor_id!=actor||naval->tick!=tick)return {false,{},common_failure("PlayerPlanNavalTerminationMismatch",action).error};
        auto checked=read(naval->receipt_id);if(!checked.ok)return {false,{},checked.error};
    }else if(std::holds_alternative<AdvanceDetails>(receipt.data)){
        for(const auto& row:w.common.state().facts.rows())if(row.second.domain=="military_naval"&&(row.second.kind=="naval_round"||row.second.kind=="naval_capture"||row.second.kind=="naval_continue")){
            auto checked=read(row.first);if(!checked.ok)return {false,{},checked.error};
        }
    }else return {false,{},common_failure("PlayerPlanNavalTerminationMismatch",action).error};
    return {true,std::move(reason),{}};
}
// 显式Naval5才消费真实航损中断；不以今天船体或Action缺失推断失败来源。
Outcome<std::optional<StableId>> player_plan_naval_navigation_termination_reason(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.naval_navigation_enabled())return {true,std::nullopt,{}};
    return player_plan_naval_navigation_termination_reason(w.common,w.naval,*w.catalog->naval,actor,action,receipt,tick);
}
// 军事历史先经完整World校验；这里只绑定该Advance、该行动/人物/时刻的唯一原伤亡。
Outcome<std::optional<StableId>> player_plan_military_termination_reason(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    const auto* advance=std::get_if<AdvanceDetails>(&receipt.data);const auto* completion=advance?advance->completed_actions.find(action):nullptr;
    if(!completion||!completion->ok||completion->completed!=std::optional<bool>{true}||completion->success!=std::optional<bool>{false}||!completion->code||w.common.action(action)||tick<0||tick>w.common.tick())return {false,{},common_failure("PlayerPlanMilitaryTerminationMismatch",action).error};
    const auto& expected=*completion->code;if(expected!="MilitaryCommanderKilled"&&expected!="MilitaryCommanderCaptured"&&expected!="MilitaryCommanderDeserted")return {false,{},common_failure("PlayerPlanMilitaryTerminationMismatch",action).error};
    std::optional<StableId> reason;
    for(const auto& casualty:w.military.casualty_facts){
        const auto* round=w.common.state().facts.find(casualty.battle_id);
        if(!round||round->domain!="military"||round->kind!="military_battle")continue;
        auto source=query_military_journey_termination(w.common,w.military,*w.catalog->military,casualty.id,action,actor);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->tick!=tick)continue;
        const auto& value=*source.value;if(reason||value.source_fact_id!=casualty.id||value.action_id!=action||value.actor_id!=actor||value.reason!=expected)return {false,{},common_failure("PlayerPlanMilitaryTerminationMismatch",action).error};reason=value.reason;
    }
    return {true,std::move(reason),{}};
}
// 新修缮与原旅行/学习互不冒用；只有完整原开工来源支持有界继续。
bool player_plan_naval_repair_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(action.kind!="naval_repair"||!w.naval_repairs_enabled())return false;
    auto source=query_naval_repair_action(w.common,w.naval,*w.catalog->naval,action.id);
    if(!source.ok||!source.value)return false;const auto& value=*source.value;
    return value.action_id==action.id&&value.actor_id==action.actor_id&&value.start_tick==action.start_tick&&value.result_id.empty()&&action.remaining_ticks>0&&action.remaining_ticks<=value.duration;
}
// 全Naval3历史已验证；再核实际回执的有限字段与独立原结果的逐值一致。
Outcome<std::optional<NavalRepairReceipt>> player_plan_naval_repair_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.naval_repairs_enabled())return {true,std::nullopt,{}};
    return player_plan_naval_repair_completion(w.common,w.naval,*w.catalog->naval,actor,action,receipt,tick);
}

// 原照护接受与完整历史已核，持续行动仍须逐身份、时刻和唯一原开工匹配。
bool player_plan_child_care_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(!w.family_birth_enabled()||action.kind!="family_child_care")return false;
    const auto* start=w.family_birth.care_starts.find(action.id);if(!start||start->action_id!=action.id||start->caregiver_actor_id!=action.actor_id||start->tick!=action.start_tick||action.remaining_ticks<=0||action.remaining_ticks>start->duration)return false;
    auto terminal=read_child_care_completion(w.common,w.family_birth,action.actor_id,action.id);return terminal.ok&&!terminal.value;
}
// World完整照护史先验，有限纯读不递归调用World或重新消费预约。
Outcome<std::optional<ChildCareCompletionSource>> player_plan_child_care_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.family_birth_enabled())return {true,std::nullopt,{}};
    return player_plan_child_care_completion(w.common,w.family_birth,actor,action,receipt,tick);
}

// 新志愿合同先完成独立全史验证，再逐值绑定原计划。
bool player_plan_child_care_relief_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(!w.family_care_relief_enabled()||action.kind!="family_child_care_relief")return false;
    const auto* start=w.child_care_relief.starts.find(action.id);if(!start||start->action_id!=action.id||start->provider_actor_id!=action.actor_id||start->tick!=action.start_tick||action.remaining_ticks<=0||action.remaining_ticks>start->duration)return false;
    auto terminal=read_child_care_relief_completion(w.common,w.child_care_relief,action.actor_id,action.id);return terminal.ok&&!terminal.value;
}
// World完整照护史先验，有限纯读不递归调用World或重新消费预约。
Outcome<std::optional<ChildCareReliefCompletionSource>> player_plan_child_care_relief_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.family_care_relief_enabled())return {true,std::nullopt,{}};
    return player_plan_child_care_relief_completion(w.common,w.child_care_relief,actor,action,receipt,tick);
}

// 只借同一Task optional实体，完整Shop/Task历史在World组合门先验；不再报价或转货。
bool player_plan_shop_order_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(!w.task_shop_orders_enabled()||!w.tasks.shop_orders||action.kind!="task_shop_order_deliver")return false;
    auto read=query_task_shop_order_action(w.common,w.tasks,*w.tasks.shop_orders,action.actor_id,action.id);
    if(!read.ok||!read.value)return false;const auto& source=*read.value;
    return source.status=="active"&&source.active_contact&&!source.completed_tick&&source.actor_id==action.actor_id&&source.action_id==action.id&&source.started_tick==action.start_tick&&action.remaining_ticks==1;
}
// 只有这份Advance的真实TaskEffect可绑定终态；旧取消或其他Action回执不替代该来源。
Outcome<std::optional<TaskShopOrderActionSource>> player_plan_shop_order_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    if(!w.task_shop_orders_enabled())return {true,std::nullopt,{}};
    if(!w.tasks.shop_orders)return {false,{},common_failure("TaskShopOrderProfileRequired").error};
    return query_task_shop_order_completion(w.common,w.tasks,*w.tasks.shop_orders,actor,action,receipt,tick);
}

// 原服务永久pin保留实际Actor/报价/开工；当今休养资格不倒验既有义务。
bool player_plan_recovery_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(!w.personal_recovery_enabled()||action.kind!="service")return false;const auto* service=w.personal_recovery.services.find(action.id);const auto* original=w.growth.life_activities.find(action.id);
    if(!service||!original||service->actor_id!=action.actor_id||service->start_tick!=action.start_tick||original->view.actor_id!=action.actor_id||original->view.activity!="rest"||original->view.phase!=LifeActivityPhase::active||original->view.completed_tick||action.remaining_ticks<1||action.remaining_ticks>service->duration)return false;
    for(const auto& row:w.personal_recovery.closures.rows())if(row.second.action_id==action.id)return false;return true;
}
Outcome<std::optional<GrowthRecoveryCompletion>> player_plan_recovery_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const CommitResult& result){
    if(!w.personal_recovery_enabled()||!w.personal_recovery.services.find(action))return {true,std::nullopt,{}};
    auto sources=read_growth_recovery_completions_for_result(w.common,w.growth,w.personal_recovery,result);if(!sources.ok)return {false,{},sources.error};
    std::optional<GrowthRecoveryCompletion> exact;for(const auto& source:sources.value)if(source.action_id==action){
        if(exact||source.actor_id!=actor||source.tick!=result.tick||w.common.action(action)||!result.details)return {false,{},common_failure("PlayerPlanRecoveryMismatch",action).error};exact=source;
    }
    if(!exact)return {true,std::nullopt,{}};
    if(std::holds_alternative<AdvanceDetails>(result.details->data)||exact->reason=="cancelled")return player_plan_recovery_completion(w.common,w.growth,w.personal_recovery,actor,action,*result.details,result.tick);
    if(exact->success||(exact->reason!="patient_dead"&&exact->reason!="provider_dead"))return {false,{},common_failure("PlayerPlanRecoveryMismatch",action).error};
    return {true,std::move(exact),{}};
}
// 完整预约历史由同候选先验；原Action确实属于本人且只剩约定一半日。
bool player_plan_appointment_continuation(const WorldBundle& w,const ActionCoreView& action){
    if(!w.catalog->personal_appointments||!w.interaction.personal_appointments||action.kind!=personal_appointment_action_kind)return false;
    auto source=query_personal_appointment_action(w.common,*w.interaction.personal_appointments,action.actor_id,action.id);return source.ok&&source.value&&source.value->status=="active"&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks;
}
// 原World ActionTime与Life4读口先核真实源，失败不变成普通不支持。
Outcome<bool> read_player_plan_appointment_continuation(const WorldBundle&w,const ActionCoreView&action){
 if(!w.catalog->personal_appointments||action.kind!=personal_appointment_action_kind)return {true,false,{}};
 if(!w.interaction.personal_appointments)return {false,{},common_failure("PlayerPlanOriginalInteractionStateRequired",action.id,ErrorKind::internal).error};
 auto source=query_personal_appointment_action_world(w,action.actor_id,action.id);if(!source.ok)return {false,{},std::move(source.error)};
 return {true,source.value&&source.value->status=="active"&&source.value->actor_id==action.actor_id&&source.value->action_id==action.id&&source.value->start_tick==action.start_tick&&source.value->remaining_ticks==action.remaining_ticks,{}};
}
Outcome<std::optional<PersonalAppointmentActionSource>> player_plan_appointment_completion(const WorldBundle& w,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick,const StableId& result_id){
    if(!w.catalog->personal_appointments)return {true,std::nullopt,{}};if(!w.interaction.personal_appointments)return {false,{},common_failure("PersonalAppointmentProfileRequired").error};return query_personal_appointment_completion_world(w,actor,action,receipt,tick,result_id);
}
Status validate_player_plan_world(const WorldBundle& w){
    if(!w.player_plan.current)return common_success();
    const auto& plan=*w.player_plan.current;
    for(const auto& step:plan.steps){
        if(step.completed_tick&&step.action_id){
            const ReceiptDetails* receipt=nullptr;const CommitResult* actual_result=nullptr;
            for(const auto& row:w.common.state().results.rows())if(row.second&&step.completion_result_id==std::optional<StableId>{row.second->result_id}){if(actual_result)return common_failure("PlayerPlanDuplicateResult",step.id);actual_result=row.second.get();receipt=actual_result->details.get();}
            // 原教学开始记录先识别Action，失败码被删也不能回退旅行。
            if(player_plan_tactic_lesson_action(w,*step.action_id)||step.stop_reason.rfind("TacticLesson",0)==0){
                if(!actual_result||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data))return common_failure("PlayerPlanTacticLessonMismatch",step.id);
                auto original=read_growth_tactic_lesson_action_source(w.common,w.growth_tactic_lessons,plan.actor_id,*step.action_id);if(!original.ok)return {false,{},original.error};
                auto source=player_plan_tactic_lesson_completion(w,plan.actor_id,*step.action_id,*actual_result);if(!source.ok)return {false,{},source.error};
                if(!source.value||original.value.agreement.tick>step.source.observed_tick||source.value->tick!=*step.completed_tick||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success||step.stop_reason!=source.value->code)return common_failure("PlayerPlanTacticLessonMismatch",step.id);
                continue;
            }
            // 原私信永久开工先识别Action，实际结果和本人必须一致。
            if(player_plan_personal_message_action(w,*step.action_id)||step.stop_reason.rfind("TaskPersonalMessage",0)==0){
                if(!player_plan_personal_message_action(w,*step.action_id)||!actual_result||!receipt)return common_failure("PlayerPlanPersonalMessageMismatch",step.id);
                auto checked=validate_player_plan_personal_message_completion(w.common,w.tasks,*w.tasks.personal_messages,plan.actor_id,step,*actual_result);if(!checked.ok)return checked;
                continue;
            }if(player_plan_personal_report_action(w,*step.action_id)||step.stop_reason.rfind("TaskPersonalReport",0)==0){
                if(!player_plan_personal_report_action(w,*step.action_id)||!actual_result||!receipt)return common_failure("PlayerPlanPersonalReportMismatch",step.id);
                auto checked=validate_player_plan_personal_report_completion(w.common,w.tasks,*w.tasks.personal_reports,plan.actor_id,step,*actual_result);if(!checked.ok)return checked;
                continue;
            }if(player_plan_personal_acceptance_action(w,*step.action_id)||step.stop_reason.rfind("TaskPersonalAcceptance",0)==0){
                if(!player_plan_personal_acceptance_action(w,*step.action_id)||!actual_result||!receipt)return common_failure("PlayerPlanPersonalAcceptanceMismatch",step.id);
                auto checked=validate_player_plan_personal_acceptance_completion(w.common,w.tasks,*w.tasks.personal_acceptances,plan.actor_id,step,*actual_result);if(!checked.ok)return checked;
                continue;
            }
            // 回书原开工账先识别所有者；删失败码或篡改人物也不能落入泛旅行终态。
            const bool return_action=w.catalog->task_message_returns&&w.tasks.message_returns&&std::any_of(w.tasks.message_returns->starts.rows().begin(),w.tasks.message_returns->starts.rows().end(),[&](const auto& row){return row.second.action_id==*step.action_id;});
            if(return_action||step.stop_reason.rfind("TaskMessageReturn",0)==0){
                if(!return_action||!actual_result||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data))return common_failure("PlayerPlanMessageReturnMismatch",step.id);
                auto source=player_plan_message_return_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick,actual_result->result_id);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->actor_id!=plan.actor_id||source.value->started_tick>step.source.observed_tick||(step.phase==PlayerPlanStepPhase::completed)!=(source.value->status=="collected")||step.stop_reason!=(source.value->status=="collected"?StableId{}:source.value->reason))return common_failure("PlayerPlanMessageReturnMismatch",step.id);
                continue;
            }
            // 先找原Action所有者，即使抹掉失败reason或换人物也不能回退到普通旅行。
            const bool visit_meeting_action=w.catalog->visit_appointment_invitations&&w.interaction.visit_appointment_meetings&&std::any_of(w.interaction.visit_appointment_meetings->started.rows().begin(),w.interaction.visit_appointment_meetings->started.rows().end(),[&](const auto& row){return row.second.action_id==*step.action_id;});
            if(visit_meeting_action||step.stop_reason.rfind("VisitAppointmentMeeting",0)==0){
                if(!visit_meeting_action||!actual_result||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data))return common_failure("PlayerPlanVisitAppointmentMismatch",step.id);
                auto source=player_plan_visit_appointment_meeting_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick,actual_result->result_id);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->actor_id!=plan.actor_id||source.value->start_tick>step.source.observed_tick||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success||step.stop_reason!=(source.value->success?StableId{"VisitAppointmentMeetingFulfilled"}:source.value->reason))return common_failure("PlayerPlanVisitAppointmentMismatch",step.id);
                continue;
            }
            const bool inquiry_action=w.catalog->contact_inquiries&&w.interaction.contact_inquiries&&std::any_of(w.interaction.contact_inquiries->started.rows().begin(),w.interaction.contact_inquiries->started.rows().end(),[&](const auto& row){return row.second.action_id==*step.action_id;});
            if(inquiry_action||step.stop_reason.rfind("ContactInquiry",0)==0){
                if(!inquiry_action||!actual_result||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data))return common_failure("PlayerPlanContactInquiryMismatch",step.id);
                auto source=player_plan_contact_inquiry_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick,actual_result->result_id);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->actor_id!=plan.actor_id||source.value->start_tick>step.source.observed_tick||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success||step.stop_reason!=(source.value->success?StableId{"ContactInquiryAnswered"}:source.value->reason))return common_failure("PlayerPlanContactInquiryMismatch",step.id);
                continue;
            }
            std::optional<PersonalAppointmentActionSource> appointment;
            if(w.catalog->personal_appointments){if(!w.interaction.personal_appointments)return common_failure("PersonalAppointmentProfileRequired");auto original=query_personal_appointment_action_world(w,plan.actor_id,*step.action_id);if(!original.ok)return {false,{},original.error};appointment=std::move(original.value);}
            if(appointment||step.stop_reason.rfind("PersonalAppointment",0)==0){
                if(!appointment||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||appointment->actor_id!=plan.actor_id||appointment->start_tick>step.source.observed_tick)return common_failure("PlayerPlanAppointmentMismatch",step.id);
                auto source=player_plan_appointment_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick,actual_result->result_id);if(!source.ok)return {false,{},source.error};if(!source.value)return common_failure("PlayerPlanAppointmentMismatch",step.id);
                const auto code=source.value->success?StableId{"PersonalAppointmentFulfilled"}:source.value->reason;if((step.phase==PlayerPlanStepPhase::completed)!=source.value->success||step.stop_reason!=code)return common_failure("PlayerPlanAppointmentMismatch",step.id);
            }
            // 永久康复工单先于泛service判定，删失败码也不能退回旅行或普通工作成功。
            const auto* recovery=w.personal_recovery_enabled()?w.personal_recovery.services.find(*step.action_id):nullptr;
            if(recovery||step.stop_reason.rfind("Recovery",0)==0){
                if(!recovery||!actual_result||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||recovery->actor_id!=plan.actor_id||recovery->start_tick>step.source.observed_tick)return common_failure("PlayerPlanRecoveryMismatch",step.id);
                auto done=player_plan_recovery_completion(w,plan.actor_id,*step.action_id,*actual_result);if(!done.ok)return {false,{},done.error};
                if(!done.value||(step.phase==PlayerPlanStepPhase::completed)!=done.value->success||step.stop_reason!=done.value->code)return common_failure("PlayerPlanRecoveryMismatch",step.id);
                // 同次舟战命令可使提供者真实死亡；其业务回执不是这条service行动的旅行终止。
                continue;
            }
            // 先按真实工单识别，即使坏档删掉reason也不能退回普通Advance解释。
            std::optional<NavalRepairActionSource> repair;
            if(w.naval_repairs_enabled()){auto original=query_naval_repair_action(w.common,w.naval,*w.catalog->naval,*step.action_id);if(!original.ok)return {false,{},original.error};repair=std::move(original.value);}
            const bool repair_reason=step.stop_reason=="NavalRepairCompleted"||step.stop_reason=="NavalRepairCancelled"||step.stop_reason=="NavalRepairContact"||step.stop_reason=="NavalRepairCrewChanged"||step.stop_reason=="NavalRepairServiceChanged";
            if(repair||repair_reason||(receipt&&std::holds_alternative<NavalRepairReceipt>(receipt->data))){
                if(!repair||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||repair->actor_id!=plan.actor_id||repair->start_tick>step.source.observed_tick)return common_failure("PlayerPlanNavalRepairMismatch",step.id);
                auto source=player_plan_naval_repair_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->reason!=step.stop_reason||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success)return common_failure("PlayerPlanNavalRepairMismatch",step.id);
            }
            // 先按永久原Action识别店单，删reason也不能退回普通到达或泛Task成功。
            std::optional<TaskShopOrderActionSource> shop_order;
            if(w.task_shop_orders_enabled()){
                if(!w.tasks.shop_orders)return common_failure("TaskShopOrderProfileRequired");
                auto original=query_task_shop_order_action(w.common,w.tasks,*w.tasks.shop_orders,plan.actor_id,*step.action_id);if(!original.ok)return {false,{},original.error};shop_order=std::move(original.value);
            }
            if(shop_order||step.stop_reason.rfind("TaskShopOrder",0)==0){
                if(!shop_order||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||shop_order->actor_id!=plan.actor_id||shop_order->started_tick>step.source.observed_tick)return common_failure("PlayerPlanShopOrderMismatch",step.id);
                auto source=player_plan_shop_order_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);if(!source.ok)return {false,{},source.error};
                if(!source.value)return common_failure("PlayerPlanShopOrderMismatch",step.id);const bool success=source.value->status=="delivered";const auto code=success?StableId{}:source.value->reason;
                if((step.phase==PlayerPlanStepPhase::completed)!=success||step.stop_reason!=code)return common_failure("PlayerPlanShopOrderMismatch",step.id);
            }
            const auto* relief_care=w.family_care_relief_enabled()?w.child_care_relief.starts.find(*step.action_id):nullptr;
            const auto* care=w.family_birth_enabled()?w.family_birth.care_starts.find(*step.action_id):nullptr;
            const bool care_reason=step.stop_reason=="CaregiverStopped"||step.stop_reason=="CaregiverDied"||step.stop_reason=="ChildDied";
            const auto* family=receipt?std::get_if<FamilyEffect>(&receipt->data):nullptr;
            if(care||care_reason||(family&&!relief_care)){
                if(!care||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||care->caregiver_actor_id!=plan.actor_id||care->tick>step.source.observed_tick)return common_failure("PlayerPlanChildCareMismatch",step.id);
                auto source=player_plan_child_care_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->code!=step.stop_reason||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success)return common_failure("PlayerPlanChildCareMismatch",step.id);
            }
            const bool relief_reason=step.stop_reason=="CareReliefProviderStopped"||step.stop_reason=="CareReliefProviderDied"||step.stop_reason=="CareReliefChildDied";
            if(relief_care||relief_reason){
                if(!relief_care||!receipt||!std::holds_alternative<PlayerPlanContinue>(step.data)||relief_care->provider_actor_id!=plan.actor_id||relief_care->tick>step.source.observed_tick)return common_failure("PlayerPlanChildCareReliefMismatch",step.id);
                auto source=player_plan_child_care_relief_completion(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);if(!source.ok)return {false,{},source.error};
                if(!source.value||source.value->code!=step.stop_reason||(step.phase==PlayerPlanStepPhase::completed)!=source.value->success)return common_failure("PlayerPlanChildCareReliefMismatch",step.id);
            }
            // 原终止事实也参与识别，坏档删reason不能退回普通到达的成功解释。
            const bool navigation_reason=step.stop_reason=="NavalNavigationDisabled";
            if(receipt){
                auto source=player_plan_naval_navigation_termination_reason(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);
                if(!source.ok)return {false,{},source.error};
                if(source.value||navigation_reason){
                    if(!source.value||*source.value!=step.stop_reason||(!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked)return common_failure("PlayerPlanNavalNavigationMismatch",step.id);
                }
            }else if(navigation_reason)return common_failure("PlayerPlanNavalNavigationMismatch",step.id);
            const bool military_reason=step.stop_reason=="MilitaryCommanderKilled"||step.stop_reason=="MilitaryCommanderCaptured"||step.stop_reason=="MilitaryCommanderDeserted";
            if(military_reason){
                if(!receipt||(!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked)return common_failure("PlayerPlanMilitaryTerminationMismatch",step.id);
                auto source=player_plan_military_termination_reason(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);if(!source.ok)return {false,{},source.error};if(!source.value||*source.value!=step.stop_reason)return common_failure("PlayerPlanMilitaryTerminationMismatch",step.id);
            }
            const bool naval_reason=step.stop_reason=="NavalCommanderKilled"||step.stop_reason=="NavalCommanderCaptured"||step.stop_reason=="NavalTravelUnserviceable";
            if(receipt&&(naval_reason||std::holds_alternative<NavalBattleCommandReceipt>(receipt->data))){
                if((!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked)return common_failure("PlayerPlanNavalTerminationMismatch",step.id);
                auto source=player_plan_naval_termination_reason(w,plan.actor_id,*step.action_id,*receipt,*step.completed_tick);
                if(!source.ok)return {false,{},source.error};if(!source.value||*source.value!=step.stop_reason)return common_failure("PlayerPlanNavalTerminationMismatch",step.id);
            }
        }
        if(!step.started_tick||step.completed_tick)continue;
        const auto* action=step.action_id?w.common.action(*step.action_id):nullptr;
        const auto fail=[&](){return common_failure("PlayerPlanActionTargetMismatch",step.id);};
        if(!action||action->actor_id!=plan.actor_id)return fail();
        if(const auto* continued=std::get_if<PlayerPlanContinue>(&step.data)){if(continued->action_id!=action->id)return fail();auto supported=supported_continuation(w,*action);if(!supported.ok)return {false,{},std::move(supported.error)};if(!supported.value)return fail();if(action->kind=="service"&&w.personal_recovery_enabled()&&w.personal_recovery.services.find(action->id)){if(w.personal_recovery.services.find(action->id)->start_tick>step.source.observed_tick)return fail();}if(action->kind=="tactic_lesson"){auto source=read_growth_tactic_lesson_action_source(w.common,w.growth_tactic_lessons,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(source.value.agreement.tick>step.source.observed_tick)return fail();}if(action->kind=="deliver_message"){auto source=query_task_personal_message_action(w.common,w.tasks,*w.tasks.personal_messages,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->started_tick>step.source.observed_tick)return fail();}if(action->kind=="report_offer"){auto source=query_task_personal_report_action(w.common,w.tasks,*w.tasks.personal_reports,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->started_tick>step.source.observed_tick)return fail();}if(action->kind=="accept_offer"){auto source=query_task_personal_acceptance_action(w.common,w.tasks,*w.tasks.personal_acceptances,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->started_tick>step.source.observed_tick)return fail();}if(action->kind==task_message_return_collect_command){auto source=query_task_message_return_action(w.common,*w.tasks.message_returns,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->started_tick>step.source.observed_tick)return fail();}if(action->kind==visit_appointment_meeting_action_kind){auto source=query_visit_appointment_meeting_action_world(w,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->start_tick>step.source.observed_tick)return fail();}if(action->kind==contact_inquiry_action_kind){auto source=query_contact_inquiry_action_world(w,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->start_tick>step.source.observed_tick)return fail();}if(action->kind==personal_appointment_action_kind){auto source=query_personal_appointment_action_world(w,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->start_tick>step.source.observed_tick)return fail();}if(action->kind=="family_child_care"){const auto* start=w.family_birth.care_starts.find(action->id);if(!start||start->tick>step.source.observed_tick)return fail();}if(action->kind=="family_child_care_relief"){const auto* start=w.child_care_relief.starts.find(action->id);if(!start||start->tick>step.source.observed_tick)return fail();}if(action->kind=="task_shop_order_deliver"){if(!w.tasks.shop_orders)return fail();auto source=query_task_shop_order_action(w.common,w.tasks,*w.tasks.shop_orders,plan.actor_id,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->started_tick>step.source.observed_tick)return fail();}if(action->kind=="naval_repair"){auto source=query_naval_repair_action(w.common,w.naval,*w.catalog->naval,action->id);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->start_tick>step.source.observed_tick)return fail();}continue;}
        if(const auto* travel=std::get_if<PlayerPlanTravel>(&step.data)){
            if(action->kind!="travel")return fail();
            if(const auto* road=w.travel.road_actions.find(action->id)){if(!road->destination||!same_destination(*road->destination,travel->request.destination)||bool(road->via)!=travel->request.has_via||road->via.value_or(std::vector<StableId>{})!=travel->request.via)return fail();}
            else if(const auto* terrain=w.travel.actions.find(action->id)){const auto* journey=w.travel.journey_facts.find(terrain->plan_id);if(!travel->request.via.empty()||!same_destination(terrain->destination,travel->request.destination)||!journey||journey->action_id!=action->id||journey->actor_id!=plan.actor_id||!same_destination(journey->destination,travel->request.destination))return fail();}
            else return fail();
            continue;
        }
        const auto* payload=w.growth.actions.find(action->id);if(!payload)return fail();
        if(const auto* course=std::get_if<PlayerPlanCourse>(&step.data)){
            if(action->kind!="study_skill"||payload->request.kind!="study_skill"||payload->request.skill_id!=course->skill_id||payload->request.ticks!=course->ticks||payload->request.challenge_mode!=course->challenge_mode||!payload->request.context||!same_context(*payload->request.context,course->context)||!same_terms(w,step,*payload,plan.actor_id))return fail();
        }else if(const auto* work=std::get_if<PlayerPlanWork>(&step.data)){
            if(action->kind!="service"||payload->request.kind!="service"||payload->request.activity!="work"||!payload->request.context||!same_context(*payload->request.context,work->context)||!same_terms(w,step,*payload,plan.actor_id))return fail();
        }else if(const auto* rest=std::get_if<PlayerPlanRest>(&step.data)){
            const auto* terms=std::get_if<RestTerms>(&payload->quote.data);if(action->kind!="long_rest"||payload->request.kind!="long_rest"||payload->request.estate_id!=rest->estate_id||payload->request.duration!=rest->duration||!terms||terms->estate_id!=rest->estate_id||terms->duration!=rest->duration)return fail();
        }else return fail();
    }
    return common_success();
}
} // 原动作完成或合法人工改道后的终态只保旧真实回执，不再强求活动参数仍相同。
