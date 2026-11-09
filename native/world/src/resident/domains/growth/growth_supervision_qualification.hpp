#pragma once
#include "growth_skill_qualification_v2.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
namespace hanmo::resident {
// The caller's real S6 scope reader resolves these values from its locked
// executor qualification. This function grants no authority or work credit.
struct GrowthSupervisionSkillRequirement {StableId skill_id;Integer minimum_level=0;};
Outcome<bool> read_growth_supervision_skill_eligible(const CharacterConditionSources&,const GrowthState&,const GrowthSkillQualificationV2& original_start_qualification,const GrowthSkillBoundarySourceRead&,const StableId& actor,const GrowthSupervisionSkillRequirement&,const MomentSpan& actual_span);
}
