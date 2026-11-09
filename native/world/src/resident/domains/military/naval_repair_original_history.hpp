#pragma once
#include "naval_asset_history.hpp"
#include "military_roster_history.hpp"
#include "military_formation_source.hpp"
namespace hanmo::resident {
// All references belong to one World candidate. The two history folds below
// verify their own actual sources and do not call the complete repair reader.
struct ExactNavalRepairHistoryContext {
 const CommonStore& common;const MilitaryState& military;const MilitaryCatalog& military_catalog;
 const MilitaryAccess& access;const NavalState& naval;const NavalCatalog& naval_catalog;
 const MilitaryActionTimeState& actions;const ActionElapsedStore& elapsed;
 const MilitaryExactState& military_exact;const TimeScaleRef& scale;const CurrentMomentRead& clock;
 const PopulationMilitaryState* population=nullptr;const PopulationMilitaryCatalog* population_catalog=nullptr;
 const MilitaryFormationSourceRead* formations=nullptr;const MilitaryReturnSourceRead* returns=nullptr;
 const ExactTrainingPeriodRead* supply_periods=nullptr;const NavalHistoryAccess* navigation_geometry=nullptr;
 Integer occupation_source_schema=1;ExactArithmeticBudget budget{};
};
// Original vessel/formation/commander/unit/member/recipe acceptance only.
// Actual actor presence, availability, occupation and stationary service are
// separate mandatory inputs of ExactNavalRepairOriginalRead, not inferred here.
Status validate_exact_naval_repair_asset_crew_acceptance(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
// The unique actual result cut, before this repair's own hull write in replay.
Status validate_exact_naval_repair_result_asset_predecessor(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
struct ExactNavalRepairServiceAuthority {
 const NavalRepairService* original_service=nullptr;
 StableId organization_id,occupation_id;
 std::optional<MomentOwnerSource> actual_ownership_source;
 std::optional<WorldMoment> ownership_changed_at;
 bool original_service_current=false;
};
// Source-free option is current trusted M only. A supplied boundary must be
// this exact repair's actual start or result Common head and its original M.
Outcome<ExactNavalRepairServiceAuthority> read_exact_naval_repair_service_authority(
 const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&,
 const std::optional<SourceRef>& actual_repair_boundary={});
// Same original roster fold, observing every intermediate source. Endpoint
// equality cannot hide a change and later recovery. Changes exactly at span.to
// do not invalidate the preceding half-open service interval.
Status validate_exact_naval_repair_crew_span(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&,const MomentSpan&);
Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_crew_change(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_crew_result(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
// First actual loss of the adopted service source, ordered by the original
// Common causal chain. A later ownership change cannot replace this cause.
Outcome<std::optional<ExactNavalRepairInterruption>> read_first_exact_naval_repair_service_change(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
Status validate_exact_naval_repair_original_charge(const ExactNavalRepairHistoryContext&,const NewNavalRepairTimeSource2&);
// Actual current admission only; immutable replay is handled by the original
// start writer before this gate. The amount/owners are the original dock rule.
Outcome<NavalRepairQuote> read_exact_naval_repair_current_offer(const ExactNavalRepairHistoryContext&,const StableId& actor,const NavalRepairStart&);

}
