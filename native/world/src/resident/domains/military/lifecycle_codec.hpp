#include "resident/domains/military/military_command_contract.hpp"
// 军令收尾/营地装卸的唯一冷边界；热候选只保存typed来源与预约，不引入另一个资产世界。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {class ScopedCheckpointJsonDecode;}
namespace hanmo::resident::lifecycle_codec {
Outcome<MilitaryLogisticsTerms> decode_logistics_terms(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Outcome<MilitaryLogisticsResult> decode_logistics_result(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Outcome<MilitaryReturnAuthority> decode_return_authority(const BoundaryValue&,ScopedCheckpointJsonDecode&);

// Original goods dictionary/sequence, reused by exact Return/2 without a
// second field table or a BoundaryValue intermediate during projection.
Outcome<std::vector<std::pair<StableId,Integer>>> decode_return_goods(const BoundaryValue&);
Outcome<std::vector<std::pair<StableId,Integer>>> decode_return_goods(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Status visit_return_goods(CodecVisitor&,std::vector<std::pair<StableId,Integer>>&);
Status emit_return_goods_fields(ProjectionWriter&,const std::vector<std::pair<StableId,Integer>>&);
Status emit_fields(ProjectionWriter&,const MilitaryLogisticsAction&);
Status emit_fields(ProjectionWriter&,const MilitaryLogisticsFact&);
Status emit_fact_fields(ProjectionWriter&,const MilitaryState&);
Status emit_action_payload_fields(ProjectionWriter&,const MilitaryState&);

Status emit_fields(ProjectionWriter&,const CampaignSettlementFact&);
Status emit_fields(ProjectionWriter&,const MilitaryDeliveryFact&);
Status emit_fields(ProjectionWriter&,const MilitaryResignationFact&);
Status emit_fields(ProjectionWriter&,const MilitaryMusterFact&);
Status emit_fields(ProjectionWriter&,const MilitaryReturnFact&);
Status emit_fields(ProjectionWriter&,const MilitaryLogisticsResult&);
Status emit_fields(ProjectionWriter&,const MilitaryLogisticsTerms&);
Status emit_fields(ProjectionWriter&,const CampaignObjectiveResult&);
Status emit_fields(ProjectionWriter&,const MilitaryReturnAuthority&);
BoundaryValue encode(const MilitaryReturnAuthority&);void decode(const BoundaryValue&,MilitaryReturnAuthority&,const StableId&);Status visit(CodecVisitor&,MilitaryReturnAuthority&);
BoundaryValue encode(const CampaignObjectiveResult&);void decode(const BoundaryValue&,CampaignObjectiveResult&,const StableId&);Status visit(CodecVisitor&,CampaignObjectiveResult&);
BoundaryValue encode(const MilitaryLogisticsTerms&);void decode(const BoundaryValue&,MilitaryLogisticsTerms&,const StableId&);Status visit(CodecVisitor&,MilitaryLogisticsTerms&);
BoundaryValue encode(const MilitaryLogisticsResult&);void decode(const BoundaryValue&,MilitaryLogisticsResult&,const StableId&);Status visit(CodecVisitor&,MilitaryLogisticsResult&);
BoundaryValue encode(const MilitaryReturnFact&);void decode(const BoundaryValue&,MilitaryReturnFact&,const StableId&);Status visit(CodecVisitor&,MilitaryReturnFact&);
BoundaryValue encode(const MilitaryMusterFact&);void decode(const BoundaryValue&,MilitaryMusterFact&,const StableId&);Status visit(CodecVisitor&,MilitaryMusterFact&);
BoundaryValue encode(const MilitaryResignationFact&);void decode(const BoundaryValue&,MilitaryResignationFact&,const StableId&);Status visit(CodecVisitor&,MilitaryResignationFact&);
BoundaryValue encode(const MilitaryDeliveryFact&);void decode(const BoundaryValue&,MilitaryDeliveryFact&,const StableId&);Status visit(CodecVisitor&,MilitaryDeliveryFact&);
BoundaryValue encode(const CampaignSettlementFact&);void decode(const BoundaryValue&,CampaignSettlementFact&,const StableId&);Status visit(CodecVisitor&,CampaignSettlementFact&);
bool command_supported(const StableId&);
Outcome<MilitaryCommand> decode_command(const StableId&,const BoundaryValue&);
Outcome<BoundaryValue> encode_quote(const MilitaryQuote&,const MilitaryCommand&);
bool decode_fact(const StableId& kind,const BoundaryValue&,const StableId& id,MilitaryState&);
bool decode_action(const StableId& kind,const BoundaryValue&,const StableId& id,MilitaryState&);
void append_state(BoundaryValue::Dict& actions,BoundaryValue::Dict& facts,const MilitaryState&);
Status visit_state(CodecVisitor&,MilitaryState&);
}
