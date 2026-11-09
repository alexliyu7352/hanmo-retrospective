#pragma once
#include "training_exact_model.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/kernel/source_retention.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
Status visit_military_return_detachments(CodecVisitor&,MilitaryActionTimeState&,Integer selected_version);
Status decode_military_return_detachments(const BoundaryValue&,MilitaryActionTimeState&,Integer selected_version);
Status emit_military_return_detachments_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected_version);
Status emit_military_return_detachments(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected_version);
Outcome<BoundaryValue> encode_military_return_detachments(const MilitaryActionTimeState&,Integer selected_version);
// Layout and source-local checks prove selected payload coverage, original receipt/application binding and the original Estate conditional effect only. Ordered immutable-pledge correspondence remains in the full return retainer proof; these checks do not replace acceptance, service, Estate authority or whole-world retention closure.
Status validate_military_return_detachment_layout(const MilitaryActionTimeState&,Integer selected_version);
Status validate_military_return_detachment_sources(const CommonStore&,const MilitaryActionTimeState&,Integer selected_version);
Status collect_military_return_detachment_pins(const CommonStore&,const MilitaryActionTimeState&,SourcePinSink&);
Status validate_military_return_detachment_pin(const CommonStore&,const MilitaryActionTimeState&,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> military_return_detachment_consumer_slots();
Outcome<bool> military_return_detachment_source_may_release(const CommonStore&,const MilitaryActionTimeState&,const SourceRef&,Integer source_schema);
}
