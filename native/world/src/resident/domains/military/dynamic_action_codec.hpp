#pragma once
#include "dynamic_action.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include <string_view>
namespace hanmo::resident {
class ScopedCheckpointJsonDecode;
// Raw scenario.dynamic_military.timings dictionary; central owns explicit
// profile selection and original catalog validation. No inferred capability.
Outcome<DynamicMilitaryActionCatalog2> decode_dynamic_military_action_catalog2(const BoundaryValue& timings);
Outcome<DynamicMilitaryActionCatalog2> decode_dynamic_military_action_catalog2(const BoundaryValue& timings,ScopedCheckpointJsonDecode&);

Status visit_dynamic_military_action(CodecVisitor&,DynamicMilitaryAction2&);
Status visit_military_action_timing_definition2(CodecVisitor&,MilitaryActionTimingDefinition2&);
Status visit_dynamic_military_action_payloads(CodecVisitor&,MilitaryExactState&);
Outcome<DynamicMilitaryAction2> decode_dynamic_military_action(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_military_action(const DynamicMilitaryAction2&);
Status emit_dynamic_military_action(ProjectionWriter&,std::string_view,const DynamicMilitaryAction2&);
Outcome<MilitaryActionTimingDefinition2> decode_military_action_timing_definition2(const BoundaryValue&);
Status emit_military_action_timing_definition2(ProjectionWriter&,std::string_view,const MilitaryActionTimingDefinition2&);
}
