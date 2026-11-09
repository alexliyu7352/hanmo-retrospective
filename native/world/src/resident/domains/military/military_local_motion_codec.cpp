#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_local_motion_codec.hpp"
#include "military_local_motion_codec_fields.hpp"
#include "military_local_boundary_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "military_state_codec_archive.hpp"
namespace hanmo::resident {namespace {
struct OriginalLocalMotionKeys {
 template<class T>static constexpr bool owns=std::is_same_v<T,LocalHexMotionBasis2>||
  std::is_same_v<T,LocalMotionCursor2>||std::is_same_v<T,LocalMotionBoundarySource>;
 bool operator()(const StableId& key,const LocalHexMotionBasis2& row)const{return !key.empty()&&key==row.id;}
 bool operator()(const StableId& key,const LocalMotionBoundarySource& row)const{return !key.empty()&&key==local_motion_boundary_id(row);}
 bool operator()(const StableId& key,const LocalMotionCursor2& row)const{return !key.empty()&&military_local_body_key_matches(key,row.local_body_ref);}
};
template<class T>Status table(CodecVisitor& c,std::string_view name,OrderedTable<T>& rows,Integer selected=2){
 military_state_codec::RootArchive<CodecVisitor,OriginalLocalMotionKeys> root{c,"LocalMotionPayloadKeyMismatch","LocalMotionPayloadRootAlreadyPopulated"};
 return root.table(name,rows,[&](auto& body,auto& row){
  if constexpr(std::is_same_v<T,LocalMotionBoundarySource>)return military_local_boundary_fields::selected(body,row,selected);
  else return military_local_motion_fields::value(body,row);
 });
}
template<class T>Status visit(CodecVisitor& c,T& row){return movement_detail::status([&]{try{
 movement_detail::take(military_local_motion_fields::value(c,row));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Outcome<T> decode(const BoundaryValue& raw){return movement_detail::guard<T>([&]{try{
 ActorFoundationJsonVisitor c(raw);movement_detail::take(prepare_checkpoint_default_values<T>(c));T row;movement_detail::take(military_local_motion_fields::value(c,row));movement_detail::take(c.finish());return row;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status fields(ProjectionWriter& sink,const T& row){return movement_detail::status([&]{try{
 ActorFoundationProjectionArchive c(sink);movement_detail::take(military_local_motion_fields::value(c,const_cast<T&>(row)));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit(ProjectionWriter& sink,std::string_view name,const T& row){return sink.object(name,[&](ProjectionWriter& body){return fields(body,row);});}
template<class T>Outcome<BoundaryValue> encode(const T& row){return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);movement_detail::take(emit(sink,"",row));movement_detail::take(sink.finish());return out;});}
}
Status visit_local_hex_motion_basis(CodecVisitor& c,LocalHexMotionBasis2& v){return visit(c,v);}
Status visit_local_motion_cursor(CodecVisitor& c,LocalMotionCursor2& v){return visit(c,v);}
Status visit_local_motion_prefix_close(CodecVisitor& c,LocalMotionPrefixClose2& v){return visit(c,v);}
Outcome<LocalHexMotionBasis2> decode_local_hex_motion_basis(const BoundaryValue& v){return decode<LocalHexMotionBasis2>(v);}
Outcome<LocalMotionCursor2> decode_local_motion_cursor(const BoundaryValue& v){return decode<LocalMotionCursor2>(v);}
Outcome<LocalMotionPrefixClose2> decode_local_motion_prefix_close(const BoundaryValue& v){return decode<LocalMotionPrefixClose2>(v);}
Outcome<BoundaryValue> encode_local_hex_motion_basis(const LocalHexMotionBasis2& v){return encode(v);}
Outcome<BoundaryValue> encode_local_motion_cursor(const LocalMotionCursor2& v){return encode(v);}
Outcome<BoundaryValue> encode_local_motion_prefix_close(const LocalMotionPrefixClose2& v){return encode(v);}
Status emit_local_hex_motion_basis(ProjectionWriter& w,std::string_view n,const LocalHexMotionBasis2& v){return emit(w,n,v);}
Status emit_local_motion_cursor(ProjectionWriter& w,std::string_view n,const LocalMotionCursor2& v){return emit(w,n,v);}
Status emit_local_motion_prefix_close(ProjectionWriter& w,std::string_view n,const LocalMotionPrefixClose2& v){return emit(w,n,v);}
Status emit_local_hex_motion_basis_fields(ProjectionWriter& w,const LocalHexMotionBasis2& v){return fields(w,v);}
Status emit_local_motion_cursor_fields(ProjectionWriter& w,const LocalMotionCursor2& v){return fields(w,v);}
Status emit_local_motion_prefix_close_fields(ProjectionWriter& w,const LocalMotionPrefixClose2& v){return fields(w,v);}
Status visit_military_local_motion_payloads(CodecVisitor& c,MilitaryLocalMotionState& state,Integer selected){return movement_detail::status([&]{try{
 if(selected!=2&&selected!=3)movement_detail::fail("MilitaryLocalMotionSchemaRequired");
 movement_detail::take(table(c,"local_motion_bases",state.bases));
 movement_detail::take(table(c,"local_motion_cursors",state.cursors));
 movement_detail::take(table(c,"local_motion_prefix_closes",state.prefix_closes,selected));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status visit_local_motion_boundary_source(CodecVisitor& c,LocalMotionBoundarySource& v,Integer selected){
 return movement_detail::status([&]{try{movement_detail::take(military_local_boundary_fields::selected(c,v,selected));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Outcome<LocalMotionBoundarySource> decode_local_motion_boundary_source(const BoundaryValue& raw,Integer selected){return movement_detail::guard<LocalMotionBoundarySource>([&]{try{
 ActorFoundationJsonVisitor c(raw);movement_detail::take(prepare_checkpoint_default_values<LocalMotionBoundarySource>(c));LocalMotionBoundarySource row;
 movement_detail::take(military_local_boundary_fields::selected(c,row,selected));movement_detail::take(c.finish());return row;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_local_motion_boundary_source_fields(ProjectionWriter& sink,const LocalMotionBoundarySource& row,Integer selected){
 return movement_detail::status([&]{try{ActorFoundationProjectionArchive c(sink);
 movement_detail::take(military_local_boundary_fields::selected(c,const_cast<LocalMotionBoundarySource&>(row),selected));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
Status emit_local_motion_boundary_source(ProjectionWriter& sink,std::string_view name,const LocalMotionBoundarySource& row,Integer selected){
 return sink.object(name,[&](ProjectionWriter& body){return emit_local_motion_boundary_source_fields(body,row,selected);});
}
Outcome<BoundaryValue> encode_local_motion_boundary_source(const LocalMotionBoundarySource& row,Integer selected){return movement_detail::guard<BoundaryValue>([&]{
 BoundaryValue out;BoundaryProjectionWriter sink(out);movement_detail::take(emit_local_motion_boundary_source(sink,"",row,selected));movement_detail::take(sink.finish());return out;
});}
}
