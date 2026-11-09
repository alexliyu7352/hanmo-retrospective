#pragma once
#include "military_action_time_records.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Source-row entrances only; the complete military_action_time/1 owner selects
// and validates all annex roots before any world can admit this layout.
#define K4_MILITARY_TIME_ROW_CODEC(T,NAME) \
 Status visit_##NAME(CodecVisitor&,T&); \
 Status emit_##NAME(ProjectionWriter&,std::string_view,const T&); \
 Outcome<BoundaryValue> encode_##NAME(const T&); \
 Outcome<T> decode_##NAME(const BoundaryValue&)
K4_MILITARY_TIME_ROW_CODEC(DynamicMilitaryActionFact2,exact_military_action_fact);
K4_MILITARY_TIME_ROW_CODEC(DynamicMilitaryLogisticsFact2,exact_military_logistics_fact);
K4_MILITARY_TIME_ROW_CODEC(DynamicMilitaryReturnFact2,exact_military_return_fact);
#undef K4_MILITARY_TIME_ROW_CODEC
}
