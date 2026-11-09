// The original production target, not a new permission or work ledger.
#pragma once
#include "facility_condition_history.hpp"
#include "../economic_work_sources.hpp"
#include "resident/domains/politics/governance_targets/governance_supervision_target.hpp"
#include "resident/domains/politics/takeover/politics_takeover_history2.hpp"
#include "resident/domains/travel/spatial_carrier.hpp"
namespace hanmo::resident {
struct ProductionSupervisionTargetContext {
 const CommonStore& common;const CommonCatalog& shared;
 const LocalEconomyState& economy;const LocalEconomyCatalog& catalog;
 const EconomicWorkCatalog& work_catalog;const TravelCatalog& travel;
 const SpatialCarrierSources& spatial;const CurrentMomentRead& clock;
 const NativePoliticalSupervisionTakeoverRead& takeovers;
 const ProductionFacilityHistoryContext& conditions;
};
// Agronomy uses the one original unit/land/facility plan and actual responsibility
// history. Industry additionally needs its original financing contract; construction
// needs its original stage/site-use producer. Neither is replaced by agronomy.
class NativeProductionSupervisionTargetRead final:public GovernanceSupervisionTargetOriginalRead {
 const ProductionSupervisionTargetContext& x_;
public:
 explicit NativeProductionSupervisionTargetRead(const ProductionSupervisionTargetContext& x):x_(x){}
 Outcome<GovernanceSupervisionTargetView> read_supervision_target_at_start(
  const GovernanceSupervisionTargetRequest&,const WorldMoment&,const SourceRef*)const override;
 Outcome<GovernanceSupervisionTargetView> read_supervision_target_for_span(
  const GovernanceSupervisionTargetRequest&,const SupervisionActionStartSource&,const MomentSpan&)const override;
};
}
