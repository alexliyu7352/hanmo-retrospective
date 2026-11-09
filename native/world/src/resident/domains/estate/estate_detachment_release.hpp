// Original lost-retainer conditional only; not the player EstateDetachment command.
#pragma once
#include "estate_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct OriginalEstateDetachmentReleaseView {
 std::string_view estate_id,actor_id,source_id;
 const StableId* detached_before;
 const StableId* detached_after;
};
class OriginalEstateDetachmentReleaseSink {
public:
 virtual ~OriginalEstateDetachmentReleaseSink()=default;
 virtual Status append(const OriginalEstateDetachmentReleaseView&)=0;
};
// Only the original conditional slot effect. Request/pledge correspondence and
// lost classification belong to the actual caller's original source gate.
// Scalar slot values are not SourceRef/schema authority or current-state proof.
Status validate_original_lost_estate_retainer_release(
 const OriginalEstateDetachmentReleaseView&);
// Request IDs borrow the retained original caller's pledge, not an erased slot.
// The sink must copy any retained observation before append returns. Its views
// do not survive this call. A sink failure requires discarding the candidate.
Status release_original_lost_estate_retainer_in_candidate(
 EstateState&,const StableId& estate_id,const StableId& actor_id,
 const StableId& source_id,OriginalEstateDetachmentReleaseSink*);
}
