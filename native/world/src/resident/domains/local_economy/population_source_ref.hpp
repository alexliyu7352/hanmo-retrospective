#pragma once
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct CivilianPopulationRef { StableId cohort_id;Integer population_revision=0;std::optional<StableId> transport_binding_id; };
}
