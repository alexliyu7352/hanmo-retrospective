// 生产管理验证唯一物理容量、未来劳力占用及配置历史；不修补坏档或重新执行旧收成。
#include "production_management.hpp"
#include "economic_work_sources.hpp"
#include "local_economy_internal.hpp"
#include "takeover/execution_configurations.hpp"
#include <map>
namespace hanmo::resident {
using namespace local_economy_detail;
// 新旧能力显式分界；管理状态与收据上下文不能偷偷进入旧profile。
Status validate_production_management(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog){
    return validate_production_management(common,state,catalog,nullptr);
}
Status validate_production_management(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const EconomicTakeoverExecutionState* takeover){
    return validate_production_management(common,state,catalog,nullptr,takeover);
}
Status validate_production_management(const CommonStore& common,const LocalEconomyState& state,const LocalEconomyCatalog& catalog,const EconomicTakeoverReadContext* read,const EconomicTakeoverExecutionState* takeover,const EconomicWorkValidationContext* exact){
    if(read&&(!takeover||&read->common!=&common||&read->economy!=&state||&read->catalog!=&catalog))return failure("EconomicConfigurationContextMismatch");
    if(read){auto commands=validate_economic_configuration_command_sources(*read,*takeover);if(!commands.ok)return commands;}
    if(!catalog.production_management){if(state.production_management)return failure("UnexpectedProductionManagementState");for(const auto& row:state.production_receipts.rows())if(row.second.production_context)return failure("UnexpectedProductionContext",row.first);return common_success();}
    if(!catalog.enabled||catalog.production_management->version!=1||!state.production_management||state.production_management->version!=1)return failure("ProductionManagementVersionMismatch");
    const auto& definitions=*catalog.production_management;const auto& management=*state.production_management;const Integer now=std::max(common.tick(),state.last_step_tick);
    std::set<StableId> asset_sources;std::map<StableId,Integer> usage;
    for(const auto& row:definitions.assets.rows()){const auto& asset=row.second;if(row.first!=asset.id||asset.id.empty()||asset.site_id.empty()||asset.source_key.empty()||!asset_sources.insert(asset.source_key).second||asset.capacity_units<0||(asset.owner.kind!=Owner::Kind::actor&&asset.owner.kind!=Owner::Kind::organization))return failure("InvalidProductionAsset",row.first);}
    for(const auto& row:definitions.seasons.rows()){const auto& season=row.second;if(row.first!=season.id||season.id.empty()||season.origin_tick<0||season.phase_ticks<=0||season.factors_bp.empty()||season.factors_bp.size()>12)return failure("InvalidProductionSeason",row.first);for(auto factor:season.factors_bp)if(factor<0||factor>20000)return failure("InvalidProductionSeasonFactor",row.first);}
    for(const auto& row:definitions.plans.rows()){
        const auto& plan=row.second;const auto* unit=state.production_units.find(plan.unit_id);const auto* binding=unit?state.production_bindings.find(unit->id):nullptr;
        if(row.first!=plan.unit_id||!unit||!catalog.production_units.find(plan.unit_id)||!binding||binding->policy_owner!=ProvisionOwner::macro||plan.asset_uses.empty()||plan.asset_uses.size()>32||(!plan.season_profile_id.empty()&&!definitions.seasons.find(plan.season_profile_id)))return failure("InvalidProductionCapacityPlan",row.first);
        std::set<StableId> assets;for(const auto& use:plan.asset_uses){const auto* asset=definitions.assets.find(use.asset_id);Integer capacity=0;if(!asset||!assets.insert(use.asset_id).second||asset->site_id!=unit->site_id||!owner_equal(asset->owner,unit->owner)||use.units<=0||use.batches_per_unit<=0||!multiply(use.units,use.batches_per_unit,capacity)||!add(usage[use.asset_id],use.units,usage[use.asset_id])||usage[use.asset_id]>asset->capacity_units)return failure("ProductionAssetOverallocated",use.asset_id);}
        auto context=production_context(catalog,*unit,unit->next_due_tick);if(!context.ok)return {false,{},context.error};
    }
    std::map<StableId,std::size_t> fact_order;for(std::size_t n=0;n<common.state().facts.rows().size();++n)fact_order[common.state().facts.rows()[n].first]=n;
    std::map<StableId,const ProductionConfigurationResult*> resolutions;
    for(const auto& row:management.results.rows()){
        const auto& result=row.second;const auto* record=management.configurations.find(result.configuration_id);const auto* fact=common.state().facts.find(row.first);
        if(row.first!=result.id||!record||!fact||fact->domain!="local_economy"||fact->kind!="production_configuration_"+result.kind||(result.kind!="applied"&&result.kind!="cancelled"&&result.kind!="superseded")||result.tick<record->requested_tick||result.tick>now||!resolutions.emplace(record->id,&result).second||!fact_order.count(record->id)||fact_order[record->id]>=fact_order[result.id])return failure("InvalidProductionConfigurationResult",row.first);
        if((!takeover||!economic_configuration_has_retained_delay(*takeover,record->id))&&(result.kind=="applied"?result.tick!=record->effective_tick:result.tick>=record->effective_tick))return failure("InvalidProductionConfigurationResultTick",row.first);
    }
    for(const auto& row:management.configurations.rows()){
        const auto& record=row.second;const auto* unit=state.production_units.find(record.unit_id);const auto* recipe=unit?catalog.recipes.find(unit->recipe_id):nullptr;const auto* fact=common.state().facts.find(row.first);const auto* pending=management.pending_by_unit.find(record.unit_id);const bool active=pending&&*pending==record.id;Integer first_due=0;
        if(row.first!=record.id||!unit||!definitions.plans.find(record.unit_id)||!recipe||!common.actor(record.actor_id)||!fact||fact->domain!="local_economy"||fact->kind!="production_configuration_requested"||record.requested_tick<0||record.requested_tick>now||record.effective_tick<=record.requested_tick||!add(record.effective_tick,recipe->period_ticks,first_due)||record.first_due_tick!=first_due||record.targets.size()!=unit->labor_allocation_ids.size()||record.targets.size()>256||record.targets.size()!=record.prior_workers.size()||record.targets.size()!=record.reservation_allocation_ids.size()||active==bool(resolutions.count(record.id)))return failure("InvalidProductionConfiguration",row.first);
        if(unit->owner.kind==Owner::Kind::organization){if(!record.authority||(record.authority->organization_id!=unit->owner.id&&(!read||!takeover||!takeover->configuration_requests.find(record.id)))||record.authority->actor_id!=record.actor_id||record.authority->authority_epoch<0||record.authority->office_id.empty())return failure("InvalidProductionConfigurationAuthority",row.first);}else if(unit->owner.kind!=Owner::Kind::actor||unit->owner.id!=record.actor_id||record.authority)return failure("InvalidProductionConfigurationAuthority",row.first);
        if(active&&(!takeover||!economic_configuration_has_retained_delay(*takeover,record.id))&&(record.effective_tick!=unit->next_due_tick||record.effective_tick<=now))return failure("OverdueProductionConfiguration",row.first);
        std::set<StableId> targets,reservations;
        for(std::size_t i=0;i<record.targets.size();++i){const auto& target=record.targets[i];const auto* authored=catalog.labor_allocations.find(target.allocation_id);const auto* allocation=state.labor_allocations.find(target.allocation_id);const auto& reservation_id=record.reservation_allocation_ids[i];const auto increase=std::max(Integer(0),target.workers-record.prior_workers[i]);
            if(!targets.insert(target.allocation_id).second||!has(unit->labor_allocation_ids,target.allocation_id)||!authored||!allocation||target.workers<0||target.workers>authored->workers||record.prior_workers[i]<0||record.prior_workers[i]>authored->workers||allocation->cohort_id!=authored->cohort_id||allocation->activity_or_production_id!=unit->id)return failure("InvalidProductionLaborTarget",target.allocation_id);
            if(active&&(allocation->workers!=record.prior_workers[i]||allocation->through_tick<record.first_due_tick))return failure("ProductionConfigurationLaborChanged",target.allocation_id);
            if(increase==0){if(!reservation_id.empty())return failure("UnexpectedProductionLaborReservation",reservation_id);continue;}
            const auto* reservation=state.labor_allocations.find(reservation_id);if(reservation_id.empty()||!reservations.insert(reservation_id).second||!reservation||reservation->commitment_id!=record.id||reservation->activity_or_production_id!=unit->id||reservation->cohort_id!=allocation->cohort_id||reservation->from_tick!=record.effective_tick||reservation->workers!=(active?increase:0)||(active&&reservation->through_tick!=allocation->through_tick))return failure("InvalidProductionLaborReservation",reservation_id);
        }
    }
    for(const auto& row:management.pending_by_unit.rows()){const auto* record=management.configurations.find(row.second);if(!record||record->unit_id!=row.first||resolutions.count(record->id))return failure("InvalidPendingProductionConfiguration",row.first);}
    for(const auto& row:state.labor_allocations.rows())if(management.configurations.find(row.second.commitment_id)){const auto* record=management.configurations.find(row.second.commitment_id);if(!has(record->reservation_allocation_ids,row.first))return failure("UnregisteredProductionLaborReservation",row.first);}
    for(const auto& row:state.production_receipts.rows()){
        const auto& receipt=row.second;const auto* plan=definitions.plans.find(receipt.unit_id);if(!plan){if(receipt.production_context)return failure("UnexpectedProductionContext",row.first);continue;}
        const auto* unit=state.production_units.find(receipt.unit_id);if(!unit||!receipt.production_context)return failure("MissingProductionContext",row.first);auto expected=production_context(catalog,*unit,receipt.due_tick);if(!expected.ok)return {false,{},expected.error};const auto& actual=*receipt.production_context;auto value=expected.value;if(exact){auto actual_capacity=read_original_production_receipt_capacity(*exact,receipt);if(!actual_capacity.ok)return {false,{},actual_capacity.error};if(actual_capacity.value)value.capacity=*actual_capacity.value;}
        if(actual.capacity!=value.capacity||actual.region_factor_bp!=value.region_factor_bp||actual.season_factor_bp!=value.season_factor_bp||actual.season_phase!=value.season_phase||actual.season_profile_id!=value.season_profile_id||actual.asset_ids!=value.asset_ids||receipt.batches>actual.capacity)return failure("InvalidProductionContext",row.first);
    }
    return common_success();
}
}
