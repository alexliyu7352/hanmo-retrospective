// Finite original owning roots admitted by the central phase-current issuer.
#pragma once
#include "read_stamp.hpp"
namespace hanmo::resident {
struct WorldBundle;
// Identity and occurrence checks only. Full business closure remains the original
// owner validator; recursively validating a newly captured FSL input is invalid.
Status validate_native_actual_read_source(const WorldBundle&,const StableId&,
                                         const WorldMoment& actual_moment);
}
