// 保存的责任必须能反查同一政治历史批准；一个相同kind的公共fact不能补齐被篡改的条款。
#include "envoy_task_rules.hpp"
#include "envoy_task_report_reference.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 来源头只是一层索引，下面仍逐项比较政治typed冻结值。
bool fact_is(const CommonStore& common,const StableId& id,const StableId& kind,const StableId& domain){const auto* fact=common.state().facts.find(id);return fact&&fact->id==id&&fact->kind==kind&&fact->domain==domain;}
}
Outcome<EnvoyTaskAcceptance> read_envoy_task_acceptance(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const DiplomaticMissionRead& politics,const StableId& id){
    if(envoy.version!=1&&envoy.version!=2)return {false,{},common_failure("UnsupportedEnvoyTaskVersion").error};const auto* contract=envoy.envoy_contracts.find(id);const auto* task=tasks.tasks.find(id);if(!contract)return {false,{},common_failure("UnknownEnvoyTask",id).error};const auto& a=contract->acceptance;
    if(!task||a.task_id!=id||task->id!=id||task->offer_id!=a.proposal_id||task->assignee!=a.envoy_actor_id||task->serial<1||task->accepted_tick!=a.accepted_tick||task->deadline_tick!=a.delivery_expires_tick||task->contract_id||task->offer_ref||task->escort||!task->budget_wallet_id.empty()||a.contract_id.empty()||a.display_name.empty()||a.display_name.size()>4096||!common.actor(a.envoy_actor_id)||a.accepted_tick<0||a.accepted_tick>common.tick()||a.accepted_revision<0||a.accepted_revision>common.revision()||!fact_is(common,a.fact_id,"envoy_task_accepted","tasks")||!fact_is(common,a.mission_authority_fact_id,diplomatic_mission_authority_fact_kind,diplomatic_fact_domain)||!fact_is(common,a.proposal_fact_id,diplomatic_proposal_fact_kind,diplomatic_fact_domain))return {false,{},common_failure("InvalidEnvoyTaskSource",id,ErrorKind::internal).error};
    auto history=politics.diplomatic_mission_source(a.mission_authority_fact_id);if(!history.ok)return {false,{},history.error};const auto& original=history.value;
    if(original.mission_authority_fact_id!=a.mission_authority_fact_id||original.proposal_id!=a.proposal_id||original.proposal_fact_id!=a.proposal_fact_id||original.terms_revision!=a.terms_revision||original.issuer_organization_id!=a.issuer_organization_id||original.target_organization_id!=a.target_organization_id||original.envoy_actor_id!=a.envoy_actor_id||original.target_office_id!=a.target_office_id||original.delivery_site_id!=a.delivery_site_id||original.report_office_id!=a.report_office_id||original.report_site_id!=a.report_site_id||original.continuity_policy_id!=a.continuity_policy_id||original.delivery_expires_tick!=a.delivery_expires_tick||original.return_report_required!=a.return_report_required)return {false,{},common_failure("EnvoyAuthorityBindingMismatch",id,ErrorKind::internal).error};
    // 历史只核接受当时的合法窗，后来的世界时间不会让已受责任悄悄消失。
    if(original.issued_tick<0||original.issued_tick>a.accepted_tick||a.accepted_tick>=original.acceptance_expires_tick||a.accepted_tick>=a.delivery_expires_tick||original.delivery_work_ticks<1||original.delivery_work_ticks>=a.delivery_expires_tick-a.accepted_tick||original.report_work_ticks<0||(a.return_report_required&&original.report_work_ticks<1))return {false,{},common_failure("InvalidEnvoyAcceptanceTime",id,ErrorKind::internal).error};
    return {true,a,{}};
}
Status validate_envoy_task_acceptances(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const DiplomaticMissionRead& politics){
    if(envoy.version!=1&&envoy.version!=2)return common_failure("UnsupportedEnvoyTaskVersion");std::set<StableId> missions,contracts,accepted_facts;
    for(const auto& row:envoy.envoy_contracts.rows()){auto source=read_envoy_task_acceptance(common,tasks,envoy,politics,row.first);if(!source.ok)return {false,{},source.error};const auto& a=source.value;if(!missions.insert(a.mission_authority_fact_id).second||!contracts.insert(a.contract_id).second||!accepted_facts.insert(a.fact_id).second)return common_failure("DuplicateEnvoyResponsibility",row.first);}
    return common_success();
}
EnvoyTaskRead::EnvoyTaskRead(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations):common_(common),tasks_(tasks),envoy_(envoy),missions_(missions),presentations_(presentations){}
EnvoyTaskRead::EnvoyTaskRead(const CommonStore& common,const TaskState& tasks,const EnvoyTaskState& envoy,const DiplomaticMissionRead& missions,const DiplomacyPresentationRead& presentations,const DiplomaticEnvoyReportRead& reports):common_(common),tasks_(tasks),envoy_(envoy),missions_(missions),presentations_(presentations),reports_(&reports){}
// 读取与最终World冷校验分工：此口可在尚未clock_commit的完成候选读真实刚写入的相互来源。
Outcome<EnvoyTaskSource> EnvoyTaskRead::envoy_task_source(const StableId& id)const{
    auto accepted=read_envoy_task_acceptance(common_,tasks_,envoy_,missions_,id);if(!accepted.ok)return {false,{},accepted.error};const auto& a=accepted.value;const auto* contract=envoy_.envoy_contracts.find(id);const auto* task=tasks_.tasks.find(id);
    if(contract->delivery){const auto& d=*contract->delivery;
        if(d.task_id!=id||d.contract_id!=a.contract_id||d.acceptance_fact_id!=a.fact_id||d.action_id.empty()||d.completed_tick<a.accepted_tick||d.completed_tick>=a.delivery_expires_tick||task->objective_completed_tick!=d.completed_tick||!fact_is(common_,d.fact_id,"envoy_task_delivered","tasks")||!fact_is(common_,d.diplomatic_presentation_fact_id,diplomatic_presentation_fact_kind,diplomatic_fact_domain))return {false,{},common_failure("InvalidEnvoyDeliverySource",id,ErrorKind::internal).error};
        auto source=presentations_.diplomatic_presentation_source(d.diplomatic_presentation_fact_id);if(!source.ok)return {false,{},source.error};const auto& p=source.value;
        if(p.fact_id!=d.diplomatic_presentation_fact_id||p.proposal_id!=a.proposal_id||p.proposal_fact_id!=a.proposal_fact_id||p.envoy_actor_id!=a.envoy_actor_id||p.terms_revision!=a.terms_revision||p.completed_tick!=d.completed_tick||!p.task_binding)return {false,{},common_failure("EnvoyPresentationBindingMismatch",id,ErrorKind::internal).error};const auto& b=*p.task_binding;
        if(b.mission_authority_fact_id!=a.mission_authority_fact_id||b.task_id!=id||b.contract_id!=a.contract_id||b.action_id!=d.action_id||b.acceptance_fact_id!=a.fact_id)return {false,{},common_failure("EnvoyPresentationBindingMismatch",id,ErrorKind::internal).error};
    }
    if(contract->completion){const auto& f=*contract->completion;const auto kind=f.result==EnvoyTaskResult::completed?"envoy_task_completed":f.result==EnvoyTaskResult::cancelled?"envoy_task_cancelled":"envoy_task_failed";
        if(f.task_id!=id||f.contract_id!=a.contract_id||f.acceptance_fact_id!=a.fact_id||f.completed_tick<a.accepted_tick||!fact_is(common_,f.fact_id,kind,"tasks")||(contract->delivery?(f.delivery_fact_id!=contract->delivery->fact_id||f.completed_tick<contract->delivery->completed_tick):bool(f.delivery_fact_id)))return {false,{},common_failure("InvalidEnvoyCompletionSource",id,ErrorKind::internal).error};
        const auto expected=f.result==EnvoyTaskResult::completed?"success":f.result==EnvoyTaskResult::cancelled?"cancelled":"failed";
        if(task->status!=expected||task->settlement_id!=f.fact_id||task->reported_tick!=f.completed_tick)return {false,{},common_failure("EnvoyTerminalSourceMismatch",id,ErrorKind::internal).error};
        if(f.result==EnvoyTaskResult::completed&&(!contract->delivery||!f.reason.empty()||(a.return_report_required?(!f.report_action_id||f.report_action_id->empty()||f.report_action_id==contract->delivery->action_id):bool(f.report_action_id))))return {false,{},common_failure("EnvoyMissionNotFulfilled",id,ErrorKind::internal).error};
    }
    else if((contract->delivery?task->status!="ready":task->status!="active")||!task->settlement_id.empty()||task->reported_tick)return {false,{},common_failure("InvalidEnvoyPendingTask",id,ErrorKind::internal).error};
    EnvoyTaskSource result{a,task->status,contract->delivery,contract->completion};auto report=validate_envoy_task_report_reference(common_,envoy_.version,result,reports_);if(!report.ok)return {false,{},report.error};return {true,result,{}};
}
// 只在本域历史表按事实ID查找，仍反查完整任务与政治绑定，拒绝同一fact被两任务复用。
Outcome<EnvoyTaskDelivery> EnvoyTaskRead::envoy_task_delivery(const StableId& id)const{
    const EnvoyTaskContract* found=nullptr;for(const auto& row:envoy_.envoy_contracts.rows())if(row.second.delivery&&row.second.delivery->fact_id==id){if(found)return {false,{},common_failure("DuplicateEnvoyDeliveryFact",id,ErrorKind::internal).error};found=&row.second;}
    if(!found)return {false,{},common_failure("UnknownEnvoyDeliveryFact",id).error};auto source=envoy_task_source(found->acceptance.task_id);if(!source.ok)return {false,{},source.error};return {true,*source.value.delivery,{}};
}
// 已呈递和已完成是不同具名事实；读取终态不会把接受外交提议推导为任务已返报。
Outcome<EnvoyTaskCompletion> EnvoyTaskRead::envoy_task_completion(const StableId& id)const{
    const EnvoyTaskContract* found=nullptr;for(const auto& row:envoy_.envoy_contracts.rows())if(row.second.completion&&row.second.completion->fact_id==id){if(found)return {false,{},common_failure("DuplicateEnvoyCompletionFact",id,ErrorKind::internal).error};found=&row.second;}
    if(!found)return {false,{},common_failure("UnknownEnvoyCompletionFact",id).error};auto source=envoy_task_source(found->acceptance.task_id);if(!source.ok)return {false,{},source.error};return {true,*source.value.completion,{}};
}
}
