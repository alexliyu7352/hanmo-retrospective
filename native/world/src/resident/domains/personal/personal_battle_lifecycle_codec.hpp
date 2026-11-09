#pragma once
#include "personal_battle_lifecycle.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/kernel/source_retention.hpp"
namespace hanmo::resident {
// Sole selected adjunct. The physical personal/1 codec still owns its original
// two tables; these rows are only a temporary projection keyed by battle ID.
Status visit_personal_battle_lifecycle(CodecVisitor&,PersonalState&,Integer selected);
Status decode_personal_battle_lifecycle(const BoundaryValue&,PersonalState&,Integer selected);
Status emit_personal_battle_lifecycle_fields(ProjectionWriter&,const PersonalState&,Integer selected);
Status emit_personal_battle_lifecycle(ProjectionWriter&,std::string_view,const PersonalState&,Integer selected);
Outcome<BoundaryValue> encode_personal_battle_lifecycle(const PersonalState&,Integer selected);
Status collect_personal_battle_lifecycle_pins(const CommonStore&,const PersonalState&,Integer selected,const WorldMoment& now,SourcePinSink&);
Status validate_personal_battle_lifecycle_pin(const CommonStore&,const PersonalState&,Integer selected,const WorldMoment& now,const SourcePin&);
std::vector<RequiredSourceConsumerSlot> personal_battle_lifecycle_consumer_slots();
Outcome<bool> personal_battle_lifecycle_source_may_release(const CommonStore&,const PersonalState&,Integer selected,const WorldMoment& now,const SourceRef&,Integer source_schema);
}
