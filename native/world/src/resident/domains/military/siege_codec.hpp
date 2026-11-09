// 围城冷边界逐字段保全原序与可选存在位；热态不保存BoundaryValue或作者原始字典。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "siege_model.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::siege_codec {
Status emit_fields(ProjectionWriter&,const SiegeRecord&);
Status emit_fields(ProjectionWriter&,const SiegeEvent&);
Status emit_fields(ProjectionWriter&,const SiegeResult&);
Status emit_fields(ProjectionWriter&,const SiegeBattleRequest&);
Status emit_fields(ProjectionWriter&,const SiegeEngineWork&);
Status emit_fields(ProjectionWriter&,const SiegeOrder&);
Status emit_fields(ProjectionWriter&,const SiegeEngineUse&);
Status emit_fields(ProjectionWriter&,const SiegeMaterial&);
BoundaryValue encode(const SiegeMaterial&);void decode(const BoundaryValue&,SiegeMaterial&,const StableId&);Status visit(CodecVisitor&,SiegeMaterial&);
BoundaryValue encode(const SiegeEngineUse&);void decode(const BoundaryValue&,SiegeEngineUse&,const StableId&);Status visit(CodecVisitor&,SiegeEngineUse&);
BoundaryValue encode(const SiegeOrder&);void decode(const BoundaryValue&,SiegeOrder&,const StableId&);Status visit(CodecVisitor&,SiegeOrder&);
BoundaryValue encode(const SiegeEngineWork&);void decode(const BoundaryValue&,SiegeEngineWork&,const StableId&);Status visit(CodecVisitor&,SiegeEngineWork&);
BoundaryValue encode(const SiegeBattleRequest&);void decode(const BoundaryValue&,SiegeBattleRequest&,const StableId&);Status visit(CodecVisitor&,SiegeBattleRequest&);
BoundaryValue encode(const SiegeResult&);void decode(const BoundaryValue&,SiegeResult&,const StableId&);Status visit(CodecVisitor&,SiegeResult&);
BoundaryValue encode(const SiegeEvent&);void decode(const BoundaryValue&,SiegeEvent&,const StableId&);Status visit(CodecVisitor&,SiegeEvent&);
BoundaryValue encode(const SiegeRecord&);void decode(const BoundaryValue&,SiegeRecord&,const StableId&);Status visit(CodecVisitor&,SiegeRecord&);
// 静态定义只在冷边界解码，随后验证战场、材料与器械交叉引用。
void decode(const BoundaryValue&,SiegeTopology&,const StableId&);
Outcome<std::vector<SiegeTopology>> decode_catalog(const BoundaryValue&,const MilitaryBattleCatalog&);
}
