// 三个原 archive 的基础值桥，保持原 object/optional 结构而不依赖事件子型。
#pragma once
#include "field_event_registry.hpp"
namespace hanmo::resident {
Status field_event_original_value(CodecVisitor &, FieldMission &);
Status field_event_original_value(CodecVisitor &, FieldBattleTime &);
Status field_event_original_value(ActorFoundationJsonVisitor &, FieldMission &);
Status field_event_original_value(ActorFoundationJsonVisitor &, FieldBattleTime &);
Status field_event_original_value(ActorFoundationProjectionArchive &, FieldMission &);
Status field_event_original_value(ActorFoundationProjectionArchive &, FieldBattleTime &);
}
