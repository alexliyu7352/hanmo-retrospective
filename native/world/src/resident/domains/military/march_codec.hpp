// 行军执行及永久条件回执的固定冷schema；热规则只消费TerrainExecution，不存JSON计划副本。
#pragma once
#include "resident/checkpoint/projection_writer.hpp"
#include "training_codec.hpp"
namespace hanmo::resident::training_codec { Status emit_fields(ProjectionWriter&,const MilitaryMarchFact&); }
namespace hanmo::resident::march_codec {
Status emit_fact_fields(ProjectionWriter&,const MilitaryState&);
bool decode_fact(const StableId&,const BoundaryValue&,const StableId&,MilitaryState&);
void append_state(BoundaryValue::Dict& facts,const MilitaryState&);
Status visit_state(CodecVisitor&,MilitaryState&);
}
