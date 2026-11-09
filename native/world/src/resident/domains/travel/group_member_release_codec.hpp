#pragma once
#include "group_movement.hpp"
namespace hanmo::resident {
class CodecVisitor;
class ProjectionWriter;
struct MovementRecordCodecSelection;
inline constexpr Integer movement_group_member_releases_codec_revision = 1;
// Explicit independent physical extension. These functions neither select it
// from table presence nor modify any original movement_exact/contact bytes.
Status visit_group_member_release_checkpoint(
    CodecVisitor &, GroupMovementExecutionState &, Integer selected_revision,
    const MovementRecordCodecSelection &);
Status emit_group_member_release_projection(
    ProjectionWriter &, std::string_view, const GroupMovementExecutionState &,
    Integer selected_revision, const MovementRecordCodecSelection &);
Outcome<BoundaryValue>
encode_group_member_releases(const GroupMovementExecutionState &,
                             Integer selected_revision,
                             const MovementRecordCodecSelection &);
Status decode_group_member_releases(const BoundaryValue &,
                                    GroupMovementExecutionState &,
                                    Integer selected_revision,
                                    const MovementRecordCodecSelection &);
} // namespace hanmo::resident
