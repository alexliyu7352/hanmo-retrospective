#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_tactical_ownership_codec.hpp"
#include "military_tactical_ownership_codec_fields.hpp"
#include "military_exact_sources.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/storage/common_checkpoint.hpp"
namespace hanmo::resident {namespace {
template<class T>Outcome<T> decode(const BoundaryValue& input){return movement_detail::guard<T>([&]{try{
 ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<T>(c));T row;movement_detail::take(military_tactical_ownership_fields::value(c,row));movement_detail::take(c.finish());return row;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status visit(CodecVisitor& c,T& row){return movement_detail::status([&]{try{
 movement_detail::take(military_tactical_ownership_fields::value(c,row));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit_fields(ProjectionWriter& sink,const T& row){return movement_detail::status([&]{try{
 ActorFoundationProjectionArchive c(sink);movement_detail::take(military_tactical_ownership_fields::value(c,const_cast<T&>(row)));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
template<class T>Status emit(ProjectionWriter& sink,std::string_view name,const T& row){return sink.object(name,[&](ProjectionWriter& value){return emit_fields(value,row);});}
template<class T>Outcome<BoundaryValue> encode(const T& row){return movement_detail::guard<BoundaryValue>([&]{
 BoundaryValue out;BoundaryProjectionWriter sink(out);movement_detail::take(emit(sink,"",row));movement_detail::take(sink.finish());return out;
 });}
const StableId& key(const MilitaryTacticalFormation3& row){return row.formation_id;}
const StableId& key(const MilitaryTacticalMissionRecord3& row){return row.id;}
template<class T>Status table(CodecVisitor& c,std::string_view name,OrderedTable<T>& rows){
 static_assert(std::is_same_v<T,MilitaryTacticalFormation3>||std::is_same_v<T,MilitaryTacticalMissionRecord3>);
 if(c.reading()&&!rows.empty())return c.reject("MilitaryTacticalOwnedRootAlreadyPopulated",name);
 if(!c.reading())for(const auto& row:rows.rows())if(row.first.empty()||row.first!=key(row.second))return c.reject("MilitaryTacticalOwnedRowKeyMismatch",name);
 auto result=common_checkpoint::table(c,name,rows,[](auto& x,auto& row){return military_tactical_ownership_fields::value(x,row);},[](auto& destination,auto& source)->Status{
  destination.emplace_back(std::move(source));return common_success();
 });
 if(!result.ok)return result;
 for(const auto& row:rows.rows())if(row.first.empty()||row.first!=key(row.second))return c.reject("MilitaryTacticalOwnedRowKeyMismatch",name);
 return c.status();
}
template<class T>Status project_table(ProjectionWriter& sink,std::string_view name,const OrderedTable<T>& rows){
 for(const auto& row:rows.rows())if(row.first.empty()||row.first!=key(row.second))return common_failure("MilitaryTacticalOwnedRowKeyMismatch",row.first);
 ActorFoundationProjectionArchive original(sink);
 return original.table(name,const_cast<OrderedTable<T>&>(rows),[](auto& c,auto& row){return military_tactical_ownership_fields::value(c,row);});
}
}
#define MILITARY_TACTICAL_CODEC(Type,suffix) \
Status visit_##suffix(CodecVisitor& c,Type& row){return visit(c,row);} \
Outcome<Type> decode_##suffix(const BoundaryValue& input){return decode<Type>(input);} \
Outcome<BoundaryValue> encode_##suffix(const Type& row){return encode(row);} \
Status emit_##suffix(ProjectionWriter& sink,std::string_view name,const Type& row){return emit(sink,name,row);} \
Status emit_##suffix##_fields(ProjectionWriter& sink,const Type& row){return emit_fields(sink,row);}
MILITARY_TACTICAL_CODEC(MilitaryTacticalFormation3,military_tactical_formation3)
MILITARY_TACTICAL_CODEC(MilitaryTacticalMissionRecord3,military_tactical_mission3)
MILITARY_TACTICAL_CODEC(DirectForceIntent3,direct_force_intent3)
MILITARY_TACTICAL_CODEC(DirectForceIntentReceipt3,direct_force_intent_receipt3)
#undef MILITARY_TACTICAL_CODEC
Status visit_military_tactical_ownership3(CodecVisitor& c,MilitaryExactState& state){return movement_detail::status([&]{try{
 movement_detail::take(table(c,"formations",state.formations));movement_detail::take(table(c,"missions",state.missions));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});}
Status emit_military_tactical_ownership3(ProjectionWriter& sink,const MilitaryExactState& state){return movement_detail::status([&]{
 movement_detail::take(project_table(sink,"formations",state.formations));movement_detail::take(project_table(sink,"missions",state.missions));
 });}
}
