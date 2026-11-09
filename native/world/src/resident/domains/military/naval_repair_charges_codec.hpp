// 收费账仅Naval4追加，保旧Naval3工单和回执的字段顺序与字节。
#pragma once
#include "naval_repair_charges_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {class ScopedCheckpointJsonDecode;}
namespace hanmo::resident::naval_repair_charges_codec {
Status emit_fact_fields(ProjectionWriter&,const NavalRepairChargeFact&);
Outcome<NavalRepairChargeFact> decode_fact(const BoundaryValue&);
Outcome<NavalRepairChargeFact> decode_fact(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status visit_fact(CodecVisitor&,NavalRepairChargeFact&);
Status validate(const NavalRepairChargeState&);
Status emit_fields(ProjectionWriter&,const NavalRepairChargeState&);
BoundaryValue encode(const NavalRepairChargeState&);
BoundaryValue encode_terms(const NavalRepairChargeTerms&);
Outcome<NavalRepairChargeState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,NavalRepairChargeState&);
}
