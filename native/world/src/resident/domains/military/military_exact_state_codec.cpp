#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_exact_state_codec.hpp"
#include "military_exact_state_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "military_state_codec_archive.hpp"
namespace hanmo::resident {namespace {
struct ExactKeys {
 template<class T>static constexpr bool owns=
  std::is_same_v<T,MilitaryTacticalFormation3>||std::is_same_v<T,MilitaryTacticalMissionRecord3>||
  std::is_same_v<T,DynamicMilitaryAction2>||std::is_same_v<T,DynamicMilitarySessionCurrent2>||
  std::is_same_v<T,DynamicSessionStateSource2>||std::is_same_v<T,LocalHexMotionBasis2>||
  std::is_same_v<T,LocalMotionCursor2>||std::is_same_v<T,LocalMotionBoundarySource>||
  std::is_same_v<T,MilitarySpatialChart2>||std::is_same_v<T,MilitarySpatialHandoff2>||
  std::is_same_v<T,DynamicBattleEffectGroup2>||std::is_same_v<T,DynamicMilitaryBattleFact2>||
  std::is_same_v<T,DynamicMilitaryCasualtyFact2>||std::is_same_v<T,MilitarySupplyEffectSource2>||
  std::is_same_v<T,DynamicMilitaryObjectiveEvidence2>;
 template<class T>bool operator()(const StableId& key,const T& row)const{return !key.empty()&&key==row.id;}
 bool operator()(const StableId& key,const MilitaryTacticalFormation3& row)const{return !key.empty()&&key==row.formation_id;}
 bool operator()(const StableId& key,const DynamicMilitarySessionCurrent2& row)const{return !key.empty()&&dynamic_military_session_key_matches(key,row.session_ref,row.generation);}
 bool operator()(const StableId& key,const LocalMotionCursor2& row)const{return !key.empty()&&military_local_body_key_matches(key,row.local_body_ref);}
 bool operator()(const StableId& key,const LocalMotionBoundarySource& row)const{return !key.empty()&&key==local_motion_boundary_id(row);}
 bool operator()(const StableId& key,const MilitarySupplyEffectSource2& row)const{return !key.empty()&&key==row.original_supply_fact_ref.id;}
};
}

Status visit_military_exact_state(CodecVisitor& c,MilitaryExactState& state,Integer selected){
 return movement_detail::status([&]{try{military_state_codec::RootArchive<CodecVisitor,ExactKeys> root{c};movement_detail::take(military_exact_state_fields::value(root,state,selected));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<MilitaryExactState> decode_military_exact_state(const BoundaryValue& input,Integer selected){
 return movement_detail::guard<MilitaryExactState>([&]{try{if(auto* budget=ScopedCheckpointJsonDecode::current())budget->default_values<MilitaryExactState>();MilitaryExactState state;ActorFoundationJsonVisitor c(input);
  military_state_codec::RootArchive<ActorFoundationJsonVisitor,ExactKeys> root{c};
  movement_detail::take(military_exact_state_fields::value(root,state,selected));movement_detail::take(c.finish());return state;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_exact_state_fields(ProjectionWriter& sink,const MilitaryExactState& state,Integer selected){
 return movement_detail::status([&]{try{ActorFoundationProjectionArchive original(sink);
  military_state_codec::RootArchive<ActorFoundationProjectionArchive,ExactKeys> root{original};
  movement_detail::take(military_exact_state_fields::value(root,const_cast<MilitaryExactState&>(state),selected));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_military_exact_state(ProjectionWriter& sink,std::string_view name,const MilitaryExactState& state,Integer selected){
 return sink.object(name,[&](ProjectionWriter& fields){return emit_military_exact_state_fields(fields,state,selected);});
}
Outcome<BoundaryValue> encode_military_exact_state(const MilitaryExactState& state,Integer selected){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);
  movement_detail::take(emit_military_exact_state(sink,"",state,selected));movement_detail::take(sink.finish());return out;});
}
}
