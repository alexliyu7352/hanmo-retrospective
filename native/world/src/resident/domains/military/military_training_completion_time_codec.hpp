#pragma once
#include "military_training_completion_time.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Explicit physical /1: schema, training_completions. Attach only after the
// original military_action_time owner decoded; no second durable result table.
Status visit_military_training_completion_time(CodecVisitor&,MilitaryActionTimeState&,Integer selected_version);
Status decode_military_training_completion_time(const BoundaryValue&,MilitaryActionTimeState&,Integer selected_version);
Status emit_military_training_completion_time_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected_version);
Status emit_military_training_completion_time(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected_version);
Outcome<BoundaryValue> encode_military_training_completion_time(const MilitaryActionTimeState&,Integer selected_version);
}
