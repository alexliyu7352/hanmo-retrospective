// 军事会话冷边界逐字段保全；仅在导入/导出/检查点使用，不把BoundaryValue留在热状态。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {class ScopedCheckpointJsonDecode;}
namespace hanmo::resident::battle_codec {
Outcome<MilitaryActorChange> decode_actor_change(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status emit_fields(ProjectionWriter&,const MilitaryContactFact&);
Status emit_fields(ProjectionWriter&,const MilitaryBattleRecord&);
Status emit_fields(ProjectionWriter&,const MilitaryBattleFact&);
Status emit_fields(ProjectionWriter&,const MilitaryCasualtyFact&);
Status emit_fields(ProjectionWriter&,const MilitaryRosterReceipt&);
Status emit_fields(ProjectionWriter&,const MilitaryMotionSource&);
Status emit_fields(ProjectionWriter&,const MilitaryBattleEvent&);
Status emit_fields(ProjectionWriter&,const MilitaryBattleResult&);
Status emit_fields(ProjectionWriter&,const MilitaryObjectiveProgress&);
Status emit_fields(ProjectionWriter&,const BattleUnit&);
Status emit_fields(ProjectionWriter&,const MilitaryOrder&);

Status emit_fields(ProjectionWriter&,const MilitaryActorChange&);

BoundaryValue encode(const MilitaryOrder&);void decode(const BoundaryValue&,MilitaryOrder&,const StableId&);Status visit(CodecVisitor&,MilitaryOrder&);
BoundaryValue encode(const BattleUnit&);void decode(const BoundaryValue&,BattleUnit&,const StableId&);Status visit(CodecVisitor&,BattleUnit&);
BoundaryValue encode(const MilitaryObjectiveProgress&);void decode(const BoundaryValue&,MilitaryObjectiveProgress&,const StableId&);Status visit(CodecVisitor&,MilitaryObjectiveProgress&);
BoundaryValue encode(const MilitaryBattleResult&);void decode(const BoundaryValue&,MilitaryBattleResult&,const StableId&);Status visit(CodecVisitor&,MilitaryBattleResult&);
BoundaryValue encode(const MilitaryBattleEvent&);void decode(const BoundaryValue&,MilitaryBattleEvent&,const StableId&);Status visit(CodecVisitor&,MilitaryBattleEvent&);
BoundaryValue encode(const MilitaryMotionSource&);void decode(const BoundaryValue&,MilitaryMotionSource&,const StableId&);Status visit(CodecVisitor&,MilitaryMotionSource&);
BoundaryValue encode(const MilitaryActorChange&);void decode(const BoundaryValue&,MilitaryActorChange&,const StableId&);Status visit(CodecVisitor&,MilitaryActorChange&);
BoundaryValue encode(const MilitaryRosterReceipt&);void decode(const BoundaryValue&,MilitaryRosterReceipt&,const StableId&);Status visit(CodecVisitor&,MilitaryRosterReceipt&);
BoundaryValue encode(const MilitaryCasualtyFact&);void decode(const BoundaryValue&,MilitaryCasualtyFact&,const StableId&);Status visit(CodecVisitor&,MilitaryCasualtyFact&);
BoundaryValue encode(const MilitaryBattleFact&);void decode(const BoundaryValue&,MilitaryBattleFact&,const StableId&);Status visit(CodecVisitor&,MilitaryBattleFact&);
BoundaryValue encode(const MilitaryBattleRecord&);void decode(const BoundaryValue&,MilitaryBattleRecord&,const StableId&);Status visit(CodecVisitor&,MilitaryBattleRecord&);
BoundaryValue encode(const MilitaryContactFact&);void decode(const BoundaryValue&,MilitaryContactFact&,const StableId&);Status visit(CodecVisitor&,MilitaryContactFact&);
Outcome<MilitaryBattleCatalog> decode_catalog(const BoundaryValue&);
}
