#pragma once
#include "military_march_contract.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_group_order_acceptance(CodecVisitor&,MilitaryGroupOrderAcceptance&);
Outcome<MilitaryGroupOrderAcceptance> decode_military_group_order_acceptance(const BoundaryValue&);
Status emit_military_group_order_acceptance_fields(ProjectionWriter&,const MilitaryGroupOrderAcceptance&);
Status emit_military_group_order_acceptance(ProjectionWriter&,std::string_view,const MilitaryGroupOrderAcceptance&);
Outcome<BoundaryValue> encode_military_group_order_acceptance(const MilitaryGroupOrderAcceptance&);
}
