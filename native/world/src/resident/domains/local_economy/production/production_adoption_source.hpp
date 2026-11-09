// Original production cut evidence. This does not validate a resulting labour
// integral and must not call the production-input or political-history reader.
#pragma once
#include "../economic_work_sources.hpp"
#include "facility_condition_history.hpp"
namespace hanmo::resident {
struct EconomicProductionAssetScope { StableId unit_id;std::vector<StableId> production_asset_ids; };
struct EconomicProductionAdoption {
 StableId unit_id;
 MomentOwnerSource actual_source;
 WorldMoment occurred_at;
 std::vector<StableId> production_asset_ids;
};
class EconomicProductionAdoptionRead {
public:
 virtual ~EconomicProductionAdoptionRead()=default;
 virtual Outcome<EconomicProductionAssetScope> read_original_production_asset_scope(const StableId& unit_id)const=0;
 virtual Outcome<EconomicProductionAdoption> read_production_adoption(
  const StableId& unit_id,const MomentOwnerSource&)const=0;
};
struct OriginalProductionSupplyContext;
struct OriginalProductionPopulationChangeContext;
struct EconomicProductionAdoptionContext {
 const CommonStore& common;const LocalEconomyState& economy;
 const LocalEconomyCatalog& catalog;const EconomicWorkCatalog& work_catalog;
 const EconomicWorkSourcesState& sources;
 const ProductionFacilityHistoryContext& conditions;
 const CurrentMomentRead& clock;
 const OriginalProductionSupplyContext* supply=nullptr;
 const OriginalProductionPopulationChangeContext* population_changes=nullptr;
};
class NativeEconomicProductionAdoptionRead final:public EconomicProductionAdoptionRead {
 const EconomicProductionAdoptionContext& x_;
public:
 explicit NativeEconomicProductionAdoptionRead(const EconomicProductionAdoptionContext& x):x_(x){}
 Outcome<EconomicProductionAssetScope> read_original_production_asset_scope(const StableId&)const override;
 Outcome<EconomicProductionAdoption> read_production_adoption(
  const StableId&,const MomentOwnerSource&)const override;
};
}
