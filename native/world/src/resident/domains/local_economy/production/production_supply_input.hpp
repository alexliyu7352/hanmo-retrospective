// Actual original ration evidence for production. No new food or worker balance.
#pragma once
#include "../economic_calendar.hpp"
#include "../population_change_time.hpp"
namespace hanmo::resident {
struct OriginalProductionSupplyContext {
 const EconomicCalendarReadContext& calendar;
 const LocalEconomyCatalog& catalog;
 const PopulationChangeTimeContext& population;
};
struct OriginalProductionRationSource {
 const RationReceipt* receipt=nullptr;WorldMoment applied_at;
};
// Original Calendar application, proved by the actual opening and source order.
// This does not upgrade the public Calendar source schema or fabricate a Moment.
Outcome<OriginalProductionRationSource> read_original_production_ration_source(
 const OriginalProductionSupplyContext&,const SourceRef&,Integer source_schema);
struct OriginalProductionSupplyInput {
 Integer labor_rate_numerator=0,labor_rate_denominator=1;
 std::vector<SourceRef> actual_supply_refs;
 // Original rule expiry only; this does not create an event or advance a clock.
 std::optional<WorldMoment> next_rule_boundary;
};
// The owning input reader has verified the actual adoption source and its Moment.
// This pure projection selects only original receipts already applied at that cut.
Outcome<OriginalProductionSupplyInput> read_original_production_supply_input(
 const OriginalProductionSupplyContext&,const LaborAllocation& actual_allocation,
 const WorldMoment& actual_adoption,const SourceRef& actual_cut,Integer fixed_denominator);
Status validate_original_production_supply_window(
 const OriginalProductionSupplyContext&,const LaborAllocation& actual_allocation,
 const WorldMoment& actual_adoption,const SourceRef& actual_cut,Integer fixed_denominator,
 const OriginalProductionSupplyInput&,const MomentSpan&);
}
