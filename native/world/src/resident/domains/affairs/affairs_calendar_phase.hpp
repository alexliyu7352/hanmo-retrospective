#pragma once
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
// The original early dynamic tail, with the original calendar boundary supplied
// by S0. No membership, political close, StateNeed deadline, or report delivery.
Status step_original_affairs_calendar(WorldBundle&,const ExactPeriodContext&);
Status validate_original_affairs_calendar_sources(const WorldBundle&,const WorldMoment&);
}
