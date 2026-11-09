// Same-candidate adoption and original terminal-publication cuts for true trade XP.
#pragma once
#include "growth_fixed_execution.hpp"
#include "growth_trade_grant.hpp"
namespace hanmo::resident {
Outcome<const GrowthTerminalPublication *> read_growth_trade_grant_publication_v1(
    const CharacterConditionSources &, const GrowthState &, const GrowthTradeGrantRef &);
Status adopt_growth_trade_grant_inputs_in_candidate(CommonStore &, GrowthState &,
    const CharacterConditionSources &, ActionElapsedStore &, const GrowthTradeGrantRef &);
Status validate_growth_trade_grant_input_coverage(const CharacterConditionSources &,
    const GrowthState &);
} // namespace hanmo::resident
