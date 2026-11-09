// 同一当前续办源供原Action逐半日和原ready清算复用；绝不绕过原死亡/健康/本金门。
#include "political_governance_continuation_detail.hpp"
namespace hanmo::resident {
namespace {
// 只对原改派暂停作有来源的替代，其它失败和终结结果不由新政权抹去。
Outcome<GovernanceContinuityResult> continuation(const PoliticalGovernanceContinuationContext& x,const StableId& authority,const StableId& executor,const std::optional<StableId>& contract,Outcome<GovernanceContinuityResult> original){
 using namespace governance_continue_detail;if(!original.ok||original.value.kind!=GovernanceContinuityKind::needs_reassignment||(original.value.code!="GovernanceControlOrAssetsChanged"&&original.value.code!="GovernanceOfficeVacant"))return original;
 auto valid=validate_political_governance_continuations(x);if(!valid.ok)return {false,{},valid.error};const PoliticalGovernanceContinuation* chosen=nullptr;
 for(const auto& row:x.state.continuations.rows())if(row.second.authority_fact_id==authority&&row.second.executor_actor_id==executor&&(!contract||row.second.contract_id==*contract)){
  if(chosen&&chosen->task_id!=row.second.task_id)return {false,{},common_failure("GovernanceContinuationTaskAmbiguous",authority).error};chosen=&row.second;
 }
 if(!chosen)return original;const auto& r=*chosen;auto a=x.original_authority.governance_authority_approval(authority);if(!a.ok)return {false,{},a.error};auto t=x.tasks.governance_task_source(r.task_id);if(!t.ok)return {false,{},t.error};if(t.value.completion||t.value.interruption||(t.value.task_status!="active"&&t.value.task_status!="ready"))return original;
 auto e=x.takeover.find_political_responsibility_for_execution({PoliticalResponsibilityKind::relief_contract,r.contract_id,{},{}},x.common.state().identity);if(!e.ok)return {false,{},e.error};if(!e.value||e.value->status!=PoliticalResponsibilityExecutionStatus::approved||!e.value->decision||e.value->registration_fact_id!=r.registration_fact_id||e.value->decision->takeover_fact_id!=r.approval_fact_id||e.value->decision->decision_fact_id!=r.decision_fact_id)return original;
 valid=execution_source(x,a.value,t.value.acceptance,*e.value);if(!valid.ok)return {false,{},valid.error};if(!current_assets(x,a.value,r.authority.organization_id).ok)return original;
 const auto* org=x.organizations.organizations.find(r.authority.organization_id);if(!org)return {false,{},common_failure("GovernanceContinuationOrganizationMissing",r.authority.organization_id).error};if(org->status==OrganizationStatus::dissolved)return {true,{GovernanceContinuityKind::needs_reassignment,"GovernanceSuccessorDissolved",r.id},{}};
 // 原连续性政策保持：已受领责任不按今日Grant到期倒销；空职型改为核实际承办新署。
 if(a.value.continuity_policy_id=="stop_when_vacant"){
  const auto* office=x.organizations.offices.find(r.authority.office_id);const auto* actor=office&&office->holder_actor_id?x.common.actor(*office->holder_actor_id):nullptr;
  if(!office||office->organization_id!=r.authority.organization_id||office->status!=OfficeStatus::active||!actor||!actor->alive)return {true,{GovernanceContinuityKind::needs_reassignment,"GovernanceSuccessorOfficeVacant",r.id},{}};
 }else if(a.value.continuity_policy_id!="continue_under_organization")return {false,{},common_failure("UnsupportedGovernanceContinuity",a.value.continuity_policy_id,ErrorKind::unsupported_capability).error};
 return {true,{GovernanceContinuityKind::continue_work,{},r.id},{}};
}
}
// 原权源和新接单准入完全委托旧系统，不通过续办重新接受过期或已受领责任。
Outcome<GovernanceAuthorityApproval> PoliticalGovernanceContinuationAccess::governance_authority_approval(const StableId& id)const{return context_.original_authority.governance_authority_approval(id);}
Outcome<GovernanceAuthorityApproval> PoliticalGovernanceContinuationAccess::governance_authority_for_acceptance(const StableId& id,const StableId& actor,Integer revision)const{return context_.original_authority.governance_authority_for_acceptance(id,actor,revision);}
Outcome<GovernanceContinuityResult> PoliticalGovernanceContinuationAccess::governance_continuity(const StableId& id,const StableId& actor)const{return continuation(context_,id,actor,{},context_.original_authority.governance_continuity(id,actor));}
// 新profile的继付同样先要求新续办；不借旧relief分支绕过它。接管读须借同一候选。
Outcome<GovernanceContinuityResult> PoliticalGovernanceContinuationAccess::governance_relief_continuity(const StableId& id,const StableId& actor,const StableId& contract,const PoliticalTakeoverRead& takeover)const{if(&takeover!=&context_.takeover)return {false,{},common_failure("GovernanceContinuationWrongTakeoverRead").error};return continuation(context_,id,actor,contract,context_.original_authority.governance_continuity(id,actor));}
}
