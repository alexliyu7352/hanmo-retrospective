#include "resident/domains/military/military_model.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_battle_lifecycle_codec.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <type_traits>
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<MilitaryBattleLifecycle1> battles;};
template<class A>Status event(A& a,MilitaryBattleLifecycleEvent1& v){a.field("source_id",v.source_id);a.object("occurred_at",[&](auto& child){return exact_fields::moment(child,v.occurred_at);});return a.status();}
template<class A>Status value(A& a,MilitaryBattleLifecycle1& v){a.object("started",[&](auto& child){return event(child,v.started);});a.optional("closed",v.closed,[](auto& child,auto& closed){return event(child,closed);});return a.status();}
template<class A>Status table(A& a,Staging& s){
 // One finite decoded row. WorldMoment's original numeric move/copy may throw;
 // the outer candidate owns failure cleanup, not a fictitious noexcept path.
 const auto transfer=[](auto& rows,auto& row)->Status{rows.emplace_back(std::move(row));return common_success();};
 if constexpr(std::is_same_v<A,CodecVisitor>)return common_checkpoint::table(a,"battles",s.battles,[](CodecVisitor& child,const StableId& key,MilitaryBattleLifecycle1& row){if(key.empty())return child.reject("MilitaryBattleLifecycleOwnerRequired");return value(child,row);},transfer);
 else if constexpr(std::is_same_v<A,ActorFoundationJsonVisitor>)return a.table("battles",s.battles,[](auto& child,auto& row){return value(child,row);},transfer);
 else return a.table("battles",s.battles,[](auto& child,auto& row){return value(child,row);});
}
template<class A>Status fields(A& a,Staging& s,Integer selected){if(selected!=1)return a.reject("MilitaryBattleLifecycleSelectedSchemaRequired");Integer schema=selected;a.field("schema",schema);if(!a.status().ok)return a.status();if(schema!=selected)return a.reject("MilitaryBattleLifecycleSelectedSchemaMismatch");return table(a,s);}
MilitaryBattleRecord* owner(MilitaryState& state,const StableId& id){MilitaryBattleRecord* out=nullptr;for(auto& b:state.battles)if(b.id==id){if(out)fail("MilitaryBattleLifecycleDuplicateOwner",id);out=&b;}return out;}
void empty_targets(const MilitaryState& state){for(const auto& b:state.battles)if(b.lifecycle)fail("MilitaryBattleLifecycleAlreadyAttached",b.id);}
void collect(const MilitaryState& state,Staging& staging){take(validate_military_battle_lifecycle_layout(state,1));for(const auto& b:state.battles)if(!staging.battles.insert(b.id,*b.lifecycle))fail("MilitaryBattleLifecycleDuplicateOwner",b.id);}
void attach(MilitaryState& state,Staging& staging){
 empty_targets(state);if(state.battles.size()!=staging.battles.size())fail("MilitaryBattleLifecycleCoverageMismatch");
 // Validate every canonical target before the first move. These are projection
 // rows only; their keys do not establish another authoritative battle ledger.
 for(const auto& item:staging.battles.rows()){const auto* b=owner(state,item.first);const auto& life=item.second;
  if(item.first.empty()||!b||life.started.source_id.empty()||(b->phase!="planning"&&b->phase!="result"&&b->phase!="closed"))fail("MilitaryBattleLifecycleOwnerRequired",item.first);
  if(bool(life.closed)!=(b->phase=="closed")||(life.closed&&life.closed->source_id.empty()))fail("MilitaryBattleLifecyclePhaseMismatch",item.first);
 }
 for(const auto& item:staging.battles.rows()){auto* b=owner(state,item.first);auto* life=staging.battles.find(item.first);b->lifecycle.emplace(std::move(*life));}
 take(validate_military_battle_lifecycle_layout(state,1));
}
}
Status visit_military_battle_lifecycle(CodecVisitor& c,MilitaryState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryBattleLifecycleSelectedSchemaRequired");
 if(c.reading())empty_targets(state);Staging staging;if(!c.reading())collect(state,staging);take(fields(c,staging,selected));if(c.reading())attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status decode_military_battle_lifecycle(const BoundaryValue& raw,MilitaryState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryBattleLifecycleSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");
 empty_targets(state);ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());Staging staging;take(fields(c,staging,selected));take(c.finish());attach(state,staging);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_battle_lifecycle_fields(ProjectionWriter& sink,const MilitaryState& state,Integer selected){return status([&]{try{
 if(selected!=1)fail("MilitaryBattleLifecycleSelectedSchemaRequired");Staging staging;collect(state,staging);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_battle_lifecycle(ProjectionWriter& sink,std::string_view name,const MilitaryState& state,Integer selected){return sink.object(name,[&](auto& fields){return emit_military_battle_lifecycle_fields(fields,state,selected);});}
Outcome<BoundaryValue> encode_military_battle_lifecycle(const MilitaryState& state,Integer selected){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_battle_lifecycle(sink,"",state,selected));take(sink.finish());return out;});}
namespace {
template<class Emit>Status pins(const CommonStore& common,const MilitaryState& state,Integer selected,const WorldMoment& now,Emit emit){return status([&]{
 take(validate_military_battle_lifecycle_sources(common,state,selected,now));if(!selected)return;
 for(const auto& b:state.battles){const auto& life=*b.lifecycle;
  take(emit(SourcePinBuildView{"military","military_battle_lifecycle",b.id,"started","military","military_battle_started",life.started.source_id,SourcePinReason::dependent_state}));
  if(life.closed)take(emit(SourcePinBuildView{"military","military_battle_lifecycle",b.id,"closed","military","military_battle_closed",life.closed->source_id,SourcePinReason::dependent_state}));
 }
});}
}
Status collect_military_battle_lifecycle_pins(const CommonStore& c,const MilitaryState& s,Integer selected,const WorldMoment& now,SourcePinSink& sink){return pins(c,s,selected,now,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_military_battle_lifecycle_pin(const CommonStore& c,const MilitaryState& s,Integer selected,const WorldMoment& now,const SourcePin& input){return status([&]{bool found=false;take(pins(c,s,selected,now,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryBattleLifecyclePinRequired",input.consumer.id);});}
std::vector<RequiredSourceConsumerSlot> military_battle_lifecycle_consumer_slots(){return {{"military","military_battle_lifecycle","started"},{"military","military_battle_lifecycle","closed"}};}
Outcome<bool> military_battle_lifecycle_source_may_release(const CommonStore& c,const MilitaryState& s,Integer selected,const WorldMoment& now,const SourceRef& source,Integer schema){return guard<bool>([&]{
 if(schema!=1||source.domain!="military"||(source.kind!="military_battle_started"&&source.kind!="military_battle_closed"))fail("MilitaryBattleLifecycleSourceRequired",source.id);
 bool found=false;take(pins(c,s,selected,now,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("MilitaryBattleLifecycleSourceRequired",source.id);return false;
});}
}
