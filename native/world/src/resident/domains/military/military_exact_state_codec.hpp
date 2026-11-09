// Every actual MilitaryExactState child, in original flattened owner order.
// This does not invent the other S2 section owners absent from this state.
#pragma once
#include "military_exact_sources.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Selected version must come from the original CatalogIdentity, never payload
// presence. /2 rejects /3 formations, missions and tagged local boundaries.
// The typed/JSON root begins with schema and requires that exact selected value.
Status visit_military_exact_state(CodecVisitor&,MilitaryExactState&,Integer selected_version);
Outcome<MilitaryExactState> decode_military_exact_state(const BoundaryValue&,Integer selected_version);
Outcome<BoundaryValue> encode_military_exact_state(const MilitaryExactState&,Integer selected_version);
Status emit_military_exact_state(ProjectionWriter&,std::string_view,const MilitaryExactState&,Integer selected_version);
Status emit_military_exact_state_fields(ProjectionWriter&,const MilitaryExactState&,Integer selected_version);
}
