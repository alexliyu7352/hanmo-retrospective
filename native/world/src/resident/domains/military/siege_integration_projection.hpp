// Original integration output, borrowed from the same typed field table.
#pragma once
#include "siege_integration_decisions.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status emit_siege_integration_state_fields(ProjectionWriter&,const SiegeIntegrationState&);
Status emit_military_integration_facts(ProjectionWriter&,const StableId& name,const std::vector<MilitaryIntegrationFact>&);
}
