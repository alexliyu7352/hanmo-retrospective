// A private candidate witness, never a persistent commit identity or a codec slot.
#pragma once
#include "study_service_termination.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
class StudyPendingReleaseProof {
 struct State;
 std::shared_ptr<const State> state_;
 explicit StudyPendingReleaseProof(std::shared_ptr<const State>);
 friend Outcome<StudyPendingReleaseProof> stop_study_for_verified_death_in_capture(CommonStore&,GrowthState&,const CharacterConditionSources&,const GrowthAccess&,const StableId&,const StudyVerifiedParticipantDeath&,const BoundActorConditionWrite&,const ReadStamp&,const VerifiedDeathRead&,StudyOriginalActionReleaseWrite&);
public:
 StudyPendingReleaseProof()=default;
 const StableId* action_id()const noexcept;
 Status validate(const CharacterConditionSources&,const GrowthState&,const StudyTerminal&)const;
};
using StudyPendingReleaseProofs=CheckpointVector<StudyPendingReleaseProof>;
// The original accepted-service termination is applied first. A proof is only
// issued after its actual release and its true Death receipt/capture are checked.
Outcome<StudyPendingReleaseProof> stop_study_for_verified_death_in_capture(CommonStore&,GrowthState&,const CharacterConditionSources&,const GrowthAccess&,const StableId& action_id,const StudyVerifiedParticipantDeath&,const BoundActorConditionWrite&,const ReadStamp&,const VerifiedDeathRead&,StudyOriginalActionReleaseWrite&);
Outcome<const StudyServiceTerminalV2*> read_study_service_termination_in_capture(const CharacterConditionSources&,const GrowthState&,const GrowthAccess&,const StudyTerminal&,const ReadStamp&,const VerifiedDeathRead&,const StudyPendingReleaseProof&);
}
