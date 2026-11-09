#pragma once
#include "military_battle_lifecycle.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
// Sole selected physical adjunct. Temporary keyed projection attaches to the
// original MilitaryBattleRecord, never another durable World/state table.
Status visit_military_battle_lifecycle(CodecVisitor&,MilitaryState&,Integer selected);
Status decode_military_battle_lifecycle(const BoundaryValue&,MilitaryState&,Integer selected);
Status emit_military_battle_lifecycle_fields(ProjectionWriter&,const MilitaryState&,Integer selected);
Status emit_military_battle_lifecycle(ProjectionWriter&,std::string_view,const MilitaryState&,Integer selected);
Outcome<BoundaryValue> encode_military_battle_lifecycle(const MilitaryState&,Integer selected);
Status collect_military_battle_lifecycle_pins(const CommonStore&,const MilitaryState&,Integer selected,const WorldMoment& now,SourcePinSink&);
Status validate_military_battle_lifecycle_pin(const CommonStore&,const MilitaryState&,Integer selected,const WorldMoment& now,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> military_battle_lifecycle_consumer_slots();
Outcome<bool> military_battle_lifecycle_source_may_release(const CommonStore&,const MilitaryState&,Integer selected,const WorldMoment& now,const SourceRef&,Integer source_schema);
}
