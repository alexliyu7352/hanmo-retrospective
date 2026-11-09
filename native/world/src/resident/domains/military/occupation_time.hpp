// The occurrence timestamp belongs to the original occupation row. No second ledger.
#pragma once
#include "military_model.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct MilitaryActualSourceWindow;
struct MilitaryOccupationExecution {
 MilitaryOccupationProducerKind producer_kind=MilitaryOccupationProducerKind::calendar_siege_results;
 // Only the integration producer supplies this real, already-applied transfer.
 StableId integration_transfer_id;
};
Outcome<MilitaryOccupationTime2> capture_military_occupation_time(
 const CommonRead&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const MilitaryOccupationFact&,const MilitaryOccupationExecution&);
// Source schema is explicit. /2 never interprets a legacy tick as fractional time.
Outcome<WorldMoment> read_military_occupation_actual_moment(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const SourceRef&,
 Integer source_schema,const WorldMoment& current,ExactArithmeticBudget={});
// Borrowed projection of the one original applied ownership chain. The caller
// proves the boundary payload's exact M; this reader proves its actual Common
// cut, every occupation predecessor, and the selected original owner. No current
// owner or integer promotion substitutes for an exact occupation source.
struct MilitaryGarrisonOwnershipBeforeSource {
 const GarrisonDefinition* original_garrison=nullptr;
 const MilitaryOccupationFact* actual_occupation=nullptr;
 Integer occupation_source_schema=1;
 const StableId& organization_id()const noexcept{return actual_occupation?actual_occupation->to_organization_id:original_garrison->organization_id;}
};
Outcome<MilitaryGarrisonOwnershipBeforeSource> read_military_garrison_owner_before_actual(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const StableId& garrison,std::optional<SourceRefBuildView> actual_boundary,const WorldMoment& boundary_at,
 const WorldMoment& current,Integer selected_occupation_schema,ExactArithmeticBudget={});
// Same strict owner source proof, with an enclosing read window. A future
// retained Return may call this for source coverage; the result is not a
// state-at-window projection. Genuine occupation occurrences check both sides.
Outcome<MilitaryGarrisonOwnershipBeforeSource> read_military_garrison_owner_before_actual(
 const CommonStore&,const MilitaryState&,const MilitaryCatalog&,const MilitaryAccess&,
 const StableId& garrison,std::optional<SourceRefBuildView> actual_boundary,const WorldMoment& boundary_at,
 const WorldMoment& current,Integer selected_occupation_schema,ExactArithmeticBudget,
 const MilitaryActualSourceWindow* source_window);
Status validate_military_occupation_times(const CommonStore&,const MilitaryState&,
 const MilitaryCatalog&,Integer selected_schema,const WorldMoment& current,ExactArithmeticBudget={});
}
