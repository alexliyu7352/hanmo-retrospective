// One real C02 gate calls the original S4 exertion writer for each participant.
#pragma once
#include "craft_job.hpp"
#include "resident/domains/actor_foundation/craft_condition.hpp"
namespace hanmo::resident {
class GrowthCapabilityRevisionWrite;
struct CraftGateConditionWriteContext {
 AuthoredActorFoundationState& foundation;
 const CharacterConditionSources& body;
 const CurrentReadStampRead& stamps;ReadStampAuthority& stamp_authority;
 const DomainReadVersionSource& versions;GrowthCapabilityRevisionWrite& capability;
 std::vector<BoundActorConditionWrite>& condition_writes;
};
// The result refs are written directly into the actual gate. No callback can
// substitute caller-provided exertion IDs, quantities, fatigue or participants.
Status apply_craft_gate_conditions_in_candidate(CommonStore&,CraftJobsState&,const CraftJobReadContext&,const CraftGateConditionWriteContext&,const StableId& actual_gate_id);
Status validate_craft_gate_body_effects(const CraftJobReadContext&,const CraftGateApplied&);
}
