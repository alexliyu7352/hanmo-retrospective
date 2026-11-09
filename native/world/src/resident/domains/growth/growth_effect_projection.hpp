#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_growth_effect_fields(ProjectionWriter&,const GrowthEffect&);
namespace growth {
Status emit_growth_public_challenge_fields(ProjectionWriter&,const GrowthPublicChallenge&);
Status emit_growth_challenge_result_fields(ProjectionWriter&,const GrowthChallengeResult&,const StableId& kind);
}
}
