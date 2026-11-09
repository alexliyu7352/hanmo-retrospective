// Original NPC result identity. The two business owners retain their own
// immutable authored intent/claim validation; this borrower owns no state.
#pragma once
#include "resident/domains/director/director_command_acceptance.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct VisitAuthoredCommandRead {
 const CommonStore& common;const DirectorState& director;const DirectorCatalog& catalog;
 const CatalogIdentity& identity;const CurrentMomentRead& clock;
};
// Strict source-to-adoption join. No current registry or future command is used.
Outcome<DirectorAcceptedCommandView> read_visit_authored_command(
 const VisitAuthoredCommandRead&,const MomentOwnerSource&,const StableId& actor,
 const StableId& actual_command,Integer original_calendar_tick);
}
