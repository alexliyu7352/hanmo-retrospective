#pragma once
#include <string_view>
#include "dynamic_effect_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct MilitaryExactState;
Status visit_dynamic_military_effect_group(CodecVisitor&,DynamicBattleEffectGroup2&);
Status visit_dynamic_military_effect_groups(CodecVisitor&,MilitaryExactState&);
Outcome<DynamicBattleEffectGroup2> decode_dynamic_military_effect_group(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_effect_group(const DynamicBattleEffectGroup2&);
Status emit_dynamic_military_effect_group(ProjectionWriter&,std::string_view name,const DynamicBattleEffectGroup2&);
}
