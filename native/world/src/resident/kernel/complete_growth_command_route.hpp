#pragma once
#include "resident/domains/growth/growth_refs.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Each call borrows this actual world and obtains its own short-lived original
// permit. A prepared quote never carries a permit across a candidate clone.
Outcome<GrowthQuote> preview_selected_growth_command(const WorldBundle&,
    const StableId& actor,const GrowthCommand&);
Outcome<GrowthEffect> apply_selected_growth_instant(WorldBundle&,
    const StableId& actor,const GrowthQuote&);
}
