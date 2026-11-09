#pragma once
#include "movement_exact_model.hpp"
namespace hanmo::resident {
// Predictions borrow the original Field rectangle and one actual motion root.
// Their source is not an original contact until installed with a Common head.
struct MovementBoundaryPrediction {
  CheckpointMemoryCharge retained_prediction;
  MovementBoundaryContact contact;
};
} // namespace hanmo::resident
