// 民心跟真实评税周期而不是政策按钮、分户assessment或后续收款；外层候选负责原子回滚。
#include "political_fiscal_civil_integration.hpp"
#include "resident/domains/politics/politics_governance_sources.hpp"
#include "world_bundle.hpp"
#include "occupation_fiscal_integration.hpp"
#include <set>
namespace hanmo::resident {
Status settle_political_fiscal_civil_in_transaction(CommonStore& common,const PoliticsState& politics,PoliticsGovernanceState& civil,const PoliticsGovernanceCatalog& catalog,const PoliticalFiscalState& policies,const FiscalSettlementState& fiscal,const DynamicStepInput& phase,const LocalEconomyEffects& effects){
    if(!catalog.enabled)return common_success();
    if(phase.from_tick<0||phase.from_tick==INT64_MAX||phase.to_tick!=phase.from_tick+1||common.tick()!=phase.to_tick||fiscal.last_step_tick!=phase.to_tick)return common_failure("FiscalCivilBoundaryMismatch",{},ErrorKind::internal);
    std::set<StableId> seen;std::vector<StableId> periods;
    for(const auto& id:effects.fact_ids){
        const auto* fact=common.state().facts.find(id);
        if(!seen.insert(id).second||!fact||fact->id!=id||fact->domain!="local_economy")return common_failure("InvalidFiscalCivilEffect",id,ErrorKind::internal);
        if(fact->kind!="fiscal_period_assessed")continue;
        const auto* period=fiscal.periods.find(id);
        if(!period||period->id!=id||period->due_tick!=phase.to_tick)return common_failure("FiscalCivilPeriodMismatch",id,ErrorKind::internal);
        // 原财政仍登记无政策周期；它不能因缺省normal而获得政治后果。
        if(!period->policy)continue;
        auto source=read_assessed_fiscal_civil_source(common,politics,civil,catalog,policies,fiscal,id);
        if(!source.ok)return {false,{},source.error};
        periods.push_back(id);
    }
    for(const auto& id:periods){auto applied=apply_fiscal_civil_consequence_in_transaction(common,politics,civil,catalog,policies,fiscal,id);if(!applied.ok)return {false,{},applied.error};}
    return common_success();
}
Status settle_political_fiscal_civil_world(WorldBundle& world,const DynamicStepInput& phase,const LocalEconomyEffects& effects){
    if(!world.governance_tasks_enabled()||!world.fiscal_settlement_enabled())return common_success();
    if(!world.catalog->politics_governance||!world.catalog->politics_governance->enabled)return common_failure("MissingGovernancePhaseCatalog",{},ErrorKind::internal);
    auto legacy=settle_political_fiscal_civil_in_transaction(world.common,world.politics,world.politics_governance,*world.catalog->politics_governance,world.political_fiscal,world.fiscal_settlement,phase,effects);if(!legacy.ok||!world.occupation_fiscal_enabled())return legacy;
    auto current=OccupationFiscalSettlementWorldAccess::create(world);if(!current.ok)return {false,{},current.error};
    for(const auto& id:effects.fact_ids){const auto* header=world.common.state().facts.find(id);if(!header||header->kind!=occupation_fiscal_period_kind)continue;auto result=apply_occupation_fiscal_civil_in_transaction(world.common,world.politics,world.politics_governance,*world.catalog->politics_governance,&world.fiscal_settlement,*current.value,id);if(!result.ok)return {false,{},result.error};}
    return common_success();
}
}
