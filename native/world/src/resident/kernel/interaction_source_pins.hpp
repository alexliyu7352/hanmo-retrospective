// Original Interaction life-source retention. These are derived consumers,
// never saved reference counts or replacement Life/Action source owners.
#pragma once
#include "source_retention.hpp"
#include "resident/domains/interaction/interaction_session_life.hpp"
namespace hanmo::resident {
// Requires the actual complete same-candidate lease only when selected.
// This strict walk belongs after the real result/release seal, before pruning.
Status visit_complete_interaction_life_pins(const WorldBundle&,InteractionSessionLifeSourcePinSink&);
// Install all six finite slots in the complete registry, together with the
// real Death4, occupancy1, command-result1 and Growth-boundary1 source owners.
// This factory alone is not a complete SourceRetentionRegistry.
Outcome<std::vector<SourceConsumerRegistration>> make_interaction_session_life_source_consumers();
// The actual immutable confirmation appendix is keyed by its real outer
// Common result. These six slots retain precisely the sources checked by the
// original acyclic confirmation reader, not a second history reconstruction.
Outcome<std::vector<SourceConsumerRegistration>> make_interaction_action_confirmation_source_consumers();
}
