#pragma once
#include "growth_permanent_contributions.hpp"
#include "resident/domains/actor_foundation/condition_capture.hpp"
namespace hanmo::resident {
Status initialize_growth_capability_revision_for_actor_in_candidate(CommonStore&,GrowthPermanentState&,
 const CharacterConditionSources&,const StableId& actor);
Status validate_growth_capability_chain(const GrowthPermanentSources&,const GrowthState&);
class NativeGrowthCapabilityRevisionWrite final:public GrowthCapabilityRevisionWrite {
 CommonStore& common_;AuthoredActorFoundationState& foundation_;GrowthState& growth_;
 GrowthPermanentState& state_;const GrowthPermanentSources& sources_;
public:
 NativeGrowthCapabilityRevisionWrite(CommonStore& c,AuthoredActorFoundationState& f,GrowthState& g,GrowthPermanentState& s,const GrowthPermanentSources& sources):common_(c),foundation_(f),growth_(g),state_(s),sources_(sources){}
 bool bound_to(const CommonStore&,const GrowthPermanentState&)const noexcept override;
 Status record_actual_growth_change(const StableId&,const LifeGrowthContributionRef&,const WorldMoment&)override;
 // The actual same-candidate Condition write token, not a receipt ID assertion.
 Status record_actual_condition_change(const BoundActorConditionWrite&)override;
};
}

namespace hanmo::resident {
struct GrowthFixedStepPermit;struct GrowthFixedStudyApplication;struct CommitResult;
// Called after this original result and its FSL boundary are materialized,
// before another business writer. Net growth precedes its original body writes.
Status record_growth_fixed_capability_result_in_candidate(const CharacterConditionSources&,
 const GrowthState&,const GrowthFixedStepPermit&,const GrowthFixedStudyApplication&,GrowthCapabilityRevisionWrite&);
// Called only after the real cached result/history and exact adjustment binding.
// It follows the original within-command application ordinal, never source IDs.
Status record_study_adjustment_capability_for_commit_in_candidate(const CharacterConditionSources&,
 const GrowthState&,const CommitResult&,GrowthCapabilityRevisionWrite&);
}
