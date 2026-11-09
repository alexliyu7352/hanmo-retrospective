// Finite original Growth-start branch of P24.1 occupation conflict.
#pragma once
#include "care_family_activity_read.hpp"
#include "resident/domains/growth/growth_started_participation.hpp"
namespace hanmo::resident {
Outcome<GrowthStartedParticipationRead> read_care_growth_start_conflict(
 const CareFamilyActivitySources&,const CareFamilyActivity&,
 const CareParticipationConflict&);
// Actual original Growth headers within this activity's Common prefix. This
// proves only this owner branch; the complete occupation provider is still
// mandatory for the other F/W/V/M/Legacy adopts and for pause/resume changes.
Status validate_care_growth_start_conflict_coverage(const CareFamilyActivitySources&,
 const CareFamilyActivity&,const SourceRef& actual_completed);
Outcome<CareRequirementApplication> lose_care_coverage_after_growth_start_in_candidate(
 CommonStore&,CareGoalWriteTables,CareCoverageWriteTables,
 const CareGoalSourceContext&,const StableId& binding,const MomentOwnerSource&);
struct CareGrowthStartWriteBinding {
 CommonStore& common;GrowthState& growth;
 CareGoalWriteTables goals;CareCoverageWriteTables coverage;
 const CareFamilyActivitySources& sources;
 AffairsState& affairs;const StateNeedPolicyRead& policies;
 const StateGoalRead& goal_read;StateTaskConsumer& tasks;
};
class NativeCareGrowthStartedParticipationWrite final:public GrowthStartedParticipationWrite {
 CareGrowthStartWriteBinding binding_;
public:explicit NativeCareGrowthStartedParticipationWrite(CareGrowthStartWriteBinding b):binding_(b){}
 bool bound_to(const CommonStore&,const GrowthState&)const noexcept override;
 Status consume_actual_growth_start(const MomentOwnerSource&,const WorldMoment&)override;
};
}
