// S0 service for the original military session seed derivation. The caller's
// real SessionInitialized row owns this projection; Common owns no second fact
// or writable session stream. This service neither allocates IDs nor draws RNG.
#pragma once
#include "resident/model/common_model.hpp"
#include "source_ref.hpp"
namespace hanmo::resident {
struct OriginalSessionRngInitialization {
    SourceRef initialization_source_ref;
    StableId session_ref;
    Integer generation=0;
    SourceRef entry_or_contact_source_ref;
    StableId algorithm_version,seed,state;
    std::uint64_t draw_ordinal=0;
};
// The military owner must first resolve both actual references against its
// original entry/contact and SessionInitialized payloads in this same candidate.
Outcome<OriginalSessionRngInitialization> derive_original_session_rng_initialization(
    const CommonState&,const StableId& session_ref,Integer generation,
    const SourceRef& actual_entry_or_contact,const SourceRef& actual_initialization);
// Cold proof recomputes only the immutable original seed formula. It never
// initializes the current session again or consumes a business random slot.
Status validate_original_session_rng_initialization(const CommonState&,const OriginalSessionRngInitialization&);
}
