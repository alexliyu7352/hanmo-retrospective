#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_formation_reservation_codec.hpp"
#include "military_formation_reservations.hpp"
#include "military_action_resource_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {using namespace movement_detail;namespace {
struct Staging {OrderedTable<MilitaryFormationReservations1> formation_acceptances;};
struct Keys {
 template<class T>static constexpr bool owns=std::is_same_v<T,MilitaryFormationReservations1>;
 bool operator()(const StableId& key,const MilitaryFormationReservations1&)const{return !key.empty();}
};
bool formation(const DynamicMilitaryActionFact2& row){return row.command=="army_form";}
void owner(const StableId& key,const DynamicMilitaryActionFact2& row){
 const auto* terms=std::get_if<ArmyFormationTerms>(&row.original_terms);
 if(key.empty()||key!=row.id||!formation(row)||row.rule_version!=2||!terms||row.actor_id!=terms->actor_id||(row.phase!="started"&&row.phase!="completed"))fail("MilitaryFormationReservationOriginalOwnerRequired",key);
}
template<class Emit>void each(const MilitaryActionTimeState& state,Emit emit){
 for(const auto& item:state.dynamic_action_facts.rows())if(formation(item.second)){owner(item.first,item.second);emit(item.first,item.second);}else if(item.second.acceptance_resource_reservations)fail("UnexpectedMilitaryFormationReservations",item.first);
}
void empty_targets(const MilitaryActionTimeState& state){for(const auto& item:state.dynamic_action_facts.rows())if(item.second.acceptance_resource_reservations)fail("MilitaryFormationReservationAdjunctAlreadyAttached",item.first);}
void layout(const MilitaryActionTimeState& state,Integer selected){
 if(selected==0){empty_targets(state);return;}if(selected!=1&&selected!=2)fail("MilitaryFormationReservationSelectedSchemaRequired");
 each(state,[&](const StableId& id,const DynamicMilitaryActionFact2& row){if(!row.acceptance_resource_reservations)fail("MilitaryFormationAcceptanceReservationsRequired",id);const auto& effect=*row.acceptance_resource_reservations;if(selected==1&&effect.empty_inventory_opening)fail("UnexpectedMilitaryFormationInventoryOpening",id);if(selected==2&&bool(effect.empty_inventory_opening)!=effect.inventory_reservations.empty())fail("MilitaryFormationInventoryOpeningCoverageMismatch",id);});
}
template<class A>Status fields(A& archive,Staging& staging,Integer selected,const military_action_resource_fields::Layout& resource_layout){
 if(selected!=1&&selected!=2)return archive.reject("MilitaryFormationReservationSelectedSchemaRequired");Integer schema=selected;archive.field("schema",schema);if(!archive.status().ok)return archive.status();if(schema!=selected)return archive.reject("MilitaryFormationReservationSelectedSchemaMismatch");
 military_state_codec::RootArchive<A,Keys> root{archive};root.table("formation_acceptances",staging.formation_acceptances,[&](auto& child,auto& row){return military_action_resource_fields::value(child,row,resource_layout,selected);});return root.status();
}
void collect(const MilitaryActionTimeState& state,Staging& staging,Integer selected){layout(state,selected);each(state,[&](const StableId& key,const DynamicMilitaryActionFact2& row){if(!staging.formation_acceptances.insert(key,*row.acceptance_resource_reservations))fail("MilitaryFormationReservationDuplicate",key);});}
CheckpointMemoryCharge reserve_row(const DynamicMilitaryActionFact2& row){return CheckpointMemoryCharge(0);}
struct PreparedRow {CheckpointMemoryCharge copied_fields;DynamicMilitaryActionFact2 row;explicit PreparedRow(const DynamicMilitaryActionFact2& v):copied_fields(reserve_row(v)),row(v){}};
void attach(MilitaryActionTimeState& state,const Staging& staging,Integer selected){
 std::size_t count=0;each(state,[&](const StableId&,const DynamicMilitaryActionFact2&){++count;});if(count!=staging.formation_acceptances.size())fail("MilitaryFormationReservationAdjunctCoverageMismatch");
 for(const auto& item:staging.formation_acceptances.rows()){const auto* row=state.dynamic_action_facts.find(item.first);if(!row)fail("MilitaryFormationReservationOrphan",item.first);owner(item.first,*row);}
 for(const auto& item:staging.formation_acceptances.rows()){PreparedRow candidate(*state.dynamic_action_facts.find(item.first));candidate.row.acceptance_resource_reservations.emplace(item.second);state.dynamic_action_facts.set(item.first,candidate.row);}layout(state,selected);
}
}
Status validate_military_formation_reservation_layout(const MilitaryActionTimeState& state,Integer selected){return status([&]{layout(state,selected);});}
// 遍历编组预约附录；保留原资源字段格式，拒绝不支持的版本与留存遍历。
Status visit_military_formation_reservations(CodecVisitor &c,
                                             MilitaryActionTimeState &state,
                                             Integer selected) {
  return status([&] {
    try {
      if (selected != 1 && selected != 2)
        fail("MilitaryFormationReservationSelectedSchemaRequired");

      const CommonResourceCodecContext resources{
          c.item_instance_refs_enabled, c.lot_property_refs_enabled,
          c.domain_activity_reservation_refs_enabled,
          c.carrier_location_enabled};
      const military_action_resource_fields::Layout resource_layout{
          bool(c.schema_features &
               common_checkpoint::typed_resource_references),
          resources};
      if (c.reading())
        empty_targets(state);
      Staging staging;
      if (!c.reading())
        collect(state, staging, selected);
      take(fields(c, staging, selected, resource_layout));
      if (c.reading())
        attach(state, staging, selected);
    } catch (const exact_fields::CodecFailure &e) {
      throw Failure{e.error};
    }
  });
}
Status decode_military_formation_reservations(const BoundaryValue& raw,MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return status([&]{try{
 if(selected!=1&&selected!=2)fail("MilitaryFormationReservationSelectedSchemaRequired");if(!ScopedCheckpointJsonDecode::current())fail("CheckpointJsonContextRequired");empty_targets(state);Staging staging;ActorFoundationJsonVisitor c(raw);take(c.ordered_fields());take(fields(c,staging,selected,{typed,resources}));take(c.finish());attach(state,staging,selected);
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_formation_reservations_fields(ProjectionWriter& sink,const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return status([&]{try{
 if(selected!=1&&selected!=2)fail("MilitaryFormationReservationSelectedSchemaRequired");Staging staging;collect(state,staging,selected);ActorFoundationProjectionArchive c(sink);take(fields(c,staging,selected,{typed,resources}));
}catch(const exact_fields::CodecFailure& e){throw Failure{e.error};}});}
Status emit_military_formation_reservations(ProjectionWriter& sink,std::string_view name,const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return sink.object(name,[&](auto& fields){return emit_military_formation_reservations_fields(fields,state,selected,typed,resources);});}
Outcome<BoundaryValue> encode_military_formation_reservations(const MilitaryActionTimeState& state,Integer selected,bool typed,const CommonResourceCodecContext& resources){return guard<BoundaryValue>([&]{BoundaryValue out;BoundaryProjectionWriter sink(out);take(emit_military_formation_reservations(sink,"",state,selected,typed,resources));take(sink.finish());return out;});}
Status validate_military_formation_reservation_sources(const CommonStore& common,const MilitaryCatalog& catalog,const MilitaryActionTimeState& state,Integer selected){return status([&]{
 layout(state,selected);if(selected==0)return;
 each(state,[&](const StableId& id,const DynamicMilitaryActionFact2& row){take(read_actual_common_fact_position(common,SourceRefBuildView{"military","military_action",id}));take(validate_original_formation_acceptance_reservations(catalog,row,selected));});
});}
namespace {
template<class Emit>Status visit_pins(const CommonStore& common,const MilitaryCatalog& catalog,const MilitaryActionTimeState& state,Integer selected,Emit emit){return status([&]{
 if(selected!=1&&selected!=2)fail("MilitaryFormationReservationSelectedSchemaRequired");
 take(validate_military_formation_reservation_sources(common,catalog,state,selected));
 each(state,[&](const StableId& id,const DynamicMilitaryActionFact2&){take(emit(SourcePinBuildView{"military","formation_reservation",id,"actual_acceptance","military","military_action",id,SourcePinReason::unique_outcome}));});
});}
}
Status collect_military_formation_reservation_pins(const CommonStore& common,const MilitaryCatalog& catalog,const MilitaryActionTimeState& state,SourcePinSink& sink,Integer selected){return visit_pins(common,catalog,state,selected,[&](const SourcePinBuildView& pin){return sink.contains(pin)?common_success():sink.append(pin);});}
Status validate_military_formation_reservation_pin(const CommonStore& common,const MilitaryCatalog& catalog,const MilitaryActionTimeState& state,const SourcePin& input,Integer selected){return status([&]{
 bool found=false;take(visit_pins(common,catalog,state,selected,[&](const SourcePinBuildView& pin){if(pin.consumer_domain==input.consumer.domain&&pin.consumer_kind==input.consumer.kind&&pin.consumer_id==input.consumer.id&&pin.consumer_slot==input.consumer.slot&&pin.source_domain==input.source.domain&&pin.source_kind==input.source.kind&&pin.source_id==input.source.id&&pin.reason==input.reason)found=true;return common_success();}));if(!found)fail("MilitaryFormationReservationPinRequired",input.consumer.id);
});}
std::vector<RequiredSourceConsumerSlot> military_formation_reservation_consumer_slots(){return {{"military","formation_reservation","actual_acceptance"}};}
Outcome<bool> military_formation_reservation_source_may_release(const CommonStore& common,const MilitaryCatalog& catalog,const MilitaryActionTimeState& state,const SourceRef& source,Integer schema,Integer selected){return guard<bool>([&]{
 if(schema!=2)fail("MilitaryFormationReservationSourceSchemaRequired",source.id);bool found=false;take(visit_pins(common,catalog,state,selected,[&](const SourcePinBuildView& pin){if(pin.source_domain==source.domain&&pin.source_kind==source.kind&&pin.source_id==source.id)found=true;return common_success();}));if(!found)fail("MilitaryFormationReservationSourceRequired",source.id);return false;
});}
}
