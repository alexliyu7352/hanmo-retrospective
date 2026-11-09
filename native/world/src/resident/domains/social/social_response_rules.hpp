// P24.3 original request arithmetic. These pure values are not a permission,
// consent, known-source reader, or a completed SocialDecision. The request
// owner must first prove hard gates and obtain the effective, sourced inputs.
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct OriginalSocialScoreInputs {
 Integer effective_trust=0,effective_affinity=0;
 Integer auxiliary_attribute=0,skill_level=0;
 // Already composed in the single original interest slot. Card modifiers
 // belonging to this slot must not be added again after this calculation.
 Integer interest_modifier=0,difficulty=0;
};
enum class OriginalSocialResponseBand {accept,refuse,negotiate};
struct OriginalSocialResponseScore {
 Integer score=0;
 OriginalSocialResponseBand band=OriginalSocialResponseBand::refuse;
};
Outcome<OriginalSocialResponseScore> score_original_social_response(
 const OriginalSocialScoreInputs&);
// Only the middle band may consume one actual original etiquette result.
// This does not run or replace the existing Growth challenge algorithm, and
// supplies no once record; the original request owner must prove that source.
Outcome<OriginalSocialResponseScore> finish_original_social_negotiation(
 Integer original_score,Integer actual_etiquette_delta);
// Unknown family standing contributes zero, not a negative recognition.
// A present value must already come from the single relevant known branch or
// clan projection. This helper does not add both or infer membership.
Outcome<Integer> original_social_family_credit(std::optional<Integer> known_standing);
}
