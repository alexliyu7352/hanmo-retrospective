#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_exact_source_codec.hpp"
#include "military_exact_source_codec_fields.hpp"
#include "military_supply_exact_codec_fields.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "military_state_codec_archive.hpp"
namespace hanmo::resident {
namespace {
struct OriginalMilitarySourceKeys {
 template<class T>static constexpr bool owns=std::is_same_v<T,DynamicMilitaryBattleFact2>||
  std::is_same_v<T,DynamicMilitaryCasualtyFact2>||std::is_same_v<T,MilitarySupplyEffectSource2>;
 template<class T>bool operator()(const StableId& key,const T& row)const{return !key.empty()&&key==row.id;}
 bool operator()(const StableId& key,const MilitarySupplyEffectSource2& row)const{return !key.empty()&&key==row.original_supply_fact_ref.id;}
};
template<class T,class F>Status source_table(CodecVisitor& c,std::string_view name,OrderedTable<T>& table,F fields){
 military_state_codec::RootArchive<CodecVisitor,OriginalMilitarySourceKeys> root{c,"MilitaryExactSourceKeyMismatch","MilitaryExactSourceRootAlreadyPopulated"};
 return root.table(name,table,std::move(fields));
}
template<class T>Outcome<T> decode_source(const BoundaryValue& input){
 return movement_detail::guard<T>([&]{try{ActorFoundationJsonVisitor c(input);movement_detail::take(prepare_checkpoint_default_values<T>(c));T row;
  movement_detail::take(military_exact_source_fields::value(c,row));
  movement_detail::take(c.finish());return row;
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
template<class T>Status visit_source(CodecVisitor& c,T& row){
 return movement_detail::status([&]{try{movement_detail::take(military_exact_source_fields::value(c,row));}
 catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
template<class T>Status emit_source_fields(ProjectionWriter& sink,const T& row){
 return movement_detail::status([&]{try{
  ActorFoundationProjectionArchive c(sink);
  movement_detail::take(military_exact_source_fields::value(c,const_cast<T&>(row)));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
template<class T>Status emit_source(ProjectionWriter& sink,std::string_view name,const T& row){
 return sink.object(name,[&](ProjectionWriter& object){return emit_source_fields(object,row);});
}
template<class T>Outcome<BoundaryValue> encode_source(const T& row){
 return movement_detail::guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);
  movement_detail::take(emit_source(sink,"",row));movement_detail::take(sink.finish());return out;});
}
}
Status visit_dynamic_military_battle_fact(CodecVisitor& c,DynamicMilitaryBattleFact2& row){return visit_source(c,row);}
Status visit_dynamic_military_casualty_fact(CodecVisitor& c,DynamicMilitaryCasualtyFact2& row){return visit_source(c,row);}
Outcome<DynamicMilitaryBattleFact2> decode_dynamic_military_battle_fact(const BoundaryValue& input){return decode_source<DynamicMilitaryBattleFact2>(input);}
Outcome<DynamicMilitaryCasualtyFact2> decode_dynamic_military_casualty_fact(const BoundaryValue& input){return decode_source<DynamicMilitaryCasualtyFact2>(input);}
Outcome<BoundaryValue> encode_dynamic_military_battle_fact(const DynamicMilitaryBattleFact2& row){return encode_source(row);}
Outcome<BoundaryValue> encode_dynamic_military_casualty_fact(const DynamicMilitaryCasualtyFact2& row){return encode_source(row);}
Status emit_dynamic_military_battle_fact(ProjectionWriter& sink,std::string_view name,const DynamicMilitaryBattleFact2& row){return emit_source(sink,name,row);}
Status emit_dynamic_military_casualty_fact(ProjectionWriter& sink,std::string_view name,const DynamicMilitaryCasualtyFact2& row){return emit_source(sink,name,row);}
Status emit_dynamic_military_battle_fact_fields(ProjectionWriter& sink,const DynamicMilitaryBattleFact2& row){return emit_source_fields(sink,row);}
Status emit_dynamic_military_casualty_fact_fields(ProjectionWriter& sink,const DynamicMilitaryCasualtyFact2& row){return emit_source_fields(sink,row);}
Status visit_military_exact_source_payloads(CodecVisitor& c,MilitaryExactState& state){
 return movement_detail::status([&]{try{
  movement_detail::take(source_table(c,"dynamic_battle_facts",state.dynamic_battle_facts,
   [](auto& fields,auto& row){return military_exact_source_fields::value(fields,row);}));
  movement_detail::take(source_table(c,"dynamic_casualty_facts",state.dynamic_casualty_facts,
   [](auto& fields,auto& row){return military_exact_source_fields::value(fields,row);}));
  movement_detail::take(source_table(c,"supply_effect_sources",state.supply_effect_sources,
   [](auto& fields,auto& row){return military_supply_exact_fields::value(fields,row);}));
 }catch(const exact_fields::CodecFailure& e){throw movement_detail::Failure{e.error};}});
}
}
