// 原占领、解围和城防材料永久事实的唯一冷边界；严格字段与可选存在位均保留。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident::occupation_codec {
Status emit_fact_fields(ProjectionWriter&,const MilitaryState&,Integer selected_schema=1);

Status emit_fields(ProjectionWriter&,const SiegeAssetEffect&);

Status emit_fields(ProjectionWriter&,const SiegeMaterialFact&);
Status emit_fields(ProjectionWriter&,const MilitaryReliefFact&);
Status emit_fields(ProjectionWriter&,const MilitaryOccupationFact&,Integer selected_schema=1);
Status emit_fields(ProjectionWriter&,const OccupationCustodyTransfer&);
Status emit_fields(ProjectionWriter&,const OccupationAssetManifest&);
Status emit_fields(ProjectionWriter&,const OccupationInventoryManifest&);
Status emit_fields(ProjectionWriter&,const OccupationWalletManifest&);
Status emit_fields(ProjectionWriter&,const LotView&);
Status emit_fields(ProjectionWriter&,const OccupationPublicLot&);
BoundaryValue encode(const OccupationPublicLot&);void decode(const BoundaryValue&,OccupationPublicLot&,const StableId&);Status visit(CodecVisitor&,OccupationPublicLot&);
BoundaryValue encode(const LotView&);void decode(const BoundaryValue&,LotView&,const StableId&);Status visit(CodecVisitor&,LotView&);
BoundaryValue encode(const OccupationWalletManifest&);void decode(const BoundaryValue&,OccupationWalletManifest&,const StableId&);Status visit(CodecVisitor&,OccupationWalletManifest&);
BoundaryValue encode(const OccupationInventoryManifest&);void decode(const BoundaryValue&,OccupationInventoryManifest&,const StableId&);Status visit(CodecVisitor&,OccupationInventoryManifest&);
BoundaryValue encode(const OccupationAssetManifest&);void decode(const BoundaryValue&,OccupationAssetManifest&,const StableId&);Status visit(CodecVisitor&,OccupationAssetManifest&);
BoundaryValue encode(const OccupationCustodyTransfer&);void decode(const BoundaryValue&,OccupationCustodyTransfer&,const StableId&);Status visit(CodecVisitor&,OccupationCustodyTransfer&);
BoundaryValue encode(const MilitaryOccupationFact&,Integer selected_schema=1);void decode(const BoundaryValue&,MilitaryOccupationFact&,const StableId&,Integer selected_schema=1);Status visit(CodecVisitor&,MilitaryOccupationFact&,Integer selected_schema=1);
BoundaryValue encode(const MilitaryReliefFact&);void decode(const BoundaryValue&,MilitaryReliefFact&,const StableId&);Status visit(CodecVisitor&,MilitaryReliefFact&);
BoundaryValue encode(const SiegeMaterialFact&);void decode(const BoundaryValue&,SiegeMaterialFact&,const StableId&);Status visit(CodecVisitor&,SiegeMaterialFact&);
bool decode_fact(const StableId& kind,const BoundaryValue&,const StableId& id,MilitaryState&,Integer selected_schema=1);
void append_state(BoundaryValue::Dict& facts,const MilitaryState&,Integer selected_schema=1);
Status visit_state(CodecVisitor&,MilitaryState&,Integer selected_schema=1);
}
