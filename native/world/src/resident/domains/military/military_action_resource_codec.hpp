#pragma once
#include "training_exact_model.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/kernel/source_retention.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// /1 physical adjunct follows original ActionTime decode. Mandatory maps, no
// second durable state. Common frame representation is explicitly selected.
Status visit_military_action_resources(CodecVisitor&,MilitaryActionTimeState&,Integer selected_version);
Status decode_military_action_resources(const BoundaryValue&,MilitaryActionTimeState&,Integer selected_version,bool typed,const CommonResourceCodecContext&);
Status emit_military_action_resources_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected_version,bool typed,const CommonResourceCodecContext&);
Status emit_military_action_resources(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected_version,bool typed,const CommonResourceCodecContext&);
Outcome<BoundaryValue> encode_military_action_resources(const MilitaryActionTimeState&,Integer selected_version,bool typed,const CommonResourceCodecContext&);
Status validate_military_action_resource_sources(const CommonStore&,const MilitaryActionTimeState&,Integer selected_version);
Status collect_military_action_resource_pins(const CommonStore&,const MilitaryActionTimeState&,SourcePinSink&);
Status validate_military_action_resource_pin(const CommonStore&,const MilitaryActionTimeState&,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> military_action_resource_consumer_slots();
Outcome<bool> military_action_resource_source_may_release(const CommonStore&,const MilitaryActionTimeState&,const SourceRef&,Integer source_schema);
}
