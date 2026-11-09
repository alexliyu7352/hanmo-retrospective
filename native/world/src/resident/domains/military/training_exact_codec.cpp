#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "training_exact_codec.hpp"
#include "training_exact_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
namespace hanmo::resident {namespace {
template<class T>Status visit_row(CodecVisitor& c,T& row){try{return training_exact_fields::value(c,row);}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
template<class T>Outcome<T> decode_row(const BoundaryValue& input){return movement_detail::guard<T>([&]{try{T row;ActorFoundationJsonVisitor c(input);movement_detail::take(training_exact_fields::value(c,row));movement_detail::take(c.finish());return row;}catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit_row(ProjectionWriter& sink,std::string_view name,const T& row){try{return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,row,[](auto& c,auto& value){return training_exact_fields::value(c,value);});}catch(const exact_fields::CodecFailure& e){return {false,{},e.error};}}
template<class T>Outcome<BoundaryValue> encode_row(const T& row){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter writer(out);movement_detail::take(emit_row(writer,"",row));movement_detail::take(writer.finish());return out;});}
}
#define K4_TRAINING_ROW_CODEC(T,NAME) \
 Status visit_##NAME(CodecVisitor& c,T& v){return visit_row(c,v);} \
 Status emit_##NAME(ProjectionWriter& w,std::string_view n,const T& v){return emit_row(w,n,v);} \
 Outcome<BoundaryValue> encode_##NAME(const T& v){return encode_row(v);} \
 Outcome<T> decode_##NAME(const BoundaryValue& v){return decode_row<T>(v);}
K4_TRAINING_ROW_CODEC(DynamicMilitaryTrainingFact2,exact_military_training_fact)
K4_TRAINING_ROW_CODEC(TrainingParticipationSpan2,exact_training_participation_span)
K4_TRAINING_ROW_CODEC(TrainingPeriodWitness2,exact_training_period_witness)
K4_TRAINING_ROW_CODEC(TrainingDrillFreezeSource2,exact_training_drill_freeze)
#undef K4_TRAINING_ROW_CODEC
}
