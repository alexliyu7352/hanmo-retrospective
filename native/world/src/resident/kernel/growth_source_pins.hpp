#pragma once
#include "source_retention.hpp"
namespace hanmo::resident {
// Actual original command and saved-prefix consumers only. The full composition
// supplies its finite compiled Common-header source kinds, never arbitrary JSON paths.
Outcome<std::vector<SourceConsumerRegistration>> make_growth_original_source_consumers(
    const std::vector<RetentionSourceKind>& compiled_header_sources);
Status validate_growth_original_fact_prefixes(const WorldBundle&);
}
