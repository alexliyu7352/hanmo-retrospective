// Original C05/C07 acceptance inputs, not a parallel body or condition history.
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
namespace hanmo::resident {
struct MilitaryActionAcceptanceConditionsBuildView {
 const CheckpointVector<const ActorConditionHead*>& participant_heads;
};

struct MilitaryActionAcceptanceConditions1;


struct MilitaryActionAcceptanceConditions1 {
 CheckpointMemoryCharge copied_fields;
 std::vector<ActorConditionHead> participant_heads;
 MilitaryActionAcceptanceConditions1()=default;
 explicit MilitaryActionAcceptanceConditions1(const MilitaryActionAcceptanceConditionsBuildView&);
 MilitaryActionAcceptanceConditions1(const MilitaryActionAcceptanceConditions1&);
 MilitaryActionAcceptanceConditions1(MilitaryActionAcceptanceConditions1&& v):MilitaryActionAcceptanceConditions1(static_cast<const MilitaryActionAcceptanceConditions1&>(v)){}
 MilitaryActionAcceptanceConditions1& operator=(MilitaryActionAcceptanceConditions1 v){swap(v);return *this;}
 void swap(MilitaryActionAcceptanceConditions1&)noexcept;
};

// /2 observations belong only to the original Action row's two real endpoints.
// The charge precedes every owned field and outlives them on every exit path.
struct MilitaryActionEndpointObservationBuildView2 {
 const CurrentCommonFactPrefix& common_prefix;
 const CharacterConditionAppliedPrefix& condition_prefix;
 const CheckpointVector<const ActorConditionHead*>& participant_heads;
};
struct MilitaryActionEndpointObservation2;


struct MilitaryActionEndpointObservation2 {
 CheckpointMemoryCharge copied_fields;
 CommonActualFactPrefix common_prefix;
 CharacterConditionAppliedPrefix condition_prefix;
 std::vector<ActorConditionHead> participant_heads;
 MilitaryActionEndpointObservation2()=default;
 explicit MilitaryActionEndpointObservation2(const MilitaryActionEndpointObservationBuildView2&);
 MilitaryActionEndpointObservation2(const MilitaryActionEndpointObservation2&);
 MilitaryActionEndpointObservation2(MilitaryActionEndpointObservation2&& v):MilitaryActionEndpointObservation2(static_cast<const MilitaryActionEndpointObservation2&>(v)){}
 MilitaryActionEndpointObservation2& operator=(MilitaryActionEndpointObservation2 v){swap(v);return *this;}
 void swap(MilitaryActionEndpointObservation2&)noexcept;
};
}
