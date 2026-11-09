#include "resident/domains/military/naval_navigation_history.hpp"
// 计划恢复先核形状和原回执；缺失行动不能当作完成，更不能自动改绑其他目标。
#include "plan_model.hpp"
#include "plan_time2.hpp"
#include "plan_binding.hpp"
#include "resident/domains/family/family_care_completion_shape.hpp"
#include "resident/domains/military/naval_repair_history.hpp"
#include "resident/kernel/command_types.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 所有数字使用闭合整数范围，阻止冷输入溢出预算、数组或预测时间。
bool money(Integer value){return value>=0&&value<=2147483647;}
// 普通接触不允许对白或路遇上下文替代原提供者。
bool local_context(const InteractionContext& value){return !value.place_id.empty()&&!value.provider_actor_id.empty()&&!value.role_id.empty()&&!value.kind&&!value.encounter_id&&!value.input_error;}
// 实际类型决定原行动kind，不由调用者提交自由字符串。
StableId action_kind(const PlayerPlanStepData& data){switch(data.index()){case 1:return "travel";case 2:return "service";case 3:return "study_skill";case 4:return "long_rest";default:return {};}}
// 原服务和课程报价只保存有限稳定条款；每次启动仍由原preview核验当前资格。
Status terms_shape(const PlayerPlanStep& step){if(!step.terms)return step.authorized?common_failure("PlayerPlanAuthorizationRequired",step.id):common_success();const auto& t=*step.terms;if(step.data.index()!=2&&step.data.index()!=3)return common_failure("UnexpectedPlayerPlanTerms",step.id);if(t.provider_actor_id.empty()||t.role_id.empty()||t.place_id.empty()||t.payer_wallet_id.empty()||t.recipient_wallet_id.empty()||t.qualification_key.empty()||t.duration<1||t.duration>2147483647||!money(t.cost)||!money(t.wage)||t.hp_gain<0||t.hp_gain>100||t.fatigue_delta< -100||t.fatigue_delta>100||t.xp_limit<0||t.xp_limit>2147483647||t.target_level<0||t.target_level>4||(t.settlement_policy&&t.settlement_policy->empty())||(t.challenge_source_sha256&&t.challenge_source_sha256->empty()))return common_failure("InvalidPlayerPlanTerms",step.id);const auto& context=step.data.index()==2?std::get<PlayerPlanWork>(step.data).context:std::get<PlayerPlanCourse>(step.data).context;if(t.provider_actor_id!=context.provider_actor_id||t.role_id!=context.role_id||t.place_id!=context.place_id)return common_failure("PlayerPlanTermsSourceMismatch",step.id);if(const auto* course=std::get_if<PlayerPlanCourse>(&step.data))if(t.skill_id!=course->skill_id||t.duration!=course->ticks||t.challenge_mode!=course->challenge_mode)return common_failure("PlayerPlanTermsParameterMismatch",step.id);return common_success();}
// 持久结果不存在或时刻不同即拒绝，不能从人物当前空闲推断原步骤成功。
const CommitResult* result_at(const CommonStore& common,const std::optional<StableId>& id,const std::optional<Integer>& tick,bool require_player=false){if(!id||!tick)return nullptr;const CommitResult* found=nullptr;for(const auto& row:common.state().results.rows()){const auto& value=row.second;if(!value||value->result_id!=*id)continue;if(require_player&&row.first.rfind(common.state().metadata.player_actor_id+":",0)!=0)return nullptr;if(found||value->tick!=*tick||!value->ok)return nullptr;found=value.get();}return found;}
}
// 调用方先核完整Naval3历史；本口只将真实结果与计划收到的同一回执逐值绑定。
Outcome<std::optional<NavalRepairReceipt>> player_plan_naval_repair_completion(const CommonStore& common,const NavalState& naval,const NavalCatalog& catalog,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    auto original=query_naval_repair_action(common,naval,catalog,action);if(!original.ok)return {false,{},original.error};if(!original.value)return {true,std::nullopt,{}};
    const auto mismatch=[&](){return Outcome<std::optional<NavalRepairReceipt>>{false,{},common_failure("PlayerPlanNavalRepairMismatch",action).error};};
    if(original.value->actor_id!=actor||original.value->result_id.empty()||common.action(action)||tick<0||tick>common.tick())return mismatch();
    auto source=query_naval_repair_completion(common,naval,catalog,original.value->result_id,action,actor);if(!source.ok)return {false,{},source.error};const auto& done=source.value;
    const bool reason=done.reason=="NavalRepairCompleted"||done.reason=="NavalRepairCancelled"||done.reason=="NavalRepairContact"||done.reason=="NavalRepairCrewChanged"||done.reason=="NavalRepairServiceChanged";
    if((done.reason=="NavalRepairServiceChanged"&&!catalog.repair_charge_policy)||!reason||!done.completed||done.tick!=tick||done.success!=(done.reason=="NavalRepairCompleted"))return mismatch();
    if(const auto* advance=std::get_if<AdvanceDetails>(&receipt.data)){
        const auto* completion=advance->completed_actions.find(action);
        if(!completion||!completion->ok||completion->completed!=std::optional<bool>{true}||completion->success!=std::optional<bool>{done.success}||completion->code!=std::optional<StableId>{done.reason}||completion->growth||completion->task||completion->family||completion->military||completion->already_completed||!completion->military_field_order.empty())return mismatch();
    }else if(const auto* direct=std::get_if<NavalRepairReceipt>(&receipt.data)){
        if(!direct->completed||direct->replayed||direct->success||direct->reason!="NavalRepairCancelled"||direct->source_id!=done.source_id||direct->action_id!=action||direct->actor_id!=actor||direct->tick!=tick||direct->reason!=done.reason||direct->success!=done.success)return mismatch();
    }else return mismatch();
    return {true,done,{}};
}
// 原照护事实确定成败；命令成功只说明本人成功取消，不能伪作已完成喂养。
Outcome<std::optional<ChildCareCompletionSource>> player_plan_child_care_completion(const CommonStore& common,const FamilyBirthState& birth,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    const auto mismatch=[&](){return Outcome<std::optional<ChildCareCompletionSource>>{false,{},common_failure("PlayerPlanChildCareMismatch",action).error};};
    const auto* start=birth.care_starts.find(action);if(!start)return {true,std::nullopt,{}};
    if(start->caregiver_actor_id!=actor||common.action(action)||tick<0||tick>common.tick())return mismatch();
    auto source=read_child_care_completion(common,birth,actor,action);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->completed_tick!=tick)return mismatch();const auto& done=*source.value;
    const auto code=done.code.empty()?std::optional<StableId>{}:std::optional<StableId>{done.code};
    const FamilyEffect* effect=nullptr;bool expected_success=done.success;
    if(const auto* advance=std::get_if<AdvanceDetails>(&receipt.data)){
        const auto* completed=advance->completed_actions.find(action);
        if(!completed)return mismatch();auto shape=validate_family_care_completion_shape(*completed,actor,actor,done.fact_id,done.success,done.code);if(!shape.ok)return mismatch();effect=&*completed->family;
    }else if(const auto* direct=std::get_if<FamilyEffect>(&receipt.data)){
        if(done.success||done.code!="CaregiverStopped"||direct->code)return mismatch();effect=direct;expected_success=true;
    }else return mismatch();
    if(effect->success!=std::optional<bool>{expected_success}||effect->actor_id!=std::optional<StableId>{actor}||effect->player_actor_id!=std::optional<StableId>{actor}||effect->fact_id!=std::optional<StableId>{done.fact_id}||effect->already_completed||effect->heir_actor_id)return mismatch();
    if(std::holds_alternative<AdvanceDetails>(receipt.data)&&effect->code!=code)return mismatch();
    return {true,done,{}};
}
// 新照护与原出生Action来源各自唯一；零时命令成功仍是照护失败。
Outcome<std::optional<ChildCareReliefCompletionSource>> player_plan_child_care_relief_completion(const CommonStore& common,const ChildCareReliefState& relief,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    const auto mismatch=[&](){return Outcome<std::optional<ChildCareReliefCompletionSource>>{false,{},common_failure("PlayerPlanChildCareReliefMismatch",action).error};};
    const auto* start=relief.starts.find(action);if(!start)return {true,std::nullopt,{}};
    if(start->provider_actor_id!=actor||common.action(action)||tick<0||tick>common.tick())return mismatch();
    auto source=read_child_care_relief_completion(common,relief,actor,action);if(!source.ok)return {false,{},source.error};if(!source.value||source.value->completed_tick!=tick)return mismatch();const auto& done=*source.value;
    const auto code=done.code.empty()?std::optional<StableId>{}:std::optional<StableId>{done.code};
    const FamilyEffect* effect=nullptr;bool expected_success=done.success;
    if(const auto* advance=std::get_if<AdvanceDetails>(&receipt.data)){
        const auto* completed=advance->completed_actions.find(action);
        if(!completed)return mismatch();auto shape=validate_family_care_completion_shape(*completed,actor,actor,done.fact_id,done.success,done.code);if(!shape.ok)return mismatch();effect=&*completed->family;
    }else if(const auto* direct=std::get_if<FamilyEffect>(&receipt.data)){
        if(done.success||done.code!="CareReliefProviderStopped"||direct->code)return mismatch();effect=direct;expected_success=true;
    }else return mismatch();
    if(effect->success!=std::optional<bool>{expected_success}||effect->actor_id!=std::optional<StableId>{actor}||effect->player_actor_id!=std::optional<StableId>{actor}||effect->fact_id!=std::optional<StableId>{done.fact_id}||effect->already_completed||effect->heir_actor_id)return mismatch();
    if(std::holds_alternative<AdvanceDetails>(receipt.data)&&effect->code!=code)return mismatch();
    return {true,done,{}};
}
// 精确原Action只有一份永久closure；完整身体/服务/恢复账先验后核实际有限回执。
Outcome<std::optional<GrowthRecoveryCompletion>> player_plan_recovery_completion(const CommonStore& common,const GrowthState& growth,const GrowthRecoveryState& recovery,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    const auto* service=recovery.services.find(action);if(!service)return {true,std::nullopt,{}};
    const auto mismatch=[&](){return Outcome<std::optional<GrowthRecoveryCompletion>>{false,{},common_failure("PlayerPlanRecoveryMismatch",action).error};};
    if(service->actor_id!=actor||common.action(action)||tick<0||tick>common.tick())return mismatch();StableId closure;
    for(const auto& row:recovery.closures.rows())if(row.second.action_id==action){if(!closure.empty())return mismatch();closure=row.first;}if(closure.empty())return mismatch();
    auto source=read_growth_recovery_completion(common,growth,recovery,closure);if(!source.ok)return {false,{},source.error};const auto& done=source.value;
    if(done.action_id!=action||done.actor_id!=actor||done.tick!=tick||done.success!=(done.reason=="completed")||(done.reason!="completed"&&done.reason!="cancelled"&&done.reason!="patient_dead"&&done.reason!="provider_dead"))return mismatch();
    if(const auto* advance=std::get_if<AdvanceDetails>(&receipt.data)){
        const auto* completed=advance->completed_actions.find(action);if(!completed)return mismatch();
        auto matched=validate_growth_recovery_action_completion(done,*completed);if(!matched.ok)return {false,{},matched.error};
    }else if(const auto* effect=std::get_if<GrowthEffect>(&receipt.data)){
        if(done.reason!="cancelled"||done.success||done.code!="RecoveryCancelled"||effect->kind!="recovery_cancel"||!effect->success||!effect->code.empty()||effect->elapsed!=done.provided_half_days||effect->action_id!=action)return mismatch();
        if(!effect->skill_id.empty()||!effect->quote_id.empty()||effect->already_completed||effect->reopened||effect->paused||effect->mastery||effect->xp_gained||effect->fee_paid||effect->unprovided_released||effect->completed_lessons||effect->paused_ticks||effect->reserved||effect->challenge||effect->public_challenge)return mismatch();
        const std::set<StableId> fields(effect->field_order.begin(),effect->field_order.end());if(!effect->field_order.empty()&&(fields.size()!=effect->field_order.size()||fields!=std::set<StableId>{"action_id","elapsed"}))return mismatch();
    }else return mismatch();
    return {true,done,{}};
}
// 不依赖世界的检查先拒绝非法参数、预算和不可重放边界。
Status validate_player_plan_shape(const PlayerPlanState& state){
    auto time_budget=validate_player_plan_time_budget(state);if(!time_budget.ok)return time_budget;
    if(!state.current)return common_success();
    const auto& plan=*state.current;
    if(plan.id.empty()||plan.actor_id.empty()||plan.created_tick<0||plan.steps.empty()||plan.steps.size()>player_plan_max_steps)return common_failure("InvalidPlayerPlan");
    if(plan.budget){const auto& b=*plan.budget;if(b.max_additional_ticks<1||b.max_gross_spend<0||b.used_ticks<0||b.gross_spend<0||b.used_ticks>b.max_additional_ticks||b.gross_spend>b.max_gross_spend||b.approved_tick<plan.created_tick)return common_failure("InvalidPlayerPlanBudget");}
    std::set<StableId> ids,actions,boundary_results;bool unfinished=false;Integer active=0;
    for(const auto& step:plan.steps){
        if(step.id.empty()||!ids.insert(step.id).second||step.source.source_id.empty()||step.source.observed_tick<0||static_cast<Integer>(step.source.kind)<0||static_cast<Integer>(step.source.kind)>4||static_cast<Integer>(step.phase)<0||static_cast<Integer>(step.phase)>4)return common_failure("InvalidPlayerPlanStep",step.id);
        const bool valid=std::visit([](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,PlayerPlanMove>)return !value.place_id.empty();else if constexpr(std::is_same_v<T,PlayerPlanTravel>){if(!value.request.has_destination||value.request.via.size()>player_plan_max_steps||(!value.request.has_via&&!value.request.via.empty()))return false;const auto* target=std::get_if<StableId>(&value.request.destination);if(!target||target->empty())return false;return std::all_of(value.request.via.begin(),value.request.via.end(),[](const auto& id){return !id.empty();});}else if constexpr(std::is_same_v<T,PlayerPlanWork>)return local_context(value.context);else if constexpr(std::is_same_v<T,PlayerPlanCourse>)return local_context(value.context)&&!value.skill_id.empty()&&value.ticks>=2&&value.ticks<=6&&(value.challenge_mode=="automatic"||value.challenge_mode=="manual");else if constexpr(std::is_same_v<T,PlayerPlanRest>)return !value.estate_id.empty()&&value.duration>=1&&value.duration<=720;else if constexpr(std::is_same_v<T,PlayerPlanContinue>)return !value.action_id.empty();else return !value.task_id.empty()&&!value.place_id.empty();},step.data);
        if(!valid)return common_failure("InvalidPlayerPlanParameters",step.id);
        const bool source_matches=std::visit([&](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,PlayerPlanMove>)return step.source.kind==PlayerPlanSourceKind::local_observation;else if constexpr(std::is_same_v<T,PlayerPlanTravel>)return step.source.kind==PlayerPlanSourceKind::authored;else if constexpr(std::is_same_v<T,PlayerPlanWork>||std::is_same_v<T,PlayerPlanCourse>)return step.source.kind==PlayerPlanSourceKind::local_observation&&step.source.source_id==value.context.role_id;else if constexpr(std::is_same_v<T,PlayerPlanRest>)return step.source.kind==PlayerPlanSourceKind::own_estate&&step.source.source_id==value.estate_id;else if constexpr(std::is_same_v<T,PlayerPlanContinue>)return step.source.kind==PlayerPlanSourceKind::existing_action&&step.source.source_id==value.action_id;else return step.source.kind==PlayerPlanSourceKind::accepted_task&&step.source.source_id==value.task_id;},step.data);
        if(!source_matches)return common_failure("PlayerPlanSourceMismatch",step.id);
        if(step.authorized&&step.data.index()!=2&&step.data.index()!=3)return common_failure("PlayerPlanNotAuthorizable",step.id);
        auto terms=terms_shape(step);if(!terms.ok)return terms;
        if(step.started_tick.has_value()!=step.start_result_id.has_value()||step.completed_tick.has_value()!=step.completion_result_id.has_value()||(step.completed_tick&&!step.started_tick)||(step.started_tick&&*step.started_tick<plan.created_tick)||(step.completed_tick&&*step.completed_tick<*step.started_tick)||(step.action_id&&(!step.started_tick||step.action_id->empty()||!actions.insert(*step.action_id).second))||(step.start_result_id&&step.start_result_id->empty())||(step.completion_result_id&&step.completion_result_id->empty()))return common_failure("InvalidPlayerPlanBoundary",step.id);
        if(step.started_tick&&!step.action_id&&!std::holds_alternative<PlayerPlanMove>(step.data))return common_failure("PlayerPlanActionReferenceRequired",step.id);
        if(step.started_tick&&step.source.observed_tick>*step.started_tick)return common_failure("PlayerPlanObservationAfterStart",step.id);
        if(step.start_result_id&&!boundary_results.insert(*step.start_result_id).second)return common_failure("PlayerPlanResultAlreadyBound",step.id);
        if(step.completion_result_id&&step.completion_result_id!=step.start_result_id&&!boundary_results.insert(*step.completion_result_id).second)return common_failure("PlayerPlanResultAlreadyBound",step.id);
        if(step.started_tick&&!step.action_id&&(!step.completed_tick||step.completed_tick!=step.started_tick||step.completion_result_id!=step.start_result_id))return common_failure("PlayerPlanInstantBoundaryMismatch",step.id);
        if(std::holds_alternative<PlayerPlanTaskReminder>(step.data)&&step.started_tick&&(!step.completed_tick||step.completed_tick!=step.started_tick||step.completion_result_id!=step.start_result_id))return common_failure("PlayerPlanReminderBoundaryMismatch",step.id);
        const bool earlier_unfinished=unfinished;
        if(step.phase==PlayerPlanStepPhase::completed){if(!step.completed_tick||unfinished)return common_failure("InvalidPlayerPlanCompletedOrder",step.id);}else unfinished=true;
        if(step.started_tick&&!step.completed_tick){if(earlier_unfinished||!step.action_id||step.phase!=PlayerPlanStepPhase::active||++active>1)return common_failure("InvalidPlayerPlanActiveStep",step.id);}else if(step.phase==PlayerPlanStepPhase::active)return common_failure("PlayerPlanStartRequired",step.id);
        if(step.completed_tick&&step.phase!=PlayerPlanStepPhase::completed&&step.phase!=PlayerPlanStepPhase::blocked)return common_failure("InvalidPlayerPlanTerminalPhase",step.id);
        if(const auto* continued=std::get_if<PlayerPlanContinue>(&step.data))if(step.action_id&&*step.action_id!=continued->action_id)return common_failure("PlayerPlanActionMismatch",step.id);
    }
    return common_success();
}
// 同一原旅程的失能只沿永久step精确绑定；普通到达、异人、异时和混合payload都不接受。
Outcome<std::optional<StableId>> player_plan_naval_navigation_termination_reason(const CommonStore& common,const NavalState& naval,const NavalCatalog& catalog,const StableId& actor,const StableId& action,const ReceiptDetails& receipt,Integer tick){
    const auto mismatch=[&](){return Outcome<std::optional<StableId>>{false,{},common_failure("PlayerPlanNavalNavigationMismatch",action).error};};
    const auto* advance=std::get_if<AdvanceDetails>(&receipt.data);const auto* completion=advance?advance->completed_actions.find(action):nullptr;
    const bool claimed=completion&&completion->code==std::optional<StableId>{"NavalNavigationDisabled"};
    const NavalNavigationStepFact* stop=nullptr;
    for(const auto& row:naval.navigation.steps)if(row.movement.action_id==action&&row.interrupted){if(stop)return mismatch();stop=&row;}
    if(!stop&&!claimed)return {true,std::nullopt,{}};
    if(!catalog.navigation_policy||!stop||common.action(action)||tick<0||tick>common.tick()||!completion)return mismatch();
    auto source=query_naval_navigation_termination(common,naval.navigation,stop->id,action);if(!source.ok)return {false,{},source.error};
    if(!source.value)return mismatch();const auto& value=*source.value;
    if(value.source_id!=stop->id||value.action_id!=action||value.actor_id!=actor||value.tick!=tick||value.reason!="NavalNavigationDisabled")return mismatch();
    if(!completion->ok||completion->completed!=std::optional<bool>{true}||completion->success!=std::optional<bool>{false}||completion->code!=std::optional<StableId>{value.reason}||completion->growth||completion->task||completion->family||completion->military||completion->already_completed||!completion->military_field_order.empty())return mismatch();
    return {true,std::optional<StableId>{value.reason},{}};
}
// 保存或安装前核本人行动和真实回执，不把缺失结果默认为成功。
Status validate_player_plan(const CommonStore& common,const PlayerPlanState& state,const GrowthTacticLessonState* lessons,const TaskState* personal_tasks){
    auto shape=validate_player_plan_shape(state);if(!shape.ok||!state.current)return shape;const auto& plan=*state.current;
    if(plan.actor_id!=common.state().metadata.player_actor_id||!common.actor(plan.actor_id)||plan.created_tick>common.tick()||(plan.budget&&plan.budget->approved_tick>common.tick()))return common_failure("InvalidPlayerPlanActor",plan.id);
    for(const auto& step:plan.steps){
        if(step.source.observed_tick>common.tick())return common_failure("InvalidPlayerPlanObservation",step.id);
        if(step.started_tick){const auto* receipt=result_at(common,step.start_result_id,step.started_tick,!std::holds_alternative<PlayerPlanTaskReminder>(step.data));if(!receipt)return common_failure("MissingPlayerPlanStartResult",step.id);if(*step.started_tick>common.tick())return common_failure("InvalidPlayerPlanStartTick",step.id);if(std::holds_alternative<PlayerPlanMove>(step.data)&&(!receipt->details||!std::holds_alternative<Done>(receipt->details->data)))return common_failure("PlayerPlanStartResultMismatch",step.id);if(std::holds_alternative<PlayerPlanContinue>(step.data)&&(!receipt->details||!std::holds_alternative<AdvanceDetails>(receipt->details->data)))return common_failure("PlayerPlanStartResultMismatch",step.id);
            if(const auto* reminder=std::get_if<PlayerPlanTaskReminder>(&step.data)){const auto* advance=receipt->details?std::get_if<AdvanceDetails>(&receipt->details->data):nullptr;const auto* completed=advance&&step.action_id?advance->completed_actions.find(*step.action_id):nullptr;if(!completed||!completed->task||completed->task->task_id!=std::optional<StableId>{reminder->task_id})return common_failure("PlayerPlanReminderResultMismatch",step.id);}
            if(step.action_id&&!std::holds_alternative<PlayerPlanContinue>(step.data)&&!std::holds_alternative<PlayerPlanTaskReminder>(step.data)){const auto* started=receipt->details?std::get_if<ActionStarted>(&receipt->details->data):nullptr;if(!started||started->action_id!=*step.action_id)return common_failure("PlayerPlanStartResultMismatch",step.id);}
        }
        if(step.action_id&&!step.completed_tick){const auto* action=common.action(*step.action_id);if(!action||(action->actor_id!=plan.actor_id&&std::find(action->participants.begin(),action->participants.end(),plan.actor_id)==action->participants.end()))return common_failure("MissingPlayerPlanActiveAction",step.id);const auto kind=action_kind(step.data);if(!kind.empty()&&kind!=action->kind)return common_failure("PlayerPlanActionKindMismatch",step.id);}
        if(step.completed_tick){const auto* receipt=result_at(common,step.completion_result_id,step.completed_tick);if(!receipt)return common_failure("MissingPlayerPlanCompletionResult",step.id);if(*step.completed_tick>common.tick())return common_failure("InvalidPlayerPlanCompletionTick",step.id);
            // 私信原来源先于通用形状：隐藏推进仍必须同一实际结果，绝不泛准Personal/Done。
            if(personal_tasks&&personal_tasks->personal_messages&&step.action_id){
                bool original=false;for(const auto& row:personal_tasks->personal_messages->starts.rows())if(row.second.action_id==*step.action_id)original=true;
                if(original||step.stop_reason.rfind("TaskPersonalMessage",0)==0){auto valid=validate_player_plan_personal_message_completion(common,*personal_tasks,*personal_tasks->personal_messages,plan.actor_id,step,*receipt);if(!valid.ok)return valid;continue;}
            }if(personal_tasks&&personal_tasks->personal_reports&&step.action_id){
                bool original=false;for(const auto& row:personal_tasks->personal_reports->starts.rows())if(row.second.action_id==*step.action_id)original=true;
                if(original||step.stop_reason.rfind("TaskPersonalReport",0)==0){auto valid=validate_player_plan_personal_report_completion(common,*personal_tasks,*personal_tasks->personal_reports,plan.actor_id,step,*receipt);if(!valid.ok)return valid;continue;}
            }if(personal_tasks&&personal_tasks->personal_acceptances&&step.action_id){
                bool original=false;for(const auto& row:personal_tasks->personal_acceptances->starts.rows())if(row.second.action_id==*step.action_id)original=true;
                if(original||step.stop_reason.rfind("TaskPersonalAcceptance",0)==0){auto valid=validate_player_plan_personal_acceptance_completion(common,*personal_tasks,*personal_tasks->personal_acceptances,plan.actor_id,step,*receipt);if(!valid.ok)return valid;continue;}
            }
            if(lessons&&step.action_id){
            auto lesson=player_plan_tactic_lesson_completion(common,*lessons,plan.actor_id,*step.action_id,*receipt);if(!lesson.ok)return {false,{},lesson.error};
            if(lesson.value){
                if(!std::holds_alternative<PlayerPlanContinue>(step.data)||(step.phase==PlayerPlanStepPhase::completed)!=lesson.value->success||step.stop_reason!=lesson.value->code)return common_failure("PlayerPlanTacticLessonMismatch",step.id);
                continue;
            }
        }if(step.action_id){if(!receipt->details)return common_failure("PlayerPlanCompletionResultMismatch",step.id);if(!std::holds_alternative<AdvanceDetails>(receipt->details->data)&&!result_at(common,step.completion_result_id,step.completed_tick,true))return common_failure("PlayerPlanCompletionActorMismatch",step.id);if(!std::holds_alternative<AdvanceDetails>(receipt->details->data)&&(step.stop_reason=="RecoveryPatientDied"||step.stop_reason=="RecoveryProviderDied")){
                // 同次零时死亡可以终止已开工疗程；World随后必须以实际缓存对象/永久closure双向核准。
                if(!std::holds_alternative<PlayerPlanContinue>(step.data)||step.phase!=PlayerPlanStepPhase::blocked||common.action(*step.action_id))return common_failure("PlayerPlanRecoveryMismatch",step.id);
            }else if(receipt->kind=="rescue"||receipt->kind=="return"||receipt->kind=="reroute"){if((!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked||step.stop_reason!="PlayerPlanTravelChanged")return common_failure("PlayerPlanChangedTravelResultMismatch",step.id);}else if(const auto* naval=std::get_if<NavalBattleCommandReceipt>(&receipt->details->data)){
                // 本层只核真实缓存的有限形状；原action与永久海军终止来源由World绑定继续精确核验。
                const auto expected=naval->kind=="naval_capture"?StableId{"NavalCommanderCaptured"}:StableId{"NavalTravelUnserviceable"};
                if((naval->kind!="naval_capture"&&naval->kind!="naval_battle_continue")||receipt->kind!=naval->kind||naval->actor_id!=plan.actor_id||naval->tick!=*step.completed_tick||(!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked||step.stop_reason!=expected)return common_failure("PlayerPlanNavalTerminationMismatch",step.id);
            }else if(const auto* repair=std::get_if<NavalRepairReceipt>(&receipt->details->data)){
                // 零时只有实际取消可以关闭原工单；接敌、换员与完工必须来自原Advance。
                if(receipt->kind!="naval_repair_cancel"||repair->source_id.empty()||repair->action_id!=*step.action_id||repair->actor_id!=plan.actor_id||repair->tick!=*step.completed_tick||!repair->completed||repair->success||repair->replayed||repair->reason!="NavalRepairCancelled"||!std::holds_alternative<PlayerPlanContinue>(step.data)||step.phase!=PlayerPlanStepPhase::blocked||step.stop_reason!=repair->reason)return common_failure("PlayerPlanNavalRepairMismatch",step.id);
            }else if(const auto* care=std::get_if<FamilyEffect>(&receipt->details->data)){
                // 此层只准真实本人零时取消形状；World随后以原照护事实核准全部业务来源。
                const bool relief_cancel=receipt->kind=="family_care_relief_cancel";const StableId stopped=relief_cancel?"CareReliefProviderStopped":"CaregiverStopped";if((!relief_cancel&&receipt->kind!="family_child_care_cancel")||care->success!=std::optional<bool>{true}||care->actor_id!=std::optional<StableId>{plan.actor_id}||care->player_actor_id!=care->actor_id||!care->fact_id||care->fact_id->empty()||care->code||care->already_completed||care->heir_actor_id||!std::holds_alternative<PlayerPlanContinue>(step.data)||step.phase!=PlayerPlanStepPhase::blocked||step.stop_reason!=stopped)return common_failure("PlayerPlanChildCareMismatch",step.id);
            }else if(const auto* cancellation=std::get_if<GrowthEffect>(&receipt->details->data)){const bool study=cancellation->kind=="study_cancel"&&!cancellation->success&&cancellation->code=="StudyCancelled";const bool rest=cancellation->kind=="rest_cancel";const bool recovery=cancellation->kind=="recovery_cancel"&&receipt->kind=="recovery_cancel"&&cancellation->success&&cancellation->code.empty()&&std::holds_alternative<PlayerPlanContinue>(step.data);const auto expected=study?StableId{"StudyCancelled"}:recovery?StableId{"RecoveryCancelled"}:StableId{"RestCancelled"};if((!study&&!rest&&!recovery)||cancellation->action_id!=*step.action_id||step.phase!=PlayerPlanStepPhase::blocked||step.stop_reason!=expected)return common_failure("PlayerPlanCancellationResultMismatch",step.id);}else{const auto* advance=std::get_if<AdvanceDetails>(&receipt->details->data);const auto* completed=advance?advance->completed_actions.find(*step.action_id):nullptr;if(!completed)return common_failure("PlayerPlanCompletionResultMismatch",step.id);const auto military_code=completed->code.value_or(StableId{});const bool military_stop=military_code=="MilitaryCommanderKilled"||military_code=="MilitaryCommanderCaptured"||military_code=="MilitaryCommanderDeserted";const bool navigation_stop=military_code=="NavalNavigationDisabled";if(navigation_stop&&((!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked||completed->completed!=std::optional<bool>{true}||completed->success!=std::optional<bool>{false}))return common_failure("PlayerPlanNavalNavigationMismatch",step.id);if(military_stop&&((!std::holds_alternative<PlayerPlanTravel>(step.data)&&!std::holds_alternative<PlayerPlanContinue>(step.data))||step.phase!=PlayerPlanStepPhase::blocked||completed->completed!=std::optional<bool>{true}||completed->success!=std::optional<bool>{false}))return common_failure("PlayerPlanMilitaryTerminationMismatch",step.id);const bool success=completed->ok&&completed->success.value_or(true)&&completed->completed.value_or(true)&&(!completed->growth||completed->growth->success)&&(!completed->task||completed->task->success)&&(!completed->family||completed->family->success.value_or(true));StableId code=completed->code.value_or(StableId{});if(completed->growth&&!completed->growth->code.empty())code=completed->growth->code;if(completed->task&&completed->task->code)code=*completed->task->code;if(completed->family&&completed->family->code)code=*completed->family->code;if((step.phase==PlayerPlanStepPhase::completed)!=success||step.stop_reason!=code)return common_failure("PlayerPlanCompletionOutcomeMismatch",step.id);}}}
    }
    return common_success();
}
} // 全世界仍须先核原领域参数与资源；本validator只维护计划和真实边界之间的约束。
