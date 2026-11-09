// 只认原使命、原呈递、本人Task四ID与真实政治报告的完整对应，不扫描远方Office或重新签发权限。
#include "envoy_task_report_reference.hpp"
namespace hanmo::resident {
namespace {
// 共用事实头先核具名领域，typed报告读取还须证明该事实的完整政治历史。
bool fact_is(const CommonStore& common,const StableId& id,const char* kind,const char* domain){const auto* fact=common.state().facts.find(id);return fact&&!id.empty()&&fact->id==id&&fact->kind==kind&&fact->domain==domain;}
}
Status validate_envoy_task_report_reference(const CommonStore& common,Integer version,const EnvoyTaskSource& source,const DiplomaticEnvoyReportRead* reports){
    if(version!=1&&version!=2)return common_failure("UnsupportedEnvoyTaskVersion");
    if(!source.completion)return common_success();const auto& f=*source.completion;const auto& a=source.acceptance;
    if(version==1)return f.diplomatic_report_fact_id?common_failure("EnvoyReportReferenceRequiresV2"):common_success();
    if(f.result!=EnvoyTaskResult::completed||!a.return_report_required)return f.diplomatic_report_fact_id?common_failure("UnexpectedEnvoyReportReference"):common_success();
    if(!source.delivery||!f.diplomatic_report_fact_id||f.diplomatic_report_fact_id->empty()||!f.report_action_id||f.report_action_id->empty())return common_failure("EnvoyReportFactRequired",a.task_id);
    const auto& d=*source.delivery;
    if(source.task_status!="success"||f.task_id!=a.task_id||f.contract_id!=a.contract_id||f.acceptance_fact_id!=a.fact_id||f.delivery_fact_id!=d.fact_id||d.task_id!=a.task_id||d.contract_id!=a.contract_id||d.acceptance_fact_id!=a.fact_id||d.diplomatic_presentation_fact_id.empty()||*f.report_action_id==d.action_id||f.completed_tick<=d.completed_tick||!fact_is(common,a.fact_id,"envoy_task_accepted","tasks")||!fact_is(common,d.fact_id,"envoy_task_delivered","tasks")||!fact_is(common,f.fact_id,"envoy_task_completed","tasks"))return common_failure("InvalidEnvoyReportCompletionSource",a.task_id);
    if(!fact_is(common,*f.diplomatic_report_fact_id,diplomatic_envoy_report_fact_kind,diplomatic_fact_domain))return common_failure("MissingDiplomaticEnvoyReportFact",a.task_id);
    if(!reports)return common_failure("MissingEnvoyReportSourceRead",a.task_id,ErrorKind::internal);
    auto actual=reports->diplomatic_envoy_report_source(*f.diplomatic_report_fact_id);if(!actual.ok)return {false,{},actual.error};const auto& r=actual.value;
    if(r.fact_id!=*f.diplomatic_report_fact_id||r.mission_authority_fact_id!=a.mission_authority_fact_id||r.proposal_id!=a.proposal_id||r.proposal_fact_id!=a.proposal_fact_id||r.presentation_fact_id!=d.diplomatic_presentation_fact_id||r.envoy_actor_id!=a.envoy_actor_id||r.terms_revision!=a.terms_revision||r.task_id!=a.task_id||r.contract_id!=a.contract_id||r.acceptance_fact_id!=a.fact_id||r.report_action_id!=*f.report_action_id||r.completed_tick!=f.completed_tick||r.report_site_id!=a.report_site_id||r.receiver.organization_id!=a.issuer_organization_id||r.receiver.office_id!=a.report_office_id||r.receiver.authority_epoch<0||r.receiver.actor_id.empty()||r.receiver.actor_id==a.envoy_actor_id||!common.actor(r.receiver.actor_id)||r.receiver_grant_id.empty())return common_failure("EnvoyReportBindingMismatch",a.task_id,ErrorKind::internal);
    return common_success();
}
}
