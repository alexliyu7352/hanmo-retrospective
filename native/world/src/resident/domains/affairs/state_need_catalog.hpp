// C19.2 author policy belongs to the original Affairs catalogue. It creates no Need.
#pragma once
#include "state_need.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
struct StateNeedCatalog {Integer version=1;OrderedTable<StateNeedPolicy> policies;};
// The finite author deadline_policy is null or an explicit Calendar {tick,inclusive}.
// It is not a duration, Birth-relative rule, or permission to reopen a closed episode.
Status validate_state_need_catalog(const StateNeedCatalog&);
Outcome<StateNeedCatalog> decode_state_need_catalog(const BoundaryValue&);
Outcome<BoundaryValue> encode_state_need_catalog(const StateNeedCatalog&);
Status emit_state_need_catalog(ProjectionWriter&,std::string_view,const StateNeedCatalog&);
}
