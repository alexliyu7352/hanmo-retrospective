// actual_contact_bindings原完整记录的JSON/shape叶；外层仍须全部原几何source冷验。
#pragma once
#include "movement_exact.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Outcome<ExactMovementContact> decode_exact_movement_contact(const BoundaryValue&);
Status emit_exact_movement_contact_projection(ProjectionWriter&,std::string_view,const ExactMovementContact&);
Outcome<BoundaryValue> export_exact_movement_contact(const ExactMovementContact&);
}
