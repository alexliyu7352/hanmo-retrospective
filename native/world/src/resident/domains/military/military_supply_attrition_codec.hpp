// Military9独立尾段；旧Military1..8不写空流失序列。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_supply_attrition_profile.hpp"
#include "military_supply_attrition_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_supply_attrition_codec {
Status emit_fields(ProjectionWriter&,const MilitarySupplyAttritionFact&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitarySupplyAttritionState&);

BoundaryValue encode_fact(const MilitarySupplyAttritionFact&);
Outcome<MilitarySupplyAttritionFact> decode_fact(const BoundaryValue&);
BoundaryValue encode(const MilitarySupplyAttritionState&);
Outcome<MilitarySupplyAttritionState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitarySupplyAttritionState&);
}
