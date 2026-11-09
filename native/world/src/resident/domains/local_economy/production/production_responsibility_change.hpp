// Same-candidate S6 responsibility change -> original S5 input transition.
#pragma once
#include "production_input_source.hpp"
namespace hanmo::resident {
struct ProductionResponsibilityChangeAccess;
class PreparedProductionResponsibilityChange {
 friend struct ProductionResponsibilityChangeAccess;
 struct Entry {StableId unit_id;std::optional<PreparedProductionInputClose> close;};
 const CommonStore* common_=nullptr;const EconomicWorkSourcesState* sources_=nullptr;
 WorldMoment at_;std::vector<Entry> entries_;bool valid_=false;
public:
 PreparedProductionResponsibilityChange()=default;
 PreparedProductionResponsibilityChange(const PreparedProductionResponsibilityChange&)=delete;
 PreparedProductionResponsibilityChange& operator=(const PreparedProductionResponsibilityChange&)=delete;
 PreparedProductionResponsibilityChange(PreparedProductionResponsibilityChange&&)=default;
 PreparedProductionResponsibilityChange& operator=(PreparedProductionResponsibilityChange&&)=default;
};
// Capture after the actual S6 original responsibility inventory has been checked,
// before occupation/registration/decision mutation. This writes no source or ID.
Outcome<PreparedProductionResponsibilityChange> prepare_production_responsibility_change(
 const CommonStore&,const LocalEconomyState&,const LocalEconomyCatalog&,
 const EconomicProductionContext&,const EconomicProductionAdoptionRead&,
 const std::vector<PoliticalOriginalResponsibility>&,const WorldMoment&);
// Call after the complete original registration/approval and all decisions exist.
// Failures require discarding the outer full candidate, including S6 and Common.
Outcome<std::vector<SourceRef>> apply_production_responsibility_change_in_candidate(
 CommonStore&,EconomicProductionContext&,const EconomicProductionAdoptionRead&,
 const PoliticalTakeoverState&,PreparedProductionResponsibilityChange&&,
 const MomentOwnerSource& actual_change);
}
