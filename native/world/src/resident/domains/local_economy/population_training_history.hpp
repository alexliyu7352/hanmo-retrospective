// S6 keeps the original training activity/result. This read model borrows P02,
// actual Calendar openings and original ration receipts; it stores no work log.
#pragma once
#include "population_change_time.hpp"
#include "resident/domains/organization/organization_calendar_admission.hpp"
namespace hanmo::resident {
enum class OriginalTrainingCoverageState { complete,actual_supply_shortage,actual_participation_changed };
struct OriginalTrainingWindowRead {
 MomentSpan window;Integer original_persons=0,original_workers=0;
 OriginalTrainingCoverageState state=OriginalTrainingCoverageState::complete;
 std::vector<SourceRef> original_sources;
 std::optional<SourceRef> actual_blocking_source;
};
struct OriginalTrainingHistoryContext {
 const PopulationChangeTimeContext& population;
 const LocalEconomyCatalog& catalog;
 const EconomicCalendarReadContext& calendar;
 const OrganizationCalendarAdmissionRead& admission;
 const SpatialCarrierState& carriers;const MovementExactState& movement;
 const SpatialCarrierSources& spatial;
};
Outcome<OriginalTrainingWindowRead> read_original_population_training_window(
 const OriginalTrainingHistoryContext&,const OrgActivity&,const OrgActivityTerms&,
 const SourceRef& actual_approval,Integer from_tick,Integer until_tick);
class NativeOrganizationTrainingCoverageRead final:public OrganizationTrainingCoverageRead {
 const OriginalTrainingHistoryContext& x_;
public:explicit NativeOrganizationTrainingCoverageRead(const OriginalTrainingHistoryContext& x):x_(x){}
 Status validate_original_training_window(const OrgActivity&,const OrgActivityTerms&,
  const SourceRef&,Integer,Integer)const override;
};
}
