// 历史责任与现行执行权分层；所有读取失败原样返回，不将坏来源吞成暂停。
#include "responsibility_execution.hpp"
#include "../fiscal/occupation_fiscal_responsibility.hpp"
#include <set>
#include <tuple>
namespace hanmo::resident {
namespace {
using Plan=Outcome<std::optional<EconomicTakeoverExecutionPlan>>;
Plan fail(const StableId& code,const StableId& id={}){return {false,{},common_failure(code,id).error};}
bool identity(const WorldIdentity& a,const WorldIdentity& b){return a.world_id==b.world_id&&a.execution_epoch==b.execution_epoch&&a.revision==b.revision&&a.tick==b.tick;}
bool owner(const Owner& a,const Owner& b){return a.kind==b.kind&&a.id==b.id;}
bool subject(const SubjectRef& a,const SubjectRef& b){return a.kind==b.kind&&a.id==b.id;}
bool subject(const std::optional<SubjectRef>& a,const std::optional<SubjectRef>& b){return bool(a)==bool(b)&&(!a||subject(*a,*b));}
bool subjects(const std::vector<SubjectRef>& a,const std::vector<SubjectRef>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(!subject(a[i],b[i]))return false;return true;}
bool contains(const std::vector<StableId>& ids,const StableId& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();}
auto key(const PoliticalResponsibilityRef& r){return std::make_tuple(r.kind,r.responsibility_id,r.unit_id,r.asset_id);}
Outcome<PoliticalResponsibilityRef> political_ref(const EconomicResponsibilityRef& r){
    PoliticalResponsibilityRef out;out.responsibility_id=r.id;
    if(r.id.empty()||(r.kind==EconomicResponsibilityKind::production_asset_use?r.unit_id.empty():!r.unit_id.empty()))return {false,{},common_failure("InvalidEconomicResponsibilityReference",r.id).error};
    switch(r.kind){
    case EconomicResponsibilityKind::production_unit:out.kind=PoliticalResponsibilityKind::production_unit;break;
    case EconomicResponsibilityKind::production_asset_use:out.kind=PoliticalResponsibilityKind::production_asset_use;out.unit_id=r.unit_id;out.asset_id=r.id;break;
    case EconomicResponsibilityKind::pending_configuration:out.kind=PoliticalResponsibilityKind::pending_configuration;break;
    case EconomicResponsibilityKind::obligation:out.kind=PoliticalResponsibilityKind::obligation;break;
    case EconomicResponsibilityKind::arrear:out.kind=PoliticalResponsibilityKind::arrear;break;
    case EconomicResponsibilityKind::fiscal_assessment:out.kind=PoliticalResponsibilityKind::fiscal_assessment;break;
    case EconomicResponsibilityKind::relief_contract:out.kind=PoliticalResponsibilityKind::relief_contract;break;
    case EconomicResponsibilityKind::supply_source:out.kind=PoliticalResponsibilityKind::supply_source;break;
    default:return {false,{},common_failure("UnsupportedEconomicResponsibilityKind",r.id).error};
    }return {true,std::move(out),{}};
}
const Owner* current_owner(const CommonStore& c,const StableId& id,bool inventory){if(inventory){const auto* v=c.inventory(id);return v?&v->owner:nullptr;}const auto* v=c.wallet(id);return v?&v->owner:nullptr;}
bool selected(const PoliticalTakeoverRegistrationSource& r,const EconomicResponsibilityContainer& c){return contains(c.inventory?r.public_inventory_ids:r.public_wallet_ids,c.id);}
bool fact(const CommonStore& c,const StableId& id,const StableId& kind,const StableId& domain){const auto* f=c.state().facts.find(id);return f&&f->id==id&&f->kind==kind&&f->domain==domain;}
bool same_registration(const PoliticalTakeoverRegistrationSource& r,const PoliticalTakeoverSource& a){return r.occupation_fact_id==a.occupation_fact_id&&r.jurisdiction_id==a.jurisdiction_id&&r.site_id==a.site_id&&r.former_organization_id==a.former_organization_id&&r.successor_organization_id==a.successor_organization_id&&r.occupation_tick==a.occupation_tick&&r.public_wallet_ids==a.public_wallet_ids&&r.public_inventory_ids==a.public_inventory_ids&&r.occupation_rule.rule_id==a.occupation_rule.rule_id&&r.occupation_rule.rule_version==a.occupation_rule.rule_version&&r.occupation_rule.integration_ticks==a.occupation_rule.integration_ticks&&r.occupation_rule.income_basis_points==a.occupation_rule.income_basis_points;}
bool same_decision(const PoliticalResponsibilityDecision& a,const PoliticalResponsibilityDecision& b){
    if(a.decision_fact_id!=b.decision_fact_id||a.takeover_fact_id!=b.takeover_fact_id||key(a.responsibility)!=key(b.responsibility)||a.disposition!=b.disposition||!subject(a.original_principal,b.original_principal)||!subjects(a.original_beneficiaries,b.original_beneficiaries)||!subject(a.execution_principal,b.execution_principal)||!subjects(a.collection_beneficiaries,b.collection_beneficiaries)||a.payment_wallet_id!=b.payment_wallet_id||a.payment_inventory_id!=b.payment_inventory_id||a.receipt_wallet_id!=b.receipt_wallet_id||a.receipt_inventory_id!=b.receipt_inventory_id||a.production_asset_ids!=b.production_asset_ids||a.original_assets.size()!=b.original_assets.size())return false;
    for(std::size_t i=0;i<a.original_assets.size();++i){const auto& x=a.original_assets[i];const auto& y=b.original_assets[i];if(x.kind!=y.kind||x.id!=y.id||!owner(x.frozen_owner,y.frozen_owner))return false;}return true;
}
}
Outcome<PoliticalResponsibilityRef> economic_responsibility_political_ref(const EconomicResponsibilityRef& ref){return political_ref(ref);}
Plan read_economic_takeover_execution(const EconomicTakeoverReadContext& x,const EconomicResponsibilityRef& ref){
    if(!x.catalog.enabled)return fail("EconomicResponsibilityDisabled",ref.id);
    Integer evaluation_tick=x.common.tick();if(x.phase){if(evaluation_tick<0||evaluation_tick==INT64_MAX||x.phase->from_tick!=evaluation_tick||x.phase->to_tick!=evaluation_tick+1)return fail("EconomicTakeoverPhaseBoundaryMismatch",ref.id);evaluation_tick=x.phase->to_tick;}
    auto mapped=political_ref(ref);if(!mapped.ok)return {false,{},mapped.error};
    auto current=x.politics.find_political_responsibility_for_execution(mapped.value,x.common.state().identity);if(!current.ok)return {false,{},current.error};if(!current.value)return {true,std::nullopt,{}};
    const auto& execution=*current.value;const auto& r=execution.registration;
    if(!identity(r.source_identity,x.common.state().identity)||key(execution.responsibility)!=key(mapped.value)||execution.registration_fact_id!=r.registration_fact_id||r.registration_fact_id.empty()||r.occupation_fact_id.empty()||r.jurisdiction_id.empty()||r.site_id.empty()||r.former_organization_id.empty()||r.successor_organization_id.empty()||r.former_organization_id==r.successor_organization_id||r.occupation_tick<0||r.occupation_tick>evaluation_tick||!fact(x.common,r.registration_fact_id,"political_public_responsibilities_pending","politics"))return fail("InvalidEconomicTakeoverRegistration",ref.id);
    // 军事清单/实际applied已由真实政治reader验证；本域另核每个容器及无重复。
    std::set<StableId> seen;for(const auto& id:r.public_wallet_ids)if(!x.common.wallet(id)||!seen.insert(id).second)return fail("InvalidEconomicTakeoverWallet",id);
    seen.clear();for(const auto& id:r.public_inventory_ids)if(!x.common.inventory(id)||!seen.insert(id).second)return fail("InvalidEconomicTakeoverInventory",id);
    EconomicPublicAccountSelection selection{{Owner::Kind::organization,r.former_organization_id},r.public_wallet_ids,r.public_inventory_ids};
    auto original=(ref.kind==EconomicResponsibilityKind::fiscal_assessment&&x.occupation_fiscal&&x.occupation_fiscal->assessments.find(ref.id)&&x.fiscal_catalog)?read_occupation_fiscal_responsibility_source(x.common,x.economy,*x.fiscal_catalog,*x.occupation_fiscal,selection,ref,x.phase):read_economic_retained_responsibility_source(x.common,x.economy,x.catalog,x.fiscal,x.fiscal_catalog,x.relief,selection,ref);if(!original.ok)return {false,{},original.error};
    EconomicTakeoverExecutionPlan out;out.original=std::move(original.value);out.registration=r;out.containers=out.original.containers;out.code=execution.code;
    if(execution.status==PoliticalResponsibilityExecutionStatus::pending){if(execution.decision||execution.code.empty())return fail("InvalidEconomicPendingTakeover",ref.id);return {true,std::move(out),{}};}
    if(execution.status!=PoliticalResponsibilityExecutionStatus::approved&&execution.status!=PoliticalResponsibilityExecutionStatus::suspended)return fail("UnknownEconomicTakeoverStatus",ref.id);
    if(!execution.decision)return fail("MissingEconomicTakeoverDecision",ref.id);
    const auto& d=*execution.decision;if(!d.execution_accounts.empty())return fail("EconomicExecutionAccountOverrideUnsupported",ref.id);
    if(key(d.responsibility)!=key(mapped.value)||!subject(d.original_principal,out.original.original_principal)||!subjects(d.original_beneficiaries,out.original.original_beneficiaries)||!subjects(d.collection_beneficiaries,out.original.original_beneficiaries)||d.original_assignee||!d.original_delivery_beneficiaries.empty()||d.receipt_wallet_id||d.receipt_inventory_id||!fact(x.common,d.decision_fact_id,"political_public_responsibility_decided","politics"))return fail("EconomicTakeoverChangedOriginalResponsibility",ref.id);
    auto approval=x.politics.political_takeover_source(d.takeover_fact_id);if(!approval.ok)return {false,{},approval.error};
    if(!same_registration(r,approval.value.source)||approval.value.source.fact_id!=d.takeover_fact_id||!fact(x.common,d.takeover_fact_id,political_takeover_fact_kind,"politics"))return fail("EconomicTakeoverApprovalMismatch",ref.id);
    const PoliticalResponsibilityDecision* approved=nullptr;for(const auto& value:approval.value.decisions)if(key(value.responsibility)==key(mapped.value)){if(approved)return fail("DuplicateEconomicTakeoverDecision",ref.id);approved=&value;}
    if(!approved||!same_decision(d,*approved))return fail("EconomicTakeoverDecisionMismatch",ref.id);
    out.decision=d;out.status=EconomicTakeoverExecutionStatus::suspended;
    if(execution.status==PoliticalResponsibilityExecutionStatus::suspended){if(out.code.empty())return fail("MissingEconomicSuspensionReason",ref.id);return {true,std::move(out),{}};}
    if(d.disposition!=PoliticalResponsibilityDisposition::continue_under_original&&d.disposition!=PoliticalResponsibilityDisposition::assume_by_successor)return fail("EconomicTakeoverNotExecutable",ref.id);
    const bool assume=d.disposition==PoliticalResponsibilityDisposition::assume_by_successor;
    if(assume&&ref.kind==EconomicResponsibilityKind::fiscal_assessment)return fail("EconomicFiscalReceivableAssignmentRequired",ref.id);
    if(assume){if(!d.execution_principal||d.execution_principal->kind!=SubjectKind::organization||d.execution_principal->id!=r.successor_organization_id)return fail("InvalidEconomicSuccessorPrincipal",ref.id);out.executing_owner=Owner{Owner::Kind::organization,r.successor_organization_id};}
    else{if(!subject(d.execution_principal,out.original.original_principal)||d.payment_wallet_id||d.payment_inventory_id||!d.production_asset_ids.empty())return fail("InvalidEconomicOriginalExecution",ref.id);if(out.original.original_operating_owner)out.executing_owner=out.original.original_operating_owner;else if(out.original.original_principal&&out.original.original_principal->kind==SubjectKind::organization)out.executing_owner=Owner{Owner::Kind::organization,out.original.original_principal->id};}
    if(!assume&&!out.executing_owner&&ref.kind==EconomicResponsibilityKind::supply_source){const auto* source=x.catalog.supply_sources.find(ref.id);if(source)out.executing_owner=Owner{Owner::Kind::organization,source->organization_id};}
    std::vector<StableId> asset_ids;for(const auto& a:out.original.production_assets)asset_ids.push_back(a.id);std::sort(asset_ids.begin(),asset_ids.end());auto granted=d.production_asset_ids;std::sort(granted.begin(),granted.end());
    if(assume&&(asset_ids!=granted||std::adjacent_find(granted.begin(),granted.end())!=granted.end()))return fail("EconomicProductionUseCoverageMismatch",ref.id);
    const auto payment=[&](const std::optional<StableId>& requested,bool inventory){if(!requested)return true;return std::any_of(out.containers.begin(),out.containers.end(),[&](const auto& c){return c.id==*requested&&c.inventory==inventory&&c.role==EconomicContainerRole::payment&&selected(r,c);});};
    if(!payment(d.payment_wallet_id,false)||!payment(d.payment_inventory_id,true))return fail("EconomicTakeoverPaymentOutsideResponsibility",ref.id);
    for(const auto& c:out.containers){
        if(c.role==EconomicContainerRole::receipt)continue;
        const auto* actual=current_owner(x.common,c.id,c.inventory);if(!actual)return fail("UnknownEconomicExecutionContainer",c.id);
        // 接管了一间库房并不授权使用未接管的另一机构库房；明确暂停整个责任。
        if(!out.executing_owner||!owner(*actual,*out.executing_owner)){out.code="EconomicExecutionContainerUnavailable";return {true,std::move(out),{}};}
        if(assume&&c.role==EconomicContainerRole::payment&&selected(r,c)&&((c.inventory?d.payment_inventory_id:d.payment_wallet_id)!=std::optional<StableId>{c.id}))return fail("MissingEconomicTakeoverPaymentAccount",ref.id);
    }
    out.status=EconomicTakeoverExecutionStatus::approved;out.code.clear();return {true,std::move(out),{}};
}
Status validate_economic_taken_container_history(const EconomicTakeoverReadContext& x,const EconomicResponsibilityRef& ref,const StableId& id,bool inventory,const Owner& expected){
    const auto* actual=current_owner(x.common,id,inventory);if(!actual)return common_failure("UnknownEconomicHistoricalContainer",id);
    auto plan=read_economic_takeover_execution(x,ref);if(!plan.ok)return {false,{},plan.error};if(!plan.value)return common_failure("MissingEconomicContainerTakeover",id);
    const auto& p=*plan.value;const auto& source=p.original;
    const auto container=std::find_if(source.containers.begin(),source.containers.end(),[&](const auto& c){return c.id==id&&c.inventory==inventory;});
    if(container==source.containers.end()||!selected(p.registration,*container)||expected.kind!=Owner::Kind::organization)return common_failure("EconomicContainerOutsideTakeover",id);
    bool original=false;if(source.original_operating_owner)original=owner(expected,*source.original_operating_owner);
    if(container->role==EconomicContainerRole::payment&&source.original_principal)original|=source.original_principal->kind==SubjectKind::organization&&source.original_principal->id==expected.id;
    if(container->role==EconomicContainerRole::receipt)for(const auto& beneficiary:source.original_beneficiaries)original|=beneficiary.kind==SubjectKind::organization&&beneficiary.id==expected.id;
    if(ref.kind==EconomicResponsibilityKind::supply_source){const auto* s=x.catalog.supply_sources.find(ref.id);original|=s&&s->organization_id==expected.id;}
    if(!original||actual->kind!=Owner::Kind::organization||actual->id!=p.registration.successor_organization_id)return common_failure("EconomicHistoricalOwnerSourceMismatch",id);
    return common_success();
}
Plan read_economic_production_execution(const EconomicTakeoverReadContext& x,const StableId& unit){
    auto result=read_economic_takeover_execution(x,{EconomicResponsibilityKind::production_unit,unit,{}});if(!result.ok||!result.value||result.value->status!=EconomicTakeoverExecutionStatus::approved)return result;
    for(const auto& asset:result.value->original.production_assets){auto permission=read_economic_takeover_execution(x,{EconomicResponsibilityKind::production_asset_use,asset.id,unit});if(!permission.ok)return permission;
        if(!permission.value||permission.value->status!=EconomicTakeoverExecutionStatus::approved||!permission.value->executing_owner||!result.value->executing_owner||!owner(*permission.value->executing_owner,*result.value->executing_owner)){result.value->status=EconomicTakeoverExecutionStatus::suspended;result.value->code="EconomicProductionAssetUseUnavailable";return result;}}
    return result;
}
// 两个入口共用逐责任真实来源核验；此实现不写观察、Need、ID或任何钱货。
static Outcome<std::vector<EconomicSupplySuspension>> supply_suspensions(const EconomicTakeoverReadContext& x){
    std::vector<EconomicSupplySuspension> paused;
    for(const auto& row:x.catalog.supply_sources.rows()){const auto& source=row.second;const auto* inventory=x.common.inventory(source.inventory_id);if(!inventory)return {false,{},common_failure("UnknownInventory",source.inventory_id).error};auto execution=read_economic_takeover_execution(x,{EconomicResponsibilityKind::supply_source,row.first,{}});if(!execution.ok)return {false,{},execution.error};const Owner original{Owner::Kind::organization,source.organization_id,{}};
        if(!execution.value){if(!owner_equal(inventory->owner,original))return {false,{},common_failure("UnprovenForeignSupplySource",row.first).error};continue;}const auto& plan=*execution.value;
        if(plan.status==EconomicTakeoverExecutionStatus::approved&&plan.executing_owner&&owner_equal(*plan.executing_owner,original)&&owner_equal(inventory->owner,original))continue;
        paused.push_back({source.source_key,source.organization_id,source.site_id,source.inventory_id,source.item_id,plan.registration.registration_fact_id,plan.status==EconomicTakeoverExecutionStatus::approved?StableId{"EconomicSupplySuccessorPolicyRequired"}:plan.code});
    }return {true,std::move(paused),{}};
}
// 常规库存观察和冷验证保持已commit边界，不能通过带phase的context偷看未来库存。
Outcome<std::vector<EconomicSupplySuspension>> read_economic_supply_suspensions(const EconomicTakeoverReadContext& x){
    if(x.phase)return {false,{},common_failure("EconomicSupplyBoundaryRequiresCommittedClock").error};return supply_suspensions(x);
}
// 显式军事阶段只允许同一候选的原from/to；未来占领仍由逐责任reader再核。
Outcome<std::vector<EconomicSupplySuspension>> read_economic_supply_suspensions(const EconomicTakeoverReadContext& x,const DynamicStepInput& phase){
    const auto tick=x.common.tick();if(x.phase!=&phase||tick<0||tick==INT64_MAX||phase.from_tick!=tick||phase.to_tick!=tick+1)return {false,{},common_failure("EconomicTakeoverPhaseBoundaryMismatch").error};return supply_suspensions(x);
}

}
