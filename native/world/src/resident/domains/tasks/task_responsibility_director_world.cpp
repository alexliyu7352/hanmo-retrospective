// 只组合既有S3/S6只读口，动作仍由原中央dispatcher执行，故不新增状态或保存集。
#include "task_responsibility_director_world.hpp"
#include "resident/kernel/world_bundle.hpp"
namespace hanmo::resident {
TaskResponsibilityDirectorWorldAccess::TaskResponsibilityDirectorWorldAccess(const WorldBundle& w,const OrganizationRead& organizations)
    :world_(w),interaction_(w.services.interaction_access()),context_{{},w.interaction,*w.catalog->interaction,interaction_,organizations,{}}{
    if(w.governance_tasks_enabled())context_.sources.governance=&w.governance_tasks;
    if(w.envoy_tasks_enabled()){
        context_.sources.envoy=&w.envoy_tasks;
        present_.emplace(DiplomaticPresentRead{w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy,w.interaction,*w.catalog->interaction,interaction_});
        missions_.emplace(w.common,w.organization,*w.catalog->organization,*w.catalog->diplomacy,w.diplomacy);
        context_.sources.missions=&*missions_;context_.sources.presentations=&*missions_;
        if(w.envoy_tasks.version==2&&w.diplomacy.v2){reports_.emplace(*present_,*w.diplomacy.v2);context_.sources.reports=&*reports_;}
        context_.sources.report_continuity=[this](const EnvoyTaskAcceptance& a,const EnvoyTaskDelivery& delivery){return diplomatic_report_responsibility(*present_,a.mission_authority_fact_id,a.envoy_actor_id,delivery.diplomatic_presentation_fact_id);};
        context_.preview_envoy=[this](const StableId& actor,EnvoyTaskActionKind kind,const StableId& task,const StableId& provider,Integer revision)->Status{
            if(kind!=EnvoyTaskActionKind::deliver&&kind!=EnvoyTaskActionKind::report)return common_failure("InvalidEnvoyContinuationKind",task);
            const auto& w=world_;DiplomacyReadContext read{w.common,*w.catalog->common,w.organization,*w.catalog->organization,w.diplomacy,*w.catalog->diplomacy,w.interaction,*w.catalog->interaction,interaction_,w.tasks,w.envoy_tasks,w.services};
            DiplomacyCommand command=kind==EnvoyTaskActionKind::deliver?DiplomacyCommand{DiplomaticEnvoyDeliverCommand{task,provider,revision}}:DiplomacyCommand{DiplomaticEnvoyReportCommand{task,provider,revision}};
            auto quote=preview_diplomacy_business(read,actor,command);return quote.ok?common_success():Status{false,{},quote.error};
        };
    }
}
Outcome<std::unique_ptr<TaskResponsibilityDirectorWorldAccess>> TaskResponsibilityDirectorWorldAccess::create(const WorldBundle& w,const OrganizationRead& organization){
    if(!w.catalog||!w.catalog->interaction||!w.catalog->director||!w.catalog->director->authored_task_continuation||w.catalog->director->authored_task_continuation->version!=2)
        return {false,{},common_failure("TaskResponsibilityContinuationProfileRequired").error};
    if(w.envoy_tasks_enabled()&&(!w.diplomacy_enabled()||!w.catalog->organization||!w.catalog->diplomacy||(w.envoy_tasks.version==2&&!w.diplomacy.v2)))
        return {false,{},common_failure("TaskResponsibilityPoliticalReadRequired").error};
    return {true,std::unique_ptr<TaskResponsibilityDirectorWorldAccess>(new TaskResponsibilityDirectorWorldAccess(w,organization)),{}};
}
}
