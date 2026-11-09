#pragma once
#include "repair_contract.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/growth/growth_skill_qualification_v2.hpp"
namespace hanmo::resident {
struct RepairCharacterSourceContext {const CharacterConditionSources& body;const GrowthState& growth;const ItemInstancesState& items;const RepairPolicyCatalog& policies;const CurrentMomentRead& clock;};
struct RepairWorkerQualification {ActorConditionHead actual_condition_head;MomentOwnerSource actual_condition_source;Integer actual_skill_level=0;};
Outcome<RepairWorkerQualification> read_current_repair_worker_qualification(const RepairCharacterSourceContext&,const StableId&worker,const RepairPolicyRef&);
class NativeRepairSkillBoundaryRead final:public GrowthSkillBoundarySourceRead {
 const CommonStore& common_;const ItemInstancesState& items_;
public:NativeRepairSkillBoundaryRead(const CommonStore&c,const ItemInstancesState&i):common_(c),items_(i){}
 bool bound_to(const CommonStore&)const noexcept override;
 Outcome<GrowthSkillActualBoundary> read_actual_skill_boundary(const StableId&,const SkillQualificationBoundaryRefV2&)const override;
};
Status validate_repair_worker_qualification(const RepairCharacterSourceContext&,const RepairTerms&,const RepairJob*actual_job);
// Replays precisely the Condition source in the actual Fixed input adoption.
// Workstation, occupancy, custody, skill and accepted service remain separate.
Outcome<CharacterCapabilityView> read_repair_worker_body_span(const RepairCharacterSourceContext&,const RepairJob&,const std::vector<FixedServiceInputRef>&actual_inputs,const MomentSpan&);
}
