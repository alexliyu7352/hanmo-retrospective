#pragma once
#include "military_exact_sources.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_exact_military_supply_source(CodecVisitor&,MilitarySupplyEffectSource2&);
Status emit_exact_military_supply_source(ProjectionWriter&,std::string_view,const MilitarySupplyEffectSource2&);
Outcome<BoundaryValue> encode_exact_military_supply_source(const MilitarySupplyEffectSource2&);
Outcome<MilitarySupplyEffectSource2> decode_exact_military_supply_source(const BoundaryValue&);
}
