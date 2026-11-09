// S0's original Common Action/participant occupation execution. These are
// internal candidate operations, not commands or permission/admission setters.
// The original owner must validate its accepted terms, participants and real
// start/completion/stop source first. The outer World candidate owns rollback.
#pragma once
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
// ID and all fields come from the original prepared Action. This allocates no
// second action ID or occupation table. Every participant must still be free.
Status adopt_started_action_in_candidate(CommonStore&,const ActionCoreView&);
// Original resource release precedes matching participant release. Keep the
// original Action head until its owning domain has cleaned its runtime payload.
// Original strict owners (e.g. Care) retain their full participant equality gate
// before this common operation. A departed participant is never cleared from a
// different Action by this operation.
Status release_action_occupation_in_candidate(CommonStore&,const StableId&);
// Last step only, after the original domain payload cleanup. Refuses to erase
// a head while one of its actual participants still points at this Action.
Status erase_released_action_in_candidate(CommonStore&,const StableId&);
// Only the original owner's successful pause/resume decision may call these.
// Keep the actual Action, remaining work and all resource reservations intact.
// Pause requires its entire current participation to still occupy this Action.
// Resume requires an empty participation set and the original revalidated set
// to be wholly free. Neither operation allocates an Action or grants authority.
Status pause_action_participation_in_candidate(CommonStore&,const StableId&);
Status resume_action_participation_in_candidate(CommonStore&,const StableId&,
                                                const std::vector<StableId>& original_participants);
}
