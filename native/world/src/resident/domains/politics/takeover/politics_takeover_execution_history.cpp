#include "resident/domains/military/military_model.hpp"
// 执行回执按共同事实分配序核当时最后决定；不通过今天的Owner、持有人或表顺序猜历史。
#include "politics_takeover_detail.hpp"
namespace hanmo::resident {
namespace {
// 只承认各责任域已注册的真实执行边界，未知header或跨域借事实不得追认权限。
bool execution_header(const FactRecord& fact,const PoliticalResponsibilityRef& ref,const StableId& prefix,Integer task_version){
    if(ref.kind==PoliticalResponsibilityKind::task_contract)return fact.domain=="tasks"&&((fact.kind=="task_contract_payment"&&prefix=="task_payment")||(task_version==2&&fact.kind=="task_static_payment"&&prefix=="task_static_payment"));
    if(ref.kind==PoliticalResponsibilityKind::relief_contract&&fact.domain=="tasks")return fact.kind=="governance_work_completed"&&prefix=="governance_work";
    if(ref.kind==PoliticalResponsibilityKind::relief_contract&&fact.domain=="political_governance_continuations")return fact.kind=="political_governance_continued"&&prefix=="political_governance_continuation";
    if(fact.domain!="local_economy")return false;
    if(ref.kind==PoliticalResponsibilityKind::relief_contract)return prefix=="public_relief_distributed"&&fact.kind=="public_relief_distributed";
    if(ref.kind==PoliticalResponsibilityKind::fiscal_assessment)return (prefix=="fiscal_payment"&&fact.kind=="fiscal_payment")||(prefix=="occupation_fiscal_payment"&&fact.kind=="occupation_fiscal_payment");
    if(prefix=="production_configuration"&&(fact.kind=="production_configuration_requested"||fact.kind=="production_configuration_cancelled"||fact.kind=="production_configuration_superseded"))return ref.kind==PoliticalResponsibilityKind::production_unit||ref.kind==PoliticalResponsibilityKind::production_asset_use||ref.kind==PoliticalResponsibilityKind::pending_configuration;
    return (prefix=="local_payment"&&fact.kind=="obligation_payment")||
        (prefix=="local_ration"&&fact.kind=="local_ration")||
        (prefix=="local_production"&&fact.kind=="local_production")||
        (prefix=="production_configuration"&&(fact.kind=="production_configuration_applied"||fact.kind=="production_configuration_deferred"));
}
}
Outcome<std::optional<PoliticalResponsibilityExecution>> PoliticalTakeoverAccess::political_responsibility_before_fact(const PoliticalResponsibilityRef& ref,const StableId& fact_id,Integer tick,const WorldIdentity& identity) const{
    using namespace takeover_detail;const auto& context=context_;const auto failure=[](const StableId& code,const StableId& id={}){return Outcome<std::optional<PoliticalResponsibilityExecution>>{false,{},common_failure(code,id).error};};
    if(!same_identity(identity,context.common.state().identity))return failure("PoliticalTakeoverRevisionMismatch");
    if(!valid_ref(ref))return failure("InvalidPoliticalResponsibilityReference");
    auto evaluation=political_takeover_evaluation_tick(context.common,context.phase,context.catalog.version,context.actual_clock);if(!evaluation.ok)return {false,{},evaluation.error};
    const auto* fact=context.common.state().facts.find(fact_id);const auto colon=fact_id.find(':');const auto sequence=colon==StableId::npos?std::optional<Integer>{}:organization_instance_sequence(fact_id,fact_id.substr(0,colon));
    if(!fact||fact->id!=fact_id||!execution_header(*fact,ref,fact_id.substr(0,colon),context.catalog.task_execution_version)||!sequence||*sequence>=context.common.state().next_instance_id||tick<0||tick>evaluation.value)return failure("InvalidPoliticalExecutionFactBoundary",fact_id);
    auto valid=validate_political_takeover_state(context);if(!valid.ok)return {false,{},valid.error};
    const PoliticalTakeoverRegistration* selected=nullptr;Integer last_registration=0;
    for(const auto& row:context.state.registrations.rows()){
        const auto number=organization_instance_sequence(row.first,"political_takeover_pending");if(!number)return failure("InvalidPoliticalTakeoverState",row.first);if(*number>=*sequence)continue;
        if(std::none_of(row.second.responsibilities.begin(),row.second.responsibilities.end(),[&](const auto& item){return key(item.responsibility)==key(ref);}))continue;
        if(row.second.tick>tick)return failure("InvalidPoliticalExecutionCausality",fact_id);if(*number>last_registration){last_registration=*number;selected=&row.second;}
    }
    if(!selected)return {true,std::nullopt,{}};
    PoliticalResponsibilityExecution out;out.registration_fact_id=selected->id;out.responsibility=ref;out.registration=registration_source(context.common,*selected,*context.catalog.rules.find(selected->rule_id));out.code="PoliticalResponsibilityPending";for(const auto& source:selected->responsibilities)if(key(source.responsibility)==key(ref))out.original_source_fact_ids=source.source_fact_ids;
    const PoliticalTakeoverApprovalRecord* approval=nullptr;Integer last_approval=0;
    for(const auto& row:context.state.approvals.rows())if(row.second.registration_fact_id==selected->id){auto number=organization_instance_sequence(row.first,"political_takeover");if(!number)return failure("InvalidPoliticalTakeoverState",row.first);if(*number>=*sequence)continue;if(row.second.tick>tick)return failure("InvalidPoliticalExecutionCausality",fact_id);if(*number>last_approval){last_approval=*number;approval=&row.second;}}
    if(!approval)return {true,std::move(out),{}};
    for(const auto& id:approval->decision_fact_ids){const auto& decision=context.state.decisions.find(id)->decision;if(key(decision.responsibility)!=key(ref))continue;const auto number=organization_instance_sequence(id,"political_responsibility");if(!number||*number>=*sequence)return failure("PoliticalExecutionBeforeDecision",fact_id);out.decision=decision;break;}
    if(!out.decision)return failure("PoliticalResponsibilityCoverageMismatch",selected->id);
    const auto disposition=out.decision->disposition;const bool executable=disposition==PoliticalResponsibilityDisposition::assume_by_successor||disposition==PoliticalResponsibilityDisposition::continue_under_original;
    out.status=executable?PoliticalResponsibilityExecutionStatus::approved:PoliticalResponsibilityExecutionStatus::suspended;out.code=executable?StableId{}:StableId{"PoliticalResponsibilitySuspended"};return {true,std::move(out),{}};
}
}
