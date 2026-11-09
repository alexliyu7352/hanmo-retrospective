#include "execution_configurations.hpp"
#include "../local_economy_internal.hpp"
#include <map>
#include "resident/domains/organization/organization_office_history.hpp"
namespace hanmo::resident {
namespace {
using namespace local_economy_detail;
EconomicOperatingPermissionSource permission(const EconomicTakeoverExecutionPlan& plan){EconomicOperatingPermissionSource out;out.responsibility=plan.original.reference;out.registration_fact_id=plan.registration.registration_fact_id;if(plan.decision){out.approval_fact_id=plan.decision->takeover_fact_id;out.decision_fact_id=plan.decision->decision_fact_id;}return out;}
std::vector<EconomicResponsibilityRef> refs_for(const LocalEconomyCatalog& catalog,const ProductionConfigurationRecord& record){auto refs=economic_production_responsibilities(catalog,record.unit_id);refs.push_back({EconomicResponsibilityKind::pending_configuration,record.id,{}});return refs;}
bool labor_expired(const LocalEconomyState& state,const ProductionConfigurationRecord& record,Integer next_due){for(const auto& target:record.targets){const auto* allocation=state.labor_allocations.find(target.allocation_id);if(!allocation||allocation->through_tick<next_due)return true;}return false;}
}
Status activate_taken_production_configuration_in_candidate(CommonStore& common,LocalEconomyState& state,
    const LocalEconomyCatalog& catalog,EconomicTakeoverPhaseContext& phase,const StableId& production_receipt_id,
    LocalEconomyEffects& effects){
    const auto* receipt=state.production_receipts.find(production_receipt_id);if(!receipt)return common_failure("EconomicConfigurationProductionRequired",production_receipt_id);
    if(!state.production_management)return common_success();auto& management=*state.production_management;const auto* pending=management.pending_by_unit.find(receipt->unit_id);if(!pending)return common_success();
    const auto* record=management.configurations.find(*pending);const auto* unit=state.production_units.find(receipt->unit_id);const auto* recipe=unit?catalog.recipes.find(unit->recipe_id):nullptr;
    if(!record||!unit||!recipe||record->effective_tick>receipt->due_tick||record->targets.size()!=record->prior_workers.size()||record->targets.size()!=record->reservation_allocation_ids.size())return common_failure("InvalidTakenProductionConfiguration",*pending);
    EconomicConfigurationExecutionSource source;source.configuration_id=record->id;source.production_receipt_id=receipt->id;source.tick=receipt->tick;source.due_tick=receipt->due_tick;bool allowed=true;
    for(const auto& ref:refs_for(catalog,*record)){auto read=read_economic_takeover_execution(phase.read,ref);if(!read.ok)return {false,{},read.error};if(read.value){source.permissions.push_back(permission(*read.value));if(read.value->status!=EconomicTakeoverExecutionStatus::approved)allowed=false;}}
    if(source.permissions.empty())return activate_production_configuration(common,state,unit->id,receipt->due_tick,effects);
    // 单元不能覆盖独立设施暂停，两个批准也不能指向不同经营主体。
    auto operating=read_economic_production_execution(phase.read,unit->id);if(!operating.ok)return {false,{},operating.error};if(!operating.value||operating.value->status!=EconomicTakeoverExecutionStatus::approved)allowed=false;
    const bool expired=labor_expired(state,*record,unit->next_due_tick);source.applied=allowed&&!expired;source.code=source.applied?"applied":expired?"EconomicConfigurationLaborExpired":"EconomicConfigurationAwaitingExecution";
    auto id=common.next_id("production_configuration");if(!id.ok)return {false,{},id.error};source.id=id.value;
    auto fact=common.append_fact({id.value,source.applied?"production_configuration_applied":"production_configuration_deferred","local_economy"});if(!fact.ok)return fact;
    if(source.applied){
        for(std::size_t n=0;n<record->targets.size();++n){auto* allocation=state.labor_allocations.find(record->targets[n].allocation_id);if(!allocation||allocation->workers!=record->prior_workers[n])return common_failure("ProductionConfigurationLaborChanged",record->id);allocation->workers=record->targets[n].workers;const auto& reserve=record->reservation_allocation_ids[n];if(!reserve.empty()){auto* held=state.labor_allocations.find(reserve);if(!held||held->commitment_id!=record->id)return common_failure("InvalidProductionLaborReservation",reserve);held->workers=0;}}
        management.results.insert(id.value,{id.value,record->id,"applied",receipt->due_tick});management.pending_by_unit.erase(unit->id);
    }
    if(!phase.state.configurations.insert(id.value,std::move(source)))return common_failure("DuplicateEconomicConfigurationSource",id.value);effects.fact_ids.push_back(id.value);effects.receipt_ids.push_back(id.value);return common_success();
}
bool economic_configuration_has_retained_delay(const EconomicTakeoverExecutionState& state,const StableId& id){for(const auto& row:state.configurations.rows())if(row.second.configuration_id==id)return true;return false;}
Status validate_economic_taken_configuration_sources(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& execution){
    if(!x.economy.production_management)return execution.configurations.size()?common_failure("UnexpectedTakenProductionConfiguration"):common_success();
    const auto& management=*x.economy.production_management;std::map<std::pair<StableId,StableId>,const EconomicConfigurationExecutionSource*> by_cycle;
    for(const auto& row:execution.configurations.rows()){
        const auto& source=row.second;const auto* record=management.configurations.find(source.configuration_id);const auto* receipt=x.economy.production_receipts.find(source.production_receipt_id);const auto* fact=x.common.state().facts.find(row.first);const auto* unit=record?x.economy.production_units.find(record->unit_id):nullptr;const auto* recipe=unit?x.catalog.recipes.find(unit->recipe_id):nullptr;
        if(row.first!=source.id||!record||!receipt||!fact||!recipe||fact->domain!="local_economy"||fact->kind!=(source.applied?"production_configuration_applied":"production_configuration_deferred")||receipt->unit_id!=record->unit_id||source.tick!=receipt->tick||source.due_tick!=receipt->due_tick||source.due_tick<record->effective_tick||(source.due_tick-record->effective_tick)%recipe->period_ticks||source.permissions.empty()||!by_cycle.emplace(std::make_pair(record->id,receipt->id),&source).second)return common_failure("InvalidEconomicConfigurationExecution",row.first);
        const auto* result=management.results.find(row.first);if(source.applied?(!result||result->configuration_id!=record->id||result->kind!="applied"||result->tick!=source.due_tick):bool(result))return common_failure("EconomicConfigurationResultMismatch",row.first);
        if(source.applied?source.code!="applied":(source.code!="EconomicConfigurationAwaitingExecution"&&source.code!="EconomicConfigurationLaborExpired"))return common_failure("EconomicConfigurationExecutionCode",row.first);
        auto valid=validate_economic_operating_permissions(x,{row.first,source.permissions},refs_for(x.catalog,*record),source.tick,source.applied);if(!valid.ok)return valid;
        // 真实先产出后配置，不能靠后造的产出头给较早配置追认来源。
        bool production_seen=false,configuration_seen=false;for(const auto& header:x.common.state().facts.rows()){if(header.first==receipt->id)production_seen=true;if(header.first==source.id){configuration_seen=production_seen;break;}}if(!configuration_seen)return common_failure("EconomicConfigurationSourceOrder",row.first);
    }
    std::size_t consumed=0;
    for(const auto& row:management.configurations.rows()){
        const auto& record=row.second;const ProductionConfigurationResult* result=nullptr;for(const auto& resolution:management.results.rows())if(resolution.second.configuration_id==record.id)result=&resolution.second;
        for(const auto& cycle:x.economy.production_receipts.rows()){
            const auto& receipt=cycle.second;if(receipt.unit_id!=record.unit_id||receipt.due_tick<record.effective_tick||(result&&receipt.due_tick>result->tick)||(result&&result->kind!="applied"&&receipt.due_tick>=result->tick))continue;
            bool required=false;for(const auto& ref:refs_for(x.catalog,record)){auto converted=economic_responsibility_political_ref(ref);if(!converted.ok)return {false,{},converted.error};auto before=x.politics.political_responsibility_before_fact(converted.value,receipt.id,receipt.tick,x.common.state().identity);if(!before.ok)return {false,{},before.error};required|=bool(before.value);}
            auto found=by_cycle.find({record.id,receipt.id});if(required){if(found==by_cycle.end())return common_failure("MissingEconomicConfigurationExecution",record.id);++consumed;}else if(found!=by_cycle.end())return common_failure("UnexpectedEconomicConfigurationExecution",record.id);
        }
    }
    if(consumed!=execution.configurations.size())return common_failure("OrphanEconomicConfigurationExecution");return common_success();
}
namespace {
// 命令与后续经营使用同一逐责任边界；旧待配的撤销不由单元批准一并吞掉。
std::vector<EconomicResponsibilityRef> command_refs(const LocalEconomyCatalog& catalog,const StableId& unit,const std::optional<StableId>& prior){auto refs=economic_production_responsibilities(catalog,unit);if(prior)refs.push_back({EconomicResponsibilityKind::pending_configuration,*prior,{}});return refs;}
Outcome<std::vector<EconomicTakeoverExecutionPlan>> command_plans(const EconomicTakeoverReadContext& x,const StableId& unit,const std::optional<StableId>& prior){
    std::vector<EconomicTakeoverExecutionPlan> plans;std::optional<Owner> owner;
    for(const auto& ref:command_refs(x.catalog,unit,prior)){auto read=read_economic_takeover_execution(x,ref);if(!read.ok)return {false,{},read.error};if(!read.value)continue;const auto& plan=*read.value;if(plan.status!=EconomicTakeoverExecutionStatus::approved||!plan.executing_owner)return {false,{},common_failure("EconomicConfigurationAwaitingExecution",ref.id).error};if(owner&&!owner_equal(*owner,*plan.executing_owner))return {false,{},common_failure("EconomicConfigurationExecutionOwnerMismatch",ref.id).error};owner=plan.executing_owner;plans.push_back(std::move(*read.value));}
    if(!plans.empty()){auto operating=read_economic_production_execution(x,unit);if(!operating.ok)return {false,{},operating.error};if(!operating.value||operating.value->status!=EconomicTakeoverExecutionStatus::approved||!operating.value->executing_owner||!owner_equal(*owner,*operating.value->executing_owner))return {false,{},common_failure("EconomicConfigurationAwaitingExecution",unit).error};}
    return {true,std::move(plans),{}};
}
bool same_authority(const ActingAuthority& a,const ActingAuthority& b){return a.organization_id==b.organization_id&&a.office_id==b.office_id&&a.actor_id==b.actor_id&&a.authority_epoch==b.authority_epoch;}
// 原签发当刻的官职和同一Grant均须覆盖完整目标人数；当前在任不能追认旧请求。
Status historical_configuration_authority(const EconomicTakeoverReadContext& x,const EconomicConfigurationCommandSource& source,Integer tick,Integer amount){
    const auto* unit=x.economy.production_units.find(source.unit_id);const auto sequence=organization_instance_sequence(source.fact_id,"production_configuration");if(!unit||!sequence||!x.organizations||!x.organization_catalog||source.actor_id!=source.authority.actor_id)return common_failure("EconomicConfigurationAuthoritySourceRequired",source.fact_id);
    auto signer=validate_historical_office_signer(x.common,*x.organizations,*x.organization_catalog,source.authority,tick,*sequence);if(!signer.ok)return signer;
    const auto* office=x.organization_catalog->offices.find(source.authority.office_id);if(!office)return common_failure("EconomicConfigurationGrantMissing",source.fact_id);
    for(const auto& id:office->grant_ids){const auto* grant=x.organization_catalog->grants.find(id);if(!grant||grant->office_id!=office->id||grant->valid_from>tick||(grant->valid_until&&*grant->valid_until<tick)||grant->amount_limit<amount||std::find(grant->allowed_command_kinds.begin(),grant->allowed_command_kinds.end(),StableId{"configure_local_production"})==grant->allowed_command_kinds.end())continue;
        if(!std::any_of(grant->scopes.begin(),grant->scopes.end(),[&](const auto& scope){return scope.kind==ScopeRef::Kind::site&&scope.id==unit->site_id;}))continue;
        // 原Grant的全部资源必须由同一真实登记证明当时已归签发机构；今日Owner不补历史。
        auto history=x.politics.political_responsibility_before_fact({PoliticalResponsibilityKind::production_unit,unit->id,{},{}},source.fact_id,tick,x.common.state().identity);if(!history.ok)return {false,{},history.error};if(!history.value)return common_failure("EconomicConfigurationGrantResourceSourceRequired",source.fact_id);const auto& captured=history.value->registration;
        if((!grant->wallet_ids.empty()||!grant->inventory_ids.empty())&&captured.successor_organization_id!=source.authority.organization_id)return common_failure("EconomicConfigurationGrantResourceSourceRequired",source.fact_id);
        for(const auto& id:grant->wallet_ids)if(std::find(captured.public_wallet_ids.begin(),captured.public_wallet_ids.end(),id)==captured.public_wallet_ids.end())return common_failure("EconomicConfigurationGrantResourceSourceRequired",id);
        for(const auto& id:grant->inventory_ids)if(std::find(captured.public_inventory_ids.begin(),captured.public_inventory_ids.end(),id)==captured.public_inventory_ids.end())return common_failure("EconomicConfigurationGrantResourceSourceRequired",id);
        return common_success();}
    return common_failure("EconomicConfigurationGrantMissing",source.fact_id);
}
}
Outcome<std::optional<Owner>> economic_configuration_command_owner(const EconomicTakeoverReadContext& x,const StableId& unit,const StableId& previous){auto plans=command_plans(x,unit,previous.empty()?std::nullopt:std::optional<StableId>{previous});if(!plans.ok)return {false,{},plans.error};return {true,plans.value.empty()?std::nullopt:plans.value.front().executing_owner,{}};}
Status record_economic_configuration_command_sources(const EconomicTakeoverReadContext& x,EconomicTakeoverExecutionState& execution,const ConfigureLocalProductionCommand& command,const ProductionConfigurationQuote& quote,const LocalEconomyEffects& effects){
    const auto prior=quote.previous_configuration_id.empty()?std::nullopt:std::optional<StableId>{quote.previous_configuration_id};auto plans=command_plans(x,command.unit_id,prior);if(!plans.ok)return {false,{},plans.error};if(plans.value.empty())return common_success();if(!command.authority)return common_failure("EconomicConfigurationAuthoritySourceRequired",command.unit_id);
    std::optional<StableId> request;for(const auto& id:effects.fact_ids){const auto* fact=x.common.state().facts.find(id);if(fact&&fact->domain=="local_economy"&&fact->kind=="production_configuration_requested"){if(request)return common_failure("DuplicateEconomicConfigurationRequest",command.unit_id);request=id;}}
    if((command.kind==ProductionConfigurationKind::configure)!=bool(request))return common_failure("EconomicConfigurationRequestMissing",command.unit_id);
    for(const auto& id:effects.fact_ids){const auto* fact=x.common.state().facts.find(id);if(!fact||fact->domain!="local_economy"||(fact->kind!="production_configuration_requested"&&fact->kind!="production_configuration_cancelled"&&fact->kind!="production_configuration_superseded"))return common_failure("InvalidEconomicConfigurationCommandFact",id);
        EconomicConfigurationCommandSource source;source.fact_id=id;source.unit_id=command.unit_id;source.actor_id=command.actor_id;source.authority=*command.authority;source.prior_configuration_id=prior;source.request_configuration_id=request;for(const auto& plan:plans.value)source.permissions.push_back(permission(plan));if(!execution.configuration_requests.insert(id,std::move(source)))return common_failure("DuplicateEconomicConfigurationCommandSource",id);}
    return common_success();
}
Status validate_economic_configuration_command_sources(const EconomicTakeoverReadContext& x,const EconomicTakeoverExecutionState& execution){
    if(!x.economy.production_management)return execution.configuration_requests.empty()?common_success():common_failure("UnexpectedEconomicConfigurationRequest");const auto& management=*x.economy.production_management;std::set<StableId> covered;
    const auto validate=[&](const StableId& id,const StableId& unit,Integer tick,const ProductionConfigurationRecord* requested,const ProductionConfigurationResult* result)->Status{
        const auto* source=execution.configuration_requests.find(id);std::optional<StableId> prior;if(result)prior=result->configuration_id;else if(source)prior=source->prior_configuration_id;
        auto refs=command_refs(x.catalog,unit,prior);bool required=false;for(const auto& ref:refs){auto mapped=economic_responsibility_political_ref(ref);if(!mapped.ok)return {false,{},mapped.error};auto history=x.politics.political_responsibility_before_fact(mapped.value,id,tick,x.common.state().identity);if(!history.ok)return {false,{},history.error};required|=bool(history.value);}
        if(!required)return source?common_failure("UnexpectedEconomicConfigurationCommandSource",id):common_success();
        if(!source||source->fact_id!=id||source->unit_id!=unit||source->permissions.empty()||source->prior_configuration_id!=prior)return common_failure("MissingEconomicConfigurationCommandSource",id);
        const auto* request=source->request_configuration_id?management.configurations.find(*source->request_configuration_id):nullptr;
        if(requested?(request!=requested):((result->kind=="superseded")!=bool(request)))return common_failure("EconomicConfigurationCommandLinkMismatch",id);
        if(request&&(request->unit_id!=unit||request->requested_tick!=tick||request->actor_id!=source->actor_id||!request->authority||!same_authority(*request->authority,source->authority)))return common_failure("EconomicConfigurationCommandIssuerMismatch",id);
        if(request&&result){const auto* request_source=execution.configuration_requests.find(request->id);if(!request_source||request_source->prior_configuration_id!=prior||request_source->request_configuration_id!=source->request_configuration_id||request_source->actor_id!=source->actor_id||!same_authority(request_source->authority,source->authority))return common_failure("EconomicConfigurationCommandLinkMismatch",id);}
        if(prior){const auto* previous=management.configurations.find(*prior);const ProductionConfigurationResult* resolution=nullptr;for(const auto& row:management.results.rows())if(row.second.configuration_id==*prior)resolution=&row.second;if(!previous||previous->unit_id!=unit||!resolution||resolution->tick!=tick||resolution->kind!=(request?"superseded":"cancelled"))return common_failure("EconomicConfigurationPriorResultMismatch",id);
            const auto* linked=execution.configuration_requests.find(resolution->id);if(!linked||linked->request_configuration_id!=source->request_configuration_id||linked->prior_configuration_id!=prior||linked->actor_id!=source->actor_id||!same_authority(linked->authority,source->authority))return common_failure("EconomicConfigurationPriorResultMismatch",id);}
        Integer amount=0;if(request)for(const auto& target:request->targets)if(target.workers<0||!add(amount,target.workers,amount))return common_failure("InvalidProductionLaborAmount",id);auto authority=historical_configuration_authority(x,*source,tick,amount);if(!authority.ok)return authority;
        auto permissions=validate_economic_operating_permissions(x,{id,source->permissions},refs,tick,true);if(!permissions.ok)return permissions;
        // 历史单元执行主体必须正是本次官职组织，不能借设施批准给另一机构签配置。
        auto unit_history=x.politics.political_responsibility_before_fact({PoliticalResponsibilityKind::production_unit,unit,{},{}},id,tick,x.common.state().identity);if(!unit_history.ok)return {false,{},unit_history.error};if(!unit_history.value||!unit_history.value->decision||!unit_history.value->decision->execution_principal||unit_history.value->decision->execution_principal->kind!=SubjectKind::organization||unit_history.value->decision->execution_principal->id!=source->authority.organization_id)return common_failure("EconomicConfigurationCommandPrincipalMismatch",id);
        covered.insert(id);return common_success();
    };
    for(const auto& row:management.configurations.rows()){auto checked=validate(row.first,row.second.unit_id,row.second.requested_tick,&row.second,nullptr);if(!checked.ok)return checked;}
    for(const auto& row:management.results.rows())if(row.second.kind=="cancelled"||row.second.kind=="superseded"){const auto* original=management.configurations.find(row.second.configuration_id);if(!original)return common_failure("InvalidProductionConfigurationResult",row.first);auto checked=validate(row.first,original->unit_id,row.second.tick,nullptr,&row.second);if(!checked.ok)return checked;}
    return covered.size()==execution.configuration_requests.size()?common_success():common_failure("OrphanEconomicConfigurationCommandSource");
}

}
