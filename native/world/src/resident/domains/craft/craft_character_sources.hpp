// Actual S4 body and original Growth skill sources for one Craft acceptance.
#pragma once
#include "craft_job.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include <cstddef>
#include <vector>
#include "resident/domains/actor_foundation/character_capability.hpp"
#include "resident/domains/actor_foundation/condition_source_read.hpp"
#include "resident/domains/growth/growth_skill_qualification_v2.hpp"
namespace hanmo::resident {
struct CraftCharacterSourceContext {
 const CharacterConditionSources& body;const GrowthState& growth;
 const CraftJobsState& jobs;const CraftRecipeCatalog& recipes;const CurrentMomentRead& clock;
};
struct CraftParticipantQualification {
 std::vector<ActorConditionHead> actual_condition_heads;
 std::vector<MomentOwnerSource> actual_body_sources;
 Integer maker_skill_level=0;
};
// Pure current preview. No future Job/source ID is manufactured to read skill.
Outcome<CraftParticipantQualification> read_current_craft_participant_qualification(const CraftCharacterSourceContext&,const StableId& maker,const std::vector<StableId>& helpers,const CraftRecipeRef&);
// Pure source-local boundary. It deliberately does not re-enter full Craft,
// Growth skill validation, fees, materials, or facility admission.
class NativeCraftSkillBoundaryRead final:public GrowthSkillBoundarySourceRead {
 const CommonStore& common_;const CraftJobsState& jobs_;
public:
 NativeCraftSkillBoundaryRead(const CommonStore&common,const CraftJobsState&jobs):common_(common),jobs_(jobs){}
 bool bound_to(const CommonStore&)const noexcept override;
 Outcome<GrowthSkillActualBoundary> read_actual_skill_boundary(const StableId& actor_id,const SkillQualificationBoundaryRefV2&)const override;
};
// Current preview rechecks current heads; existing source replays its immutable
// adopted head and real Common order. Recipe learning remains its S4 source.
Status validate_craft_participant_qualification(const CraftCharacterSourceContext&,const CraftTerms&,const SourceRef* actual_acceptance);
// Service input capture owns no new body state. These are the real seed/receipt
// refs for the maker and every accepted helper at the selected slice's from.
// The current capture is temporary until the actual-to Craft writer saves its
// existing CraftServiceSpan. No future span is passed to the S4 history reader.
class CapturedCraftParticipantBodyInputs final {
 CheckpointMemoryCharge copied_fields_;
 std::vector<MomentOwnerSource> sources_;
 std::size_t capacity_=0;
 static CheckpointMemoryCharge storage_memory(std::size_t);
 explicit CapturedCraftParticipantBodyInputs(std::size_t);
 void append(const SourceRef&,Integer);
 friend Outcome<CapturedCraftParticipantBodyInputs> capture_craft_participant_body_inputs(const CraftCharacterSourceContext&,const CraftJob&,const MomentSpan&);
public:
 CapturedCraftParticipantBodyInputs()noexcept=default;
 ~CapturedCraftParticipantBodyInputs()=default;
 CapturedCraftParticipantBodyInputs(const CapturedCraftParticipantBodyInputs&)=delete;
 CapturedCraftParticipantBodyInputs& operator=(const CapturedCraftParticipantBodyInputs&)=delete;
 CapturedCraftParticipantBodyInputs(CapturedCraftParticipantBodyInputs&&)noexcept=default;
 CapturedCraftParticipantBodyInputs& operator=(CapturedCraftParticipantBodyInputs&&)=delete;
 // Borrow only while this exact capture output owner remains alive.
 const std::vector<MomentOwnerSource>& sources()const noexcept{return sources_;}
};
Outcome<CapturedCraftParticipantBodyInputs> capture_craft_participant_body_inputs(const CraftCharacterSourceContext&,const CraftJob&,const MomentSpan& selected_span);
bool is_craft_participant_body_source(const MomentOwnerSource&)noexcept;
Status validate_prepared_craft_participant_body_source(const CraftCharacterSourceContext&,const CraftJob&,const WorkServiceIntervalProof&,const MomentOwnerSource&);
// Checks complete participant/source coverage and the actual whole span. The
// original C02 gate remains N service units; this adds no efficiency multiplier.
Status validate_craft_participant_body_span(const CraftCharacterSourceContext&,const CraftJob&,const WorkServiceIntervalProof&);
}
