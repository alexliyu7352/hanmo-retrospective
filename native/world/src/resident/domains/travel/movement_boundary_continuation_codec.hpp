#pragma once
#include "movement_exact_model.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
struct MovementRecordCodecSelection;
inline constexpr Integer movement_boundary_continuations_codec_revision = 1;
// Explicit independent physical extension. These functions neither select it
// from table presence nor modify any original movement_exact/contact bytes.
Status visit_movement_boundary_continuation_checkpoint(
    CodecVisitor &, MovementExactState &, Integer selected_revision,
    const MovementRecordCodecSelection &);
Status emit_movement_boundary_continuation_projection(
    ProjectionWriter &, std::string_view, const MovementExactState &,
    Integer selected_revision, const MovementRecordCodecSelection &);
Outcome<BoundaryValue>
encode_movement_boundary_continuations(const MovementExactState &,
                                       Integer selected_revision,
                                       const MovementRecordCodecSelection &);
Status decode_movement_boundary_continuations(
    const BoundaryValue &, MovementExactState &, Integer selected_revision,
    const MovementRecordCodecSelection &);
} // namespace hanmo::resident
