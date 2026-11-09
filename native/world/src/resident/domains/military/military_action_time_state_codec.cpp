#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_action_time_state_codec.hpp"
#include "military_action_time_state_codec_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {namespace {
struct ActionTimeKeys {
 template<class T>static constexpr bool owns=
  std::is_same_v<T,DynamicMilitaryActionFact2>||std::is_same_v<T,DynamicMilitaryLogisticsFact2>||
  std::is_same_v<T,DynamicMilitaryReturnFact2>||std::is_same_v<T,DynamicMilitaryTrainingFact2>||
  std::is_same_v<T,TrainingParticipationSpan2>||std::is_same_v<T,TrainingPeriodWitness2>||
  std::is_same_v<T,TrainingDrillFreezeSource2>||std::is_same_v<T,NewNavalRepairTimeSource2>;
 template<class T>bool operator()(const StableId& key,const T& row)const{return !key.empty()&&key==row.id;}
 // These original source rows deliberately retain identity in the table key.
 // Only the original Common/source reader can authenticate that allocated key;
 // codec reconstruction must not synthesize an id from action or period.
 bool operator()(const StableId& key,const TrainingParticipationSpan2&)const{return !key.empty();}
 bool operator()(const StableId& key,const TrainingPeriodWitness2&)const{return !key.empty();}
 bool operator()(const StableId& key,const TrainingDrillFreezeSource2&)const{return !key.empty();}
 bool operator()(const StableId& key,const NewNavalRepairTimeSource2& row)const{return !key.empty()&&key==row.timing.start_fact_ref.id&&key==row.timing.action_ref;}
};
}
Status visit_military_action_time_state(CodecVisitor& c,MilitaryActionTimeState& state,Integer selected){
 return movement_detail::status([&]{try{military_state_codec::RootArchive<CodecVisitor,ActionTimeKeys> root{c};
  movement_detail::take(military_action_time_state_fields::value(root,state,selected));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<MilitaryActionTimeState> decode_military_action_time_state(const BoundaryValue& input,Integer selected){
 return movement_detail::guard<MilitaryActionTimeState>([&]{try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<MilitaryActionTimeState>();MilitaryActionTimeState state;ActorFoundationJsonVisitor c(input);
  military_state_codec::RootArchive<ActorFoundationJsonVisitor,ActionTimeKeys> root{c};
  movement_detail::take(military_action_time_state_fields::value(root,state,selected));movement_detail::take(c.finish());return state;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_action_time_state_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected){
 return movement_detail::status([&]{try{ActorFoundationProjectionArchive original(sink);
  military_state_codec::RootArchive<ActorFoundationProjectionArchive,ActionTimeKeys> root{original};
  movement_detail::take(military_action_time_state_fields::value(root,const_cast<MilitaryActionTimeState&>(state),selected));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_action_time_state(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected){
 return sink.object(name,[&](ProjectionWriter& fields){return emit_military_action_time_state_fields(fields,state,selected);});
}
Outcome<BoundaryValue> encode_military_action_time_state(const MilitaryActionTimeState& state,Integer selected){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);
  movement_detail::take(emit_military_action_time_state(sink,"",state,selected));movement_detail::take(sink.finish());return out;});
}
}
