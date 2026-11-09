#pragma once
#include "military_authority_time.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Invoke only after the original legacy MilitaryState owners have decoded.
// /1 requires both maps even when empty. No payload presence selects a module.
Status visit_military_authority_time(CodecVisitor&,MilitaryState&,Integer selected_version);
Status decode_military_authority_time(const BoundaryValue&,MilitaryState&,Integer selected_version);
Status emit_military_authority_time_fields(ProjectionWriter&,const MilitaryState&,Integer selected_version);
Status emit_military_authority_time(ProjectionWriter&,std::string_view,const MilitaryState&,Integer selected_version);
Outcome<BoundaryValue> encode_military_authority_time(const MilitaryState&,Integer selected_version);
}
