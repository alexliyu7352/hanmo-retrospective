#pragma once
#include "field_session.hpp"
namespace hanmo::resident {
// Local result events are the original ordered output, not global DeathRefs.
// This closes their bidirectional retained-state relationship. Actual casualty
// roster math and world application remain their existing original validators.
Status validate_field_local_event_sources(const FieldBattleSession &,
                                          const FieldBattleParameters &);
} // namespace hanmo::resident
