#pragma once
#include "training_exact_model.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
Status validate_military_formation_reservation_layout(const MilitaryActionTimeState&,Integer selected);
Status validate_military_formation_reservation_sources(const CommonStore&,const MilitaryCatalog&,const MilitaryActionTimeState&,Integer selected);
Status visit_military_formation_reservations(CodecVisitor&,MilitaryActionTimeState&,Integer selected);
Status decode_military_formation_reservations(const BoundaryValue&,MilitaryActionTimeState&,Integer selected,bool typed,const CommonResourceCodecContext&);
Status emit_military_formation_reservations_fields(ProjectionWriter&,const MilitaryActionTimeState&,Integer selected,bool typed,const CommonResourceCodecContext&);
Status emit_military_formation_reservations(ProjectionWriter&,std::string_view,const MilitaryActionTimeState&,Integer selected,bool typed,const CommonResourceCodecContext&);
Outcome<BoundaryValue> encode_military_formation_reservations(const MilitaryActionTimeState&,Integer selected,bool typed,const CommonResourceCodecContext&);
Status collect_military_formation_reservation_pins(const CommonStore&,const MilitaryCatalog&,const MilitaryActionTimeState&,SourcePinSink&,Integer selected=1);
Status validate_military_formation_reservation_pin(const CommonStore&,const MilitaryCatalog&,const MilitaryActionTimeState&,const SourcePin&,Integer selected=1);
std::vector<RequiredSourceConsumerSlot> military_formation_reservation_consumer_slots();
Outcome<bool> military_formation_reservation_source_may_release(const CommonStore&,const MilitaryCatalog&,const MilitaryActionTimeState&,const SourceRef&,Integer source_schema,Integer selected=1);
}
