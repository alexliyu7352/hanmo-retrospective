// 复用S6逐发生事实历史读，旧配给/生产回执的私有资产来源不由今天Owner追认。
#include "execution_materials.hpp"
#include "../relief/public_relief_rules.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
auto key(const EconomicResponsibilityRef& r){return std::make_tuple(r.kind,r.id,r.unit_id);}
bool same_identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
EconomicOperatingPermissionSource source(const EconomicTakeoverExecutionPlan& p){EconomicOperatingPermissionSource out;out.responsibility=p.original.reference;out.registration_fact_id=p.registration.registration_fact_id;if(p.decision){out.approval_fact_id=p.decision->takeover_fact_id;out.decision_fact_id=p.decision->decision_fact_id;}return out;}
Outcome<std::optional<PoliticalResponsibilityExecution>> historical(const EconomicTakeoverReadContext& x,const EconomicResponsibilityRef& r,const StableId& fact,Integer tick){auto ref=economic_responsibility_political_ref(r);if(!ref.ok)return {false,{},ref.error};return x.politics.political_responsibility_before_fact(ref.value,fact,tick,x.common.state().identity);}
bool matches(const EconomicOperatingPermissionSource& link,const PoliticalResponsibilityExecution& actual){return link.registration_fact_id==actual.registration_fact_id&&bool(link.approval_fact_id)==bool(actual.decision)&&bool(link.decision_fact_id)==bool(actual.decision)&&(!actual.decision||(link.approval_fact_id==std::optional<StableId>{actual.decision->takeover_fact_id}&&link.decision_fact_id==std::optional<StableId>{actual.decision->decision_fact_id}));}
Status validate_permissions(const EconomicTakeoverReadContext& x,const EconomicOperatingReceiptSource& record,
    const std::vector<EconomicResponsibilityRef>& refs,Integer tick,bool performed){
    std::set<decltype(key(EconomicResponsibilityRef{}))> covered;
    for(const auto& r:refs){auto actual=historical(x,r,record.receipt_id,tick);if(!actual.ok)return {false,{},actual.error};
        const EconomicOperatingPermissionSource* link=nullptr;for(const auto& p:record.permissions)if(key(p.responsibility)==key(r)){if(link)return common_failure("DuplicateEconomicOperatingPermission",record.receipt_id);link=&p;}
        if(!actual.value){if(link)return common_failure("UnexpectedEconomicOperatingPermission",record.receipt_id);continue;}
        if(!link||!matches(*link,*actual.value))return common_failure("MissingEconomicOperatingPermission",record.receipt_id);
        if(performed&&actual.value->status!=PoliticalResponsibilityExecutionStatus::approved)return common_failure("EconomicMaterialExecutedWithoutApproval",record.receipt_id);
        covered.insert(key(r));
    }
    if(covered.size()!=record.permissions.size())return common_failure("EconomicOperatingPermissionOutsideReceipt",record.receipt_id);return common_success();
}
std::vector<EconomicResponsibilityRef> production_refs(const LocalEconomyCatalog& catalog,const StableId& unit){std::vector<EconomicResponsibilityRef> refs{{EconomicResponsibilityKind::production_unit,unit,{}}};if(catalog.production_management)if(const auto* plan=catalog.production_management->plans.find(unit))for(const auto& use:plan->asset_uses)refs.push_back({EconomicResponsibilityKind::production_asset_use,use.asset_id,unit});return refs;}
}
std::vector<EconomicResponsibilityRef> economic_production_responsibilities(const LocalEconomyCatalog& c,const StableId& unit){return production_refs(c,unit);}
Status validate_economic_operating_permissions(const EconomicTakeoverReadContext& x,const EconomicOperatingReceiptSource& record,const std::vector<EconomicResponsibilityRef>& refs,Integer tick,bool performed){return validate_permissions(x,record,refs,tick,performed);}
Status record_economic_taken_ration_source(const EconomicTakeoverReadContext& x,EconomicTakeoverExecutionState& state,const EconomicTakeoverExecutionPlan& plan,const StableId& id){
    const auto* receipt=x.economy.rations.find(id);const auto* obligation=receipt?x.economy.obligations.find(receipt->obligation_id):nullptr;const auto* fact=x.common.state().facts.find(id);
    if(state.version!=1||!receipt||!obligation||!fact||fact->domain!="local_economy"||fact->kind!="local_ration"||plan.original.reference.kind!=EconomicResponsibilityKind::obligation||plan.original.reference.id!=receipt->obligation_id||!same_identity(plan.original.source_identity,x.common.state().identity)||receipt->inventory_id!=obligation->source_wallet_or_inventory||receipt->beneficiary.kind!=obligation->beneficiary.kind||receipt->beneficiary.id!=obligation->beneficiary.id||receipt->item_id!=obligation->item_id||receipt->consumed<0||receipt->consumed>receipt->required)return common_failure("InvalidEconomicTakenRationSource",id);
    if(plan.status!=EconomicTakeoverExecutionStatus::approved&&receipt->consumed!=0)return common_failure("EconomicRationExecutedWithoutApproval",id);
    if(!state.rations.insert(id,{id,{source(plan)}}))return common_failure("DuplicateEconomicTakenRationSource",id);return common_success();
}
Status record_economic_taken_production_source(const EconomicTakeoverReadContext& x,EconomicTakeoverExecutionState& state,const std::vector<EconomicTakeoverExecutionPlan>& plans,const StableId& id){
    const auto* receipt=x.economy.production_receipts.find(id);const auto* fact=x.common.state().facts.find(id);
    if(state.version!=1||!receipt||!fact||fact->domain!="local_economy"||fact->kind!="local_production"||plans.empty())return common_failure("InvalidEconomicTakenProductionSource",id);
    EconomicOperatingReceiptSource record;record.receipt_id=id;std::set<decltype(key(EconomicResponsibilityRef{}))> seen;
    const auto refs=production_refs(x.catalog,receipt->unit_id);
    for(const auto& plan:plans){const auto& ref=plan.original.reference;if(!same_identity(plan.original.source_identity,x.common.state().identity)||!seen.insert(key(ref)).second||std::none_of(refs.begin(),refs.end(),[&](const auto& expected){return key(expected)==key(ref);}))return common_failure("EconomicProductionPermissionMismatch",id);
        if(plan.status!=EconomicTakeoverExecutionStatus::approved&&(receipt->batches!=0||!receipt->inputs.empty()||!receipt->outputs.empty()))return common_failure("EconomicProductionExecutedWithoutApproval",id);record.permissions.push_back(source(plan));}
    if(!state.production.insert(id,std::move(record)))return common_failure("DuplicateEconomicTakenProductionSource",id);return common_success();
}
// 来源记录只接原分发fact与原contract，不复制发放数额或生成第二个完成事实。
Status record_economic_taken_relief_source(const EconomicTakeoverReadContext& x,EconomicTakeoverExecutionState& state,const EconomicTakeoverExecutionPlan& plan,const StableId& id){
    const auto* receipt=x.relief?x.relief->receipts.find(id):nullptr;const auto* fact=x.common.state().facts.find(id);
    if(state.version!=1||!receipt||receipt->id!=id||!fact||fact->kind!="public_relief_distributed"||fact->domain!="local_economy"||plan.original.reference.kind!=EconomicResponsibilityKind::relief_contract||plan.original.reference.id!=receipt->contract_id||plan.original.task_id!=receipt->task_id||!same_identity(plan.original.source_identity,x.common.state().identity)||plan.status!=EconomicTakeoverExecutionStatus::approved||!plan.decision||!plan.executing_owner)return common_failure("InvalidEconomicTakenReliefSource",id);
    if(!state.relief.insert(id,{id,{source(plan)}}))return common_failure("DuplicateEconomicTakenReliefSource",id);return common_success();
}
// 删除整个关联也必须拒绝；历史政治reader决定分发之前究竟已登记和批准哪一责任。
Status validate_economic_taken_relief_sources(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& state){
    if(!x.relief)return state.relief.empty()?common_success():common_failure("EconomicReliefSourceUnavailable");std::size_t covered=0;
    for(const auto& row:x.relief->receipts.rows()){const auto& receipt=row.second;const EconomicResponsibilityRef ref{EconomicResponsibilityKind::relief_contract,receipt.contract_id,{}};auto actual=historical(x,ref,row.first,receipt.tick);if(!actual.ok)return {false,{},actual.error};const auto* link=state.relief.find(row.first);
        if(!actual.value){if(link)return common_failure("UnexpectedEconomicTakenReliefSource",row.first);continue;}
        if(!link||link->receipt_id!=row.first)return common_failure("MissingEconomicTakenReliefSource",row.first);auto valid=validate_permissions(x,*link,{ref},receipt.tick,true);if(!valid.ok)return valid;++covered;
    }
    return covered==state.relief.size()?common_success():common_failure("OrphanEconomicTakenReliefSource");
}
Status validate_economic_taken_material_sources(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& state){
    std::size_t rations=0,production=0;
    for(const auto& row:x.economy.rations.rows()){const auto& receipt=row.second;const auto* record=state.rations.find(row.first);const EconomicResponsibilityRef ref{EconomicResponsibilityKind::obligation,receipt.obligation_id,{}};auto actual=historical(x,ref,row.first,receipt.tick);if(!actual.ok)return {false,{},actual.error};
        if(!actual.value){if(record)return common_failure("UnexpectedEconomicTakenRationSource",row.first);continue;}
        if(!record||record->receipt_id!=row.first)return common_failure("MissingEconomicTakenRationSource",row.first);auto valid=validate_permissions(x,*record,{ref},receipt.tick,receipt.consumed>0);if(!valid.ok)return valid;++rations;
    }
    for(const auto& row:x.economy.production_receipts.rows()){const auto& receipt=row.second;const auto* record=state.production.find(row.first);const auto refs=production_refs(x.catalog,receipt.unit_id);bool required=false;
        for(const auto& ref:refs){auto actual=historical(x,ref,row.first,receipt.tick);if(!actual.ok)return {false,{},actual.error};required|=bool(actual.value);}
        if(!required){if(record)return common_failure("UnexpectedEconomicTakenProductionSource",row.first);continue;}
        if(!record||record->receipt_id!=row.first)return common_failure("MissingEconomicTakenProductionSource",row.first);auto valid=validate_permissions(x,*record,refs,receipt.tick,receipt.batches>0||!receipt.inputs.empty()||!receipt.outputs.empty());if(!valid.ok)return valid;++production;
    }
    if(rations!=state.rations.size()||production!=state.production.size())return common_failure("OrphanEconomicOperatingSource");return common_success();
}
}
