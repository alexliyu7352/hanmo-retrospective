// Every actual child of the original separate military_action_time/1 owner.
// Missing annex owners are not fabricated and still prevent full admission.
#pragma once
#include "training_exact_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_action_time_state(CodecVisitor&,MilitaryActionTimeState&,Integer selected_version);
Outcome<MilitaryActionTimeState> decode_military_action_time_state(const BoundaryValue&,Integer selected_version);
Outcome<BoundaryValue> encode_military_action_time_state(const MilitaryActionTimeState&,Integer selected_version);
Status emit_military_action_time_state(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected_version);
Status emit_military_action_time_state_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected_version);
}
