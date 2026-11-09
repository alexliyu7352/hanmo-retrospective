// 协商整编的中央组合只借唯一世界，领域保留决定、资产和人口的权威。
#pragma once
#include "world_bundle.hpp"
#include "resident/domains/military/siege_integration_decisions.hpp"
#include "resident/domains/military/military_integration_assets.hpp"
namespace hanmo::resident {
inline bool siege_integration_command_kind(const StableId& kind){return kind=="siege_integration_propose"||kind=="siege_integration_decide"||kind=="siege_integration_cancel"||kind=="siege_integration_apply";}
MilitaryIntegrationAccess siege_population_access(WorldBundle&);
MilitaryIntegrationAccess siege_population_read_access(const WorldBundle&);
Outcome<SiegeIntegrationExecutionQuote> preview_siege_handover(const WorldBundle&,const StableId&,const SiegeIntegrationApply&);
Outcome<SiegeIntegrationEffect> apply_siege_handover(WorldBundle&,const StableId&,const SiegeIntegrationApply&,const SiegeIntegrationExecutionQuote&);
Status run_siege_integration_decisions(WorldBundle&);
std::optional<CommandPayload> siege_population_enrollment(const WorldBundle&,const StableId&,const ArmyForm&);
Status validate_siege_integration_world(const WorldBundle&);
}
