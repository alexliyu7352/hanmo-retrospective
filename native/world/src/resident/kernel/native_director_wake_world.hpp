#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Only the original native-new-world path may create the first registrations
// and adopt the original seed's observable sources as pending work.
Status initialize_native_complete_director_in_candidate(WorldBundle&);
// A successful original source-writing transaction calls this before pruning,
// savepoint publication or final validation. It never reviews or executes work.
Status collect_native_complete_director_wakes_in_candidate(WorldBundle&);
// Decode-time reconstruction of the derived index only. Existing persisted
// wake_reasons remain authoritative; this is never a live-candidate fallback.
Status restore_native_complete_director_wake_cache_in_candidate(WorldBundle&);
}
