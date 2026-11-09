// Concrete S2 -> original S4 consequence barrier. No legacy body mutation.
#pragma once
#include "military_exact_sources.hpp"
#include "resident/domains/actor_foundation/military_condition.hpp"
#include "resident/domains/actor_foundation/life_condition.hpp"
#include "resident/domains/actor_foundation/military_availability.hpp"
namespace hanmo::resident {
class GrowthCapabilityRevisionWrite;
class CareLifeConditionWrite;
struct DynamicMilitaryConditionApplication {
    std::vector<ConditionReceiptRefV2> condition_receipts;
    std::vector<DeathRef> death_refs;
    std::vector<BoundActorConditionWrite> actual_writes;
    std::vector<StableId> availability_transition_ids;
    std::vector<BoundActorAvailabilityWrite> availability_writes;
};
// Must run in the same actual-M candidate after the real casualty row and before
// objective/CLC/MD. Captures are borrowed from the enclosing S0 slice. Original
// S1 travel/participant/custody effects are a subsequent required barrier.
Outcome<DynamicMilitaryConditionApplication> apply_dynamic_military_named_conditions_in_candidate(
    CommonStore&,AuthoredActorFoundationState&,FamilyDeathState&,
    const ActorFoundationCatalog&,const MilitaryState&,const MilitaryExactState&,
    const FamilyLifeCauseSources4&,const SourceRef& actual_casualty,
    const CurrentReadStampRead&,ReadStampAuthority&,const DomainReadVersionSource&,
    const CharacterConditionSources&,GrowthCapabilityRevisionWrite* capability_changes=nullptr,
    CareLifeConditionWrite* care_life=nullptr);
}
