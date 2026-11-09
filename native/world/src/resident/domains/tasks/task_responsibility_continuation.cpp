// 读取冻结接受与当前连续性两种来源；不会接受新使命、开始劳动、分发资源或自动结束会话。
#include "task_responsibility_continuation.hpp"
#include "governance_task_rules.hpp"
#include "envoy_task_rules.hpp"
namespace hanmo::resident {
namespace {
using Result=Outcome<std::optional<TaskResponsibilityContinuationSource>>;
// 当前Action必须属于本人原责任；不能借另一活动占用证明本任务仍在进行。
Status action_source(const CommonStore& common,const TaskRecord& task,const StableId& id,const StableId& kind){
    const auto* actor=common.actor(task.assignee);const auto* action=common.action(id);
    if(!actor||!action||action->id!=id||action->kind!=kind||action->actor_id!=task.assignee
        ||actor->action_id!=id||action->start_tick<task.accepted_tick||action->start_tick>common.tick()
        ||action->remaining_ticks<1||std::count(action->participants.begin(),action->participants.end(),task.assignee)!=1)
        return common_failure("InvalidTaskResponsibilityAction",task.id,ErrorKind::internal);
    return common_success();
}
}
Result read_task_responsibility_continuation(const CommonStore& common,const TaskState& tasks,
    const TaskResponsibilityContinuationRead& read,const StableId& observer,const StableId& id){
    const auto fail=[&](const StableId& code)->Result{return {false,{},common_failure(code,id,ErrorKind::internal).error};};
    const auto* task=tasks.tasks.find(id);
    if(!task||task->id!=id||observer.empty()||task->assignee!=observer||!common.actor(observer))return fail("TaskResponsibilityNotKnown");
    const auto* governance=read.governance?read.governance->governance_contracts.find(id):nullptr;
    const auto* envoy=read.envoy?read.envoy->envoy_contracts.find(id):nullptr;
    if(governance&&envoy)return fail("AmbiguousTaskResponsibilitySource");
    if(!governance&&!envoy)return {true,std::nullopt,{}};
    TaskResponsibilityContinuationSource out;out.source_identity=common.state().identity;out.actor_id=observer;out.task_id=id;
    if(governance){
        if(read.governance->version!=1)return fail("UnsupportedGovernanceTaskVersion");
        GovernanceTaskRead access(common,tasks,*read.governance);auto source=access.governance_task_source(id);if(!source.ok)return {false,{},source.error};
        const auto& value=source.value;const auto& a=value.acceptance;
        if(a.executor_actor_id!=observer||a.accepted_tick<0||a.accepted_tick>common.tick()
            ||(value.work&&value.work->completed_tick>common.tick())
            ||(value.completion&&value.completion->completed_tick>common.tick())
            ||(value.interruption&&value.interruption->stopped_tick>common.tick()))return fail("TaskResponsibilityBeforeClockCommit");
        if(!value.completion){
            if(value.work){if(value.task_status!="ready"||common.action(governance->action_id))return fail("InvalidGovernanceReadySource");}
            else if(!value.interruption){
                if(value.task_status!="active")return fail("InvalidGovernanceActiveSource");
                auto valid=action_source(common,*task,governance->action_id,"governance_appease");if(!valid.ok)return {false,{},valid.error};out.action_id=governance->action_id;
            }else if(value.task_status!="settling"||common.action(governance->action_id))return fail("InvalidGovernanceInterruptionSource");
        }
        out.pending_code=governance->pending_code.empty()?governance->execution_note_code:governance->pending_code;
        if(out.pending_code.size()>4096)return fail("TaskResponsibilityTextLimit");out.responsibility=std::move(source.value);
    }else{
        if(!read.missions||!read.presentations||(read.envoy->version==2&&!read.reports))return fail("TaskResponsibilityPoliticalReadRequired");
        EnvoyTaskRead access=read.reports?EnvoyTaskRead(common,tasks,*read.envoy,*read.missions,*read.presentations,*read.reports):EnvoyTaskRead(common,tasks,*read.envoy,*read.missions,*read.presentations);
        auto source=access.envoy_task_source(id);if(!source.ok)return {false,{},source.error};const auto& value=source.value;
        if(value.acceptance.envoy_actor_id!=observer||(value.delivery&&value.delivery->completed_tick>common.tick())
            ||(value.completion&&value.completion->completed_tick>common.tick()))return fail("TaskResponsibilityBeforeClockCommit");
        if(envoy->action){
            if(value.completion||envoy->action->kind!=EnvoyTaskActionKind::deliver&&envoy->action->kind!=EnvoyTaskActionKind::report)return fail("InvalidEnvoyResponsibilityAction");
            const bool reporting=envoy->action->kind==EnvoyTaskActionKind::report;
            if(reporting!=bool(value.delivery))return fail("InvalidEnvoyResponsibilityAction");
            auto valid=action_source(common,*task,envoy->action->action_id,reporting?"envoy_report":"envoy_deliver");if(!valid.ok)return {false,{},valid.error};out.action_id=envoy->action->action_id;
        }
        if(!value.completion){
            if(value.delivery&&!read.report_continuity)return fail("TaskResponsibilityReportContinuityRequired");
            auto continuity=value.delivery?read.report_continuity(value.acceptance,*value.delivery):read.missions->diplomatic_mission_continuity(value.acceptance.mission_authority_fact_id,observer);if(!continuity.ok)return {false,{},continuity.error};
            if(static_cast<Integer>(continuity.value.kind)<0||static_cast<Integer>(continuity.value.kind)>static_cast<Integer>(DiplomaticMissionContinuityKind::terminate_principal_dissolved))return fail("InvalidTaskResponsibilityContinuity");
            out.mission_continuity=std::move(continuity.value);
        }
        out.module_version=read.envoy->version;out.pending_code=envoy->pending_code;if(out.pending_code.size()>4096)return fail("TaskResponsibilityTextLimit");out.responsibility=std::move(source.value);
    }
    return {true,std::move(out),{}};
}
}
