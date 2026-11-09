// 接受是本人的新责任，实际旅行与当面交接随后仍走原Common行动和政治呈递复验。
#include "envoy_task_admission.hpp"
namespace hanmo::resident {
namespace {
// 完整候选身份含执行epoch，不能把同一tick或旧冷历史当当前批准。
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
// 公共fact头必须与政治typed批准同时匹配；任意存在的fact不能授权任务。
bool fact_matches(const CommonStore& common,const StableId& id,const StableId& kind){const auto* fact=common.state().facts.find(id);return fact&&fact->id==id&&fact->kind==kind&&fact->domain==diplomatic_fact_domain;}
}
Outcome<DiplomaticMissionApproval> preview_envoy_task_acceptance(const CommonStore& common,const EnvoyTaskState& envoy,const DiplomaticMissionRead& politics,const StableId& actor_id,const StableId& fact_id,Integer expected_revision){
    if(expected_revision!=common.revision())return {false,{},common_failure("StaleWorldRevision").error};
    if(envoy.version!=1&&envoy.version!=2)return {false,{},common_failure("UnsupportedEnvoyTaskVersion").error};
    const auto* actor=common.actor(actor_id);if(!actor||!actor->alive||!actor->available||actor->hp<=0)return {false,{},common_failure("ActorUnavailable",actor_id).error};if(!actor->action_id.empty())return {false,{},common_failure("ActorBusy",actor_id).error};
    auto quote=politics.diplomatic_mission_for_acceptance(fact_id,actor_id);if(!quote.ok)return quote;const auto& a=quote.value;
    if(envoy.version==1&&a.return_report_required)return {false,{},common_failure("EnvoyReportVersionRequired").error};
    if(!same_identity(common.state().identity,a.source_identity))return {false,{},common_failure("StaleDiplomaticMissionApproval").error};
    if(a.mission_authority_fact_id!=fact_id||a.envoy_actor_id!=actor_id||a.proposal_id.empty()||a.policy_id.empty()||a.rule_name_key.empty()||a.terms_revision<1||a.issuer_organization_id.empty()||a.target_organization_id.empty()||a.issuer_office_id.empty()||a.issuer_actor_id.empty()||a.issuer_grant_id.empty()||a.issuer_authority_epoch<0||a.target_office_id.empty()||a.delivery_site_id.empty()||a.report_office_id.empty()||a.report_site_id.empty()||a.continuity_policy_id.empty()||!fact_matches(common,fact_id,diplomatic_mission_authority_fact_kind)||!fact_matches(common,a.proposal_fact_id,diplomatic_proposal_fact_kind))return {false,{},common_failure("InvalidEnvoyMissionAuthority",fact_id,ErrorKind::internal).error};
    if(common.tick()<0||a.issued_tick<0||a.issued_tick>common.tick()||common.tick()>=a.acceptance_expires_tick||common.tick()>=a.delivery_expires_tick)return {false,{},common_failure("EnvoyMissionExpired",fact_id).error};
    // 只核实际交接最低工时，绝不猜往返路程；严格小于绝对递交失效点且用减法避免溢出。
    if(a.delivery_work_ticks<1||a.report_work_ticks<0||(a.return_report_required&&a.report_work_ticks<1)||a.delivery_work_ticks>=a.delivery_expires_tick-common.tick())return {false,{},common_failure("InvalidEnvoyMissionTiming",fact_id).error};
    for(const auto& row:envoy.envoy_contracts.rows())if(row.second.acceptance.mission_authority_fact_id==fact_id)return {false,{},common_failure("EnvoyMissionAlreadyAccepted",fact_id).error};
    return quote;
}
Outcome<EnvoyTaskAcceptance> accept_envoy_task_in_candidate(CommonStore& common,TaskState& tasks,EnvoyTaskState& envoy,const CommonCatalog& catalog,const DiplomaticMissionRead& politics,const StableId& actor_id,const StableId& fact_id,Integer expected_revision){
    auto quote=preview_envoy_task_acceptance(common,envoy,politics,actor_id,fact_id,expected_revision);if(!quote.ok)return {false,{},quote.error};const auto& a=quote.value;const auto* name=catalog.texts.find(a.rule_name_key);if(!name||name->empty())return {false,{},common_failure("UnknownEnvoyMissionText",a.rule_name_key).error};
    const auto serial=common.state().next_instance_id;auto task_id=common.next_id("task");if(!task_id.ok)return {false,{},task_id.error};auto contract_id=common.next_id("envoy_contract");if(!contract_id.ok)return {false,{},contract_id.error};auto accepted_id=common.next_id("envoy_accept");if(!accepted_id.ok)return {false,{},accepted_id.error};
    EnvoyTaskAcceptance accepted;accepted.fact_id=accepted_id.value;accepted.task_id=task_id.value;accepted.contract_id=contract_id.value;accepted.mission_authority_fact_id=fact_id;accepted.proposal_id=a.proposal_id;accepted.proposal_fact_id=a.proposal_fact_id;accepted.terms_revision=a.terms_revision;accepted.issuer_organization_id=a.issuer_organization_id;accepted.target_organization_id=a.target_organization_id;accepted.envoy_actor_id=actor_id;accepted.target_office_id=a.target_office_id;accepted.delivery_site_id=a.delivery_site_id;accepted.report_office_id=a.report_office_id;accepted.report_site_id=a.report_site_id;accepted.continuity_policy_id=a.continuity_policy_id;accepted.display_name=*name;accepted.accepted_revision=common.revision();accepted.accepted_tick=common.tick();accepted.delivery_expires_tick=a.delivery_expires_tick;accepted.return_report_required=a.return_report_required;
    TaskRecord task;task.id=accepted.task_id;task.offer_id=accepted.proposal_id;task.assignee=actor_id;task.status="active";task.serial=serial;task.accepted_tick=accepted.accepted_tick;task.deadline_tick=accepted.delivery_expires_tick;
    if(!tasks.tasks.insert(task.id,task))return {false,{},common_failure("DuplicateEnvoyTask",task.id,ErrorKind::internal).error};EnvoyTaskContract contract;contract.acceptance=accepted;if(!envoy.envoy_contracts.insert(task.id,contract))return {false,{},common_failure("DuplicateEnvoyContract",task.id,ErrorKind::internal).error};auto written=common.append_fact({accepted.fact_id,"envoy_task_accepted","tasks"});if(!written.ok)return {false,{},written.error};return {true,std::move(accepted),{}};
}
}
