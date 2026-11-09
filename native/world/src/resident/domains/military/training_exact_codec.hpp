#pragma once
#include "training_exact_model.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
#include "resident/checkpoint/projection_writer.hpp"
namespace hanmo::resident {
// Same original row fields in every representation. These row entrances do not
// register a partial military_action_time/1 section or initialize missing kinds.
#define K4_TRAINING_ROW_CODEC(T,NAME) \
 Status visit_##NAME(CodecVisitor&,T&); \
 Status emit_##NAME(ProjectionWriter&,std::string_view,const T&); \
 Outcome<BoundaryValue> encode_##NAME(const T&); \
 Outcome<T> decode_##NAME(const BoundaryValue&)
K4_TRAINING_ROW_CODEC(DynamicMilitaryTrainingFact2,exact_military_training_fact);
K4_TRAINING_ROW_CODEC(TrainingParticipationSpan2,exact_training_participation_span);
K4_TRAINING_ROW_CODEC(TrainingPeriodWitness2,exact_training_period_witness);
K4_TRAINING_ROW_CODEC(TrainingDrillFreezeSource2,exact_training_drill_freeze);
#undef K4_TRAINING_ROW_CODEC
}
