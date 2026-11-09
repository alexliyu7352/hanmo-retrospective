// 恢复独立序列仅在Military8特性位存在时读写；旧1..7不追加空段。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "military_recovery_model.hpp"
#include "military_recovery_profile.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/ordered_value.hpp"
namespace hanmo::resident::military_recovery_codec {
Status emit_fields(ProjectionWriter&,const MilitaryRecoveryFact&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitaryBattleReleaseFact&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitarySiegeReleaseFact&,bool apply_order=true);
Status emit_fields(ProjectionWriter&,const MilitaryRecoveryState&);

// 原Common facts薄导出只追加kind；内部fact字段序仍由同一codec保留。
BoundaryValue encode_siege_release(const MilitarySiegeReleaseFact&);
Outcome<MilitarySiegeReleaseFact> decode_siege_release(const BoundaryValue&);
BoundaryValue encode_release(const MilitaryBattleReleaseFact&);
Outcome<MilitaryBattleReleaseFact> decode_release(const BoundaryValue&);
BoundaryValue encode_fact(const MilitaryRecoveryFact&);
Outcome<MilitaryRecoveryFact> decode_fact(const BoundaryValue&);
BoundaryValue encode(const MilitaryRecoveryState&);
Outcome<MilitaryRecoveryState> decode(const BoundaryValue&);
Status visit(CodecVisitor&,MilitaryRecoveryState&);
}
