#include "resident/domains/military/military_command_contract.hpp"
// 军阵题面/驻军训练逐字段冷边界，来源摘要复用共同canonical摘要；不存动态世界字典。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/military/military_refs.hpp"
#include "resident/checkpoint/ordered_value.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {class ScopedCheckpointJsonDecode;}
namespace hanmo::resident::training_codec {
Outcome<MilitaryTrainingTerms> decode_training_terms(const BoundaryValue&,ScopedCheckpointJsonDecode&);
Outcome<DrillState> decode_drill_state(const BoundaryValue&,ScopedCheckpointJsonDecode&);

Status emit_fact_fields(ProjectionWriter&,const MilitaryState&);
Status emit_action_payload_fields(ProjectionWriter&,const MilitaryState&);

Status emit_fields(ProjectionWriter&,const TrainingDrillInstance&);
Status emit_fields(ProjectionWriter&,const DrillState&);
Status emit_fields(ProjectionWriter&,const MilitaryMarchUnit&);
Status emit_fields(ProjectionWriter&,const MilitaryTrainingFact&);
Status emit_fields(ProjectionWriter&,const MilitaryTrainingLesson&);
Status emit_fields(ProjectionWriter&,const MilitaryTrainingTerms&);
Status emit_fields(ProjectionWriter&,const TrainingDrillTerms&);
Status emit_fields(ProjectionWriter&,const TrainingGroupSource&);
Status emit_fields(ProjectionWriter&,const DrillResult&);
Status emit_fields(ProjectionWriter&,const DrillObjectives&);
Status emit_fields(ProjectionWriter&,const DrillSnapshot&);
Status emit_fields(ProjectionWriter&,const DrillMove&);
Status emit_fields(ProjectionWriter&,const DrillPending&);
Status emit_fields(ProjectionWriter&,const MilitaryTrainingReceipt&);

BoundaryValue encode(const DrillPending&);void decode(const BoundaryValue&,DrillPending&,const StableId&);Status visit(CodecVisitor&,DrillPending&);
BoundaryValue encode(const DrillMove&);void decode(const BoundaryValue&,DrillMove&,const StableId&);Status visit(CodecVisitor&,DrillMove&);
BoundaryValue encode(const DrillSnapshot&);void decode(const BoundaryValue&,DrillSnapshot&,const StableId&);Status visit(CodecVisitor&,DrillSnapshot&);
BoundaryValue encode(const DrillObjectives&);void decode(const BoundaryValue&,DrillObjectives&,const StableId&);Status visit(CodecVisitor&,DrillObjectives&);
BoundaryValue encode(const DrillResult&);void decode(const BoundaryValue&,DrillResult&,const StableId&);Status visit(CodecVisitor&,DrillResult&);
BoundaryValue encode(const TrainingGroupSource&);void decode(const BoundaryValue&,TrainingGroupSource&,const StableId&);Status visit(CodecVisitor&,TrainingGroupSource&);
BoundaryValue encode(const TrainingDrillTerms&);void decode(const BoundaryValue&,TrainingDrillTerms&,const StableId&);Status visit(CodecVisitor&,TrainingDrillTerms&);
BoundaryValue encode(const MilitaryTrainingTerms&);void decode(const BoundaryValue&,MilitaryTrainingTerms&,const StableId&);Status visit(CodecVisitor&,MilitaryTrainingTerms&);
BoundaryValue encode(const MilitaryTrainingLesson&);void decode(const BoundaryValue&,MilitaryTrainingLesson&,const StableId&);Status visit(CodecVisitor&,MilitaryTrainingLesson&);
BoundaryValue encode(const MilitaryTrainingFact&);void decode(const BoundaryValue&,MilitaryTrainingFact&,const StableId&);Status visit(CodecVisitor&,MilitaryTrainingFact&);
BoundaryValue encode(const MilitaryTrainingReceipt&);void decode(const BoundaryValue&,MilitaryTrainingReceipt&,const StableId&);Status visit(CodecVisitor&,MilitaryTrainingReceipt&);
BoundaryValue encode(const MilitaryMarchUnit&);void decode(const BoundaryValue&,MilitaryMarchUnit&,const StableId&);Status visit(CodecVisitor&,MilitaryMarchUnit&);
BoundaryValue encode(const DrillState&);void decode(const BoundaryValue&,DrillState&,const StableId&);Status visit(CodecVisitor&,DrillState&);
BoundaryValue encode(const TrainingDrillInstance&);void decode(const BoundaryValue&,TrainingDrillInstance&,const StableId&);Status visit(CodecVisitor&,TrainingDrillInstance&);
BoundaryValue encode(const MilitaryMarchFact&);void decode(const BoundaryValue&,MilitaryMarchFact&,const StableId&);Status visit(CodecVisitor&,MilitaryMarchFact&);
Outcome<std::vector<DrillDefinition>> decode_drill_catalog(const BoundaryValue&);
StableId roster_digest(const std::vector<StableId>&);
bool command_supported(const StableId&);
Outcome<MilitaryCommand> decode_command(const StableId&,const BoundaryValue&);
Outcome<BoundaryValue> encode_quote(const MilitaryQuote&,const MilitaryCommand&);
bool decode_fact(const StableId&,const BoundaryValue&,const StableId&,MilitaryState&);
bool decode_action(const StableId&,const BoundaryValue&,const StableId&,MilitaryState&);
void append_state(BoundaryValue::Dict&,BoundaryValue::Dict&,const MilitaryState&);
Status visit_state(CodecVisitor&,MilitaryState&);
}
