// The original Growth owner executes its real fixed-service gates in the common
// exact phase. Reads are borrowed afresh from the same WorldBundle owning lease.
#pragma once
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
ExactPhaseRegistration make_growth_exact_phase_registration();
}
