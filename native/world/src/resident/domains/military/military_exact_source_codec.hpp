// The three original source payload tables in S2 4.2/6.2. This is not the
// dynamic_military/2 section: its remaining selected roots are still required.
#pragma once
#include "military_exact_sources.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_dynamic_military_battle_fact(CodecVisitor&,DynamicMilitaryBattleFact2&);
Status visit_dynamic_military_casualty_fact(CodecVisitor&,DynamicMilitaryCasualtyFact2&);
Outcome<DynamicMilitaryBattleFact2> decode_dynamic_military_battle_fact(const BoundaryValue&);
Outcome<DynamicMilitaryCasualtyFact2> decode_dynamic_military_casualty_fact(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_battle_fact(const DynamicMilitaryBattleFact2&);
Outcome<BoundaryValue> encode_dynamic_military_casualty_fact(const DynamicMilitaryCasualtyFact2&);
Status emit_dynamic_military_battle_fact(ProjectionWriter&,std::string_view,const DynamicMilitaryBattleFact2&);
Status emit_dynamic_military_casualty_fact(ProjectionWriter&,std::string_view,const DynamicMilitaryCasualtyFact2&);
// Writes only fields into an already open object. No intermediate JSON tree.
Status emit_dynamic_military_battle_fact_fields(ProjectionWriter&,const DynamicMilitaryBattleFact2&);
Status emit_dynamic_military_casualty_fact_fields(ProjectionWriter&,const DynamicMilitaryCasualtyFact2&);
}
