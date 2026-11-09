#pragma once
#include "military_local_motion.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_local_hex_motion_basis(CodecVisitor&,LocalHexMotionBasis2&);
Status visit_local_motion_cursor(CodecVisitor&,LocalMotionCursor2&);
Status visit_local_motion_prefix_close(CodecVisitor&,LocalMotionPrefixClose2&);
Outcome<LocalHexMotionBasis2> decode_local_hex_motion_basis(const BoundaryValue&);
Outcome<LocalMotionCursor2> decode_local_motion_cursor(const BoundaryValue&);
Outcome<LocalMotionPrefixClose2> decode_local_motion_prefix_close(const BoundaryValue&);
Outcome<BoundaryValue> encode_local_hex_motion_basis(const LocalHexMotionBasis2&);
Outcome<BoundaryValue> encode_local_motion_cursor(const LocalMotionCursor2&);
Outcome<BoundaryValue> encode_local_motion_prefix_close(const LocalMotionPrefixClose2&);
Status emit_local_hex_motion_basis(ProjectionWriter&,std::string_view,const LocalHexMotionBasis2&);
Status emit_local_motion_cursor(ProjectionWriter&,std::string_view,const LocalMotionCursor2&);
Status emit_local_motion_prefix_close(ProjectionWriter&,std::string_view,const LocalMotionPrefixClose2&);
Status emit_local_hex_motion_basis_fields(ProjectionWriter&,const LocalHexMotionBasis2&);
Status emit_local_motion_cursor_fields(ProjectionWriter&,const LocalMotionCursor2&);
Status emit_local_motion_prefix_close_fields(ProjectionWriter&,const LocalMotionPrefixClose2&);
// The selected version is explicit. /2 emits only the old original payload,
// without a discriminant; /3 emits the reviewed tag/value union at this slot.
Status visit_local_motion_boundary_source(CodecVisitor&,LocalMotionBoundarySource&,Integer selected_local_schema);
Outcome<LocalMotionBoundarySource> decode_local_motion_boundary_source(const BoundaryValue&,Integer selected_local_schema);
Outcome<BoundaryValue> encode_local_motion_boundary_source(const LocalMotionBoundarySource&,Integer selected_local_schema);
Status emit_local_motion_boundary_source(ProjectionWriter&,std::string_view,const LocalMotionBoundarySource&,Integer selected_local_schema);
Status emit_local_motion_boundary_source_fields(ProjectionWriter&,const LocalMotionBoundarySource&,Integer selected_local_schema);
}
