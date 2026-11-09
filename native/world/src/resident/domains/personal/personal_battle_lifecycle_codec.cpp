#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "personal_battle_lifecycle_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <type_traits>
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<PersonalBattleLifecycle1> battles;};
template<class A>Status event(A& a,PersonalBattleLifecycleEvent1& v){
 a.field("source_id",v.source_id);a.object("occurred_at",[&](auto& child){return exact_fields::moment(child,v.occurred_at);});return a.status();
}
template<class A>Status value(A& a,PersonalBattleLifecycle1& v){
 a.object("started",[&](auto& child){return event(child,v.started);});
 a.optional("result_applied_at",v.result_applied_at,[](auto& child,auto& at){return exact_fields::moment(child,at);});
 a.optional("closed",v.closed,[](auto& child,auto& closed){return event(child,closed);});return a.status();
}
template<class A>Status table(A& a,Staging& s){
 // Only this finite row is transferred. WorldMoment's original numeric copy
 // may throw; neither the row nor this adoption is represented as noexcept.
 const auto transfer=[](auto& rows,auto& row)->Status{rows.emplace_back(std::move(row));return common_success();};
 if constexpr(std::is_same_v<A,CodecVisitor>)return common_checkpoint::table(a,"battles",s.battles,[](CodecVisitor& child,const StableId& key,PersonalBattleLifecycle1& row){if(key.empty())return child.reject("PersonalBattleLifecycleOwnerRequired");return value(child,row);},transfer);
 else if constexpr(std::is_same_v<A,ActorFoundationJsonVisitor>)return a.table("battles",s.battles,[](auto& child,auto& row){return value(child,row);},transfer);
 else return a.table("battles",s.battles,[](auto& child,auto& row){return value(child,row);});
}
template<class A>Status fields(A& a,Staging& s,Integer selected){
 if(selected!=1)return a.reject("PersonalBattleLifecycleSelectedSchemaRequired");Integer schema=selected;a.field("schema",schema);if(!a.status().ok)return a.status();if(schema!=selected)return a.reject("PersonalBattleLifecycleSelectedSchemaMismatch");return table(a,s);
}
void empty_targets(const PersonalState& state){for(const auto& item:state.battles.rows())if(item.second.lifecycle)fail("PersonalBattleLifecycleAlreadyAttached",item.first);}
void collect(const PersonalState& state,Staging& staging){
 take(validate_personal_battle_lifecycle_layout(state,1));
 for(const auto& item:state.battles.rows())if(!staging.battles.insert(item.first,*item.second.lifecycle))fail("PersonalBattleLifecycleDuplicate",item.first);
}
void attach(PersonalState& state,Staging& staging){
 empty_targets(state);if(state.battles.size()!=staging.battles.size())fail("PersonalBattleLifecycleCoverageMismatch");
 // Validate the complete projection before the first move. Installation is
 // into the original private World candidate, not a second durable table.
 for(const auto& item:staging.battles.rows()){
  const auto* b=state.battles.find(item.first);const auto& life=item.second;
  if(item.first.empty()||!b||b->id!=item.first||life.started.source_id.empty()||(b->phase!="planning"&&b->phase!="result"&&b->phase!="closed"))fail("PersonalBattleLifecycleOwnerRequired",item.first);
  if(bool(life.result_applied_at)!=(b->phase!="planning")||bool(life.closed)!=(b->phase=="closed")||(life.closed&&life.closed->source_id.empty()))fail("PersonalBattleLifecyclePhaseMismatch",item.first);
 }
 for(const auto& item:staging.battles.rows()){
  auto* original=state.battles.find(item.first);auto* saved=staging.battles.find(item.first);
  // The original string buffers move with their real decode reservation.
  // Numeric fields use their own tracked fallible copy. On any failure the
  // outer owner discards the entire unpublished candidate, not a partial row.
  original->lifecycle.emplace(std::move(*saved));
 }
 take(validate_personal_battle_lifecycle_layout(state,1));
}
}
Status visit_personal_battle_lifecycle(CodecVisitor& c,PersonalState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("PersonalBattleLifecycleSelectedSchemaRequired");
 if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_personal_battle_lifecycle(const BoundaryValue& raw,PersonalState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("PersonalBattleLifecycleSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");
 empty_targets(state);ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());Staging staging;take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_personal_battle_lifecycle_fields(ProjectionWriter& sink,const PersonalState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("PersonalBattleLifecycleSelectedSchemaRequired");Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_personal_battle_lifecycle(ProjectionWriter& sink,std::string_view name,const PersonalState& state,Integer selected){return sink.object(name,[&](auto& fields){return emit_personal_battle_lifecycle_fields(fields,state,selected);});}
Outcome<BoundaryValue> encode_personal_battle_lifecycle(const PersonalState& state,Integer selected){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_personal_battle_lifecycle(sink,"",state,selected));take(sink.finish());return out;});}
namespace {
template<class Emit>Status pins(const CommonStore& common,const PersonalState& state,Integer selected,const WorldMoment& now,Emit emit){return status([&]{
 take(validate_personal_battle_lifecycle_sources(common,state,selected,now));if(!selected)return;
 for(const auto& item:state.battles.rows()){const auto& b=item.second;const auto& life=*b.lifecycle;
  take(emit(SourcePinBuildView{"personal","personal_battle_lifecycle",b.id,"started","personal","personal_battle_started",life.started.source_id,SourcePinReason::dependent_state}));
  if(life.result_applied_at)take(emit(SourcePinBuildView{"personal","personal_battle_lifecycle",b.id,"result_applied","personal","personal_battle",b.id,SourcePinReason::unique_outcome}));
  if(life.closed)take(emit(SourcePinBuildView{"personal","personal_battle_lifecycle",b.id,"closed","personal","personal_battle_closed",life.closed->source_id,SourcePinReason::dependent_state}));
 }
});}
}
Status collect_personal_battle_lifecycle_pins(const CommonStore& c,const PersonalState& s,Integer selected,const WorldMoment& now,SourcePinSink& sink){return pins(c,s,selected,now,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_personal_battle_lifecycle_pin(const CommonStore& c,const PersonalState& s,Integer selected,const WorldMoment& now,const SourcePin& input){return status([&]{bool found=false;take(pins(c,s,selected,now,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("PersonalBattleLifecyclePinRequired",input.consumer.id);});}
std::vector<RequiredSourceConsumerSlot> personal_battle_lifecycle_consumer_slots(){return {{"personal","personal_battle_lifecycle","started"},{"personal","personal_battle_lifecycle","result_applied"},{"personal","personal_battle_lifecycle","closed"}};}
Outcome<bool> personal_battle_lifecycle_source_may_release(const CommonStore& c,const PersonalState& s,Integer selected,const WorldMoment& now,const SourceRef& source,Integer schema){return guard<bool>([&]{
 if(schema!=1||source.domain!="personal"||(source.kind!="personal_battle_started"&&source.kind!="personal_battle"&&source.kind!="personal_battle_closed"))fail("PersonalBattleLifecycleSourceRequired",source.id);bool found=false;take(pins(c,s,selected,now,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("PersonalBattleLifecycleSourceRequired",source.id);return false;
});}
}
