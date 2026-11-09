#include "naval_contact_history.hpp"
#include "return_spatial.hpp"
#include "return_original_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "return_exact_detail.hpp"
#include "military_roster_history.hpp"
#include "military_field_command_source.hpp"
#include "resident/storage/actor_location_execution_draft.hpp"
#include <algorithm>
#include <limits>
#include <set>
namespace hanmo::resident {using namespace return_exact_detail;namespace {
CheckpointMemoryCharge reserve_completed_return(const MilitaryCompletedReturnSourceBuildView& input){return CheckpointMemoryCharge(0);}
template<class Context>const PlaceDefinition& original_place_definition(const Context& x,const DynamicMilitaryLogisticsFact2& f){
 const auto* place=x.access.common_catalog?x.access.common_catalog->places.find(f.original_terms.place_id):nullptr;
 if(!place||!f.terminal_source_ref)fail("MilitaryReturnOriginalPlaceRequired",f.id);return *place;
}
MovementAnchor original_place(const ExactReturnReadContext& x,const DynamicMilitaryLogisticsFact2& f){
 const auto& place=original_place_definition(x,f);
 return OriginalMovementAnchor{SettlementLocation{place.settlement_id,place.id},*f.terminal_source_ref};
}
// The original placement selection returns its own fresh strings. Borrowed
// scratch never outlives the original members read, and cannot be its output.
struct MilitaryReturnPlacedActors {
 CheckpointMemoryCharge copied_fields;std::vector<StableId> values;
 explicit MilitaryReturnPlacedActors(const CheckpointStringVectorBuildView& input)
  :copied_fields([&]{return CheckpointMemoryCharge(0);}()){
  values.reserve(input.values.size());for(const auto value:input.values)values.emplace_back(value);
 }
 MilitaryReturnPlacedActors(const MilitaryReturnPlacedActors&)=default;
 MilitaryReturnPlacedActors(MilitaryReturnPlacedActors&&)noexcept=default;
 MilitaryReturnPlacedActors& operator=(const MilitaryReturnPlacedActors&)=delete;
 MilitaryReturnPlacedActors& operator=(MilitaryReturnPlacedActors&&)=delete;
};
CheckpointVector<const StableId*> placed_members(const CommonStore& c,const MilitaryState& m,const ExactReturnReadContext& x,const DynamicMilitaryReturnFact2& returned,const DynamicMilitaryLogisticsFact2& f,const WorldMoment& now,const MilitaryReturnSourceRead& returns){
 if(x.identity){const NavalActualPrefixScratch cut_owner{take(read_actual_common_fact_prefix_before(c,*f.terminal_source_ref))};const auto& cut=cut_owner.prefix;return take(read_military_army_member_refs_at_selected_actual_prefix(c,m,*x.identity,x.exact,returned.army_id,cut,*f.completed_at,now,x.budget,&x.formations,&returns,x.field_history));}
 if(!x.exact)fail("ExactReturnOriginalDynamicRootRequired");return take(read_military_army_member_refs_before_exact(c,m,*x.exact,returned.army_id,*f.terminal_source_ref,*f.completed_at,now,x.budget,&x.formations,&returns));
}
CheckpointVector<const StableId*> placed_members(const CommonStore& c,const MilitaryState& m,const ExactReturnSourceReadContext& x,const DynamicMilitaryReturnFact2& returned,const DynamicMilitaryLogisticsFact2& f,const WorldMoment& now,const MilitaryReturnSourceRead& returns){
 if(!x.identity){if(!x.exact)fail("ExactReturnOriginalDynamicRootRequired");return take(read_military_army_member_refs_before_exact(c,m,*x.exact,returned.army_id,*f.terminal_source_ref,*f.completed_at,now,x.budget,&x.formations,&returns));}
 const NavalActualPrefixScratch cut_owner{take(read_actual_common_fact_prefix_before(c,*f.terminal_source_ref))};const auto& cut=cut_owner.prefix;
 return take(read_military_army_member_refs_at_selected_actual_prefix(c,m,*x.identity,x.exact,returned.army_id,cut,*f.completed_at,now,x.budget,&x.formations,&returns,x.field_history));
}
template<class Context>MilitaryReturnPlacedActors placed_actors(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const Context& x,const DynamicMilitaryLogisticsFact2& f,const MilitaryReturnSourceRead& returns){
 const auto& returned=original_return(c,m,s,x,f);const auto now=take(x.clock.read_current_moment());
 // Exactly the original member loop followed by its named captive handover.
 // Member and commander identities come from the actual completion Common cut.
 const auto members=placed_members(c,m,x,returned,f,now,returns);
 const auto command=take(read_military_army_command_source_exact(c,m,returned.army_id,now,*f.terminal_source_ref,&x.formations,x.budget));
 CheckpointVector<std::string_view> selected;
 std::set<std::string_view,std::less<>,CheckpointAllocator<std::string_view>> unique;
 if(members.size()>selected.max_size()||returned.actor_changes.size()>selected.max_size()-members.size())fail("MilitaryReturnPlacementTooLarge",f.id);
 selected.reserve(members.size()+returned.actor_changes.size());
 for(const auto* member:members){const auto& actor=*member;if(actor!=command.commander_actor_id){if(!c.actor(actor)||!unique.insert(actor).second)fail("MilitaryReturnMemberSourceMismatch",actor);selected.push_back(actor);}}
 for(const auto& change:returned.actor_changes){if(!unique.insert(change.actor_id).second)fail("MilitaryReturnDuplicatePlacement",change.actor_id);selected.push_back(change.actor_id);}
 return MilitaryReturnPlacedActors{CheckpointStringVectorBuildView{selected}};
}
}
MilitaryCompletedReturnSource::MilitaryCompletedReturnSource(const MilitaryCompletedReturnSourceBuildView& input)
 :copied_fields(reserve_completed_return(input)),original(input.original),
 actual_return{{StableId(input.actual_return.source_ref.domain),StableId(input.actual_return.source_ref.kind),StableId(input.actual_return.source_ref.id)},input.actual_return.source_schema},
 actual_application{{StableId(input.actual_application.source_ref.domain),StableId(input.actual_application.source_ref.kind),StableId(input.actual_application.source_ref.id)},input.actual_application.source_schema}{
 static_assert(std::is_nothrow_move_constructible_v<MomentOwnerSource>);
}


namespace {
template<class Context>Outcome<std::optional<MilitaryCompletedReturnSource>> return_source_read(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MilitaryReturnSourceRead& returns_,const StableId& army){return guard<std::optional<MilitaryCompletedReturnSource>>([&]()->std::optional<MilitaryCompletedReturnSource>{
 bound(common_,military_,state_,context_);bool exists=false;for(const auto& row:military_.armies)if(row.id==army){if(exists)fail("DuplicateMilitaryArmy",army);exists=true;}if(!exists)fail("MilitaryReturnArmyRequired",army);
 const DynamicMilitaryReturnFact2* found=nullptr;
 for(const auto& item:state_.dynamic_return_facts.rows())if(item.second.army_id==army){if(item.first!=item.second.id||found)fail("DuplicateExactReturnReceipt",item.first);found=&item.second;}
 if(!found)return std::nullopt;
 const auto& completion=found->completion_source_ref;
 const MomentOwnerSourceBuildView applied_view{{completion.domain,completion.kind,completion.id},2};
 auto local_fields=CheckpointMemoryCharge(0);
 const MomentOwnerSource applied{completion,2};const auto& f=source_local_return(common_,military_,state_,elapsed_,context_,applied);
 if(!f.result||f.result->receipt_id!=found->id)fail("ExactReturnReceiptUnowned",found->id);
 return MilitaryCompletedReturnSource{MilitaryCompletedReturnSourceBuildView{found,{{"military","military_return",found->id},2},applied_view}};
});}
template<class Context>Outcome<WorldMoment> return_source_moment(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MilitaryReturnSourceRead& returns_,const MomentOwnerSource& source){return guard<WorldMoment>([&]{return *source_local_return(common_,military_,state_,elapsed_,context_,source).completed_at;});}
template<class Context>Outcome<std::optional<MilitaryActionPlacement>> return_actor_placement(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MilitaryReturnSourceRead& returns_,const MomentOwnerSource& source,const StableId& actor){return guard<std::optional<MilitaryActionPlacement>>([&]()->std::optional<MilitaryActionPlacement>{
 if(!common_.actor(actor))fail("SpatialActorRequired",actor);const auto& f=source_local_return(common_,military_,state_,elapsed_,context_,source);
 const auto people=placed_actors(common_,military_,state_,context_,f,returns_);if(std::find(people.values.begin(),people.values.end(),actor)==people.values.end())return std::nullopt;
 const auto& place=original_place_definition(context_,f);const auto& terminal=*f.terminal_source_ref;
 return MilitaryActionPlacement{MilitaryActionPlacementBuildView{actor,*f.completed_at,
  {{source.source_ref.domain,source.source_ref.kind,source.source_ref.id},source.source_schema},
  SettlementLocationBuildView{place.settlement_id,place.id},{{place.settlement_id,place.id},{terminal.domain,terminal.kind,terminal.id}}}};
});}
template<class Context>Outcome<CheckpointVector<MilitaryActionPlacement>> return_placement_history(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MilitaryReturnSourceRead& returns_,const StableId& actor){return guard<CheckpointVector<MilitaryActionPlacement>>([&]{
 if(!common_.actor(actor))fail("SpatialActorRequired",actor);CheckpointVector<MilitaryActionPlacement> out;
 out.reserve(state_.dynamic_logistics_facts.rows().size());
 for(const auto& item:state_.dynamic_logistics_facts.rows()){
  const auto& f=item.second;if(f.command!="army_return"||f.phase!="completed"||!f.success)continue;
  if(!f.terminal_source_ref)fail("MilitaryReturnPlacementSourceRequired",f.id);const auto& terminal=*f.terminal_source_ref;
  auto local_fields=CheckpointMemoryCharge(0);
  const MomentOwnerSource source{terminal,2};auto actual=take(return_actor_placement(common_,military_,state_,elapsed_,context_,returns_,source,actor));if(actual)out.push_back(std::move(*actual));
 }return out;
});}
}
Outcome<std::optional<MilitaryCompletedReturnSource>> NativeReturnPlacementRead::read_exact_army_return(const StableId& army)const{return return_source_read(common_,military_,state_,elapsed_,context_,*this,army);}
Outcome<std::optional<MilitaryCompletedReturnSource>> NativeReturnSourceRead::read_exact_army_return(const StableId& army)const{return return_source_read(common_,military_,state_,elapsed_,context_,*this,army);}
Outcome<WorldMoment> NativeReturnPlacementRead::read_military_action_placement_source_moment(const MomentOwnerSource& source)const{return return_source_moment(common_,military_,state_,elapsed_,context_,*this,source);}
Outcome<WorldMoment> NativeReturnSourceRead::read_military_action_placement_source_moment(const MomentOwnerSource& source)const{return return_source_moment(common_,military_,state_,elapsed_,context_,*this,source);}
Outcome<std::optional<MilitaryActionPlacement>> NativeReturnPlacementRead::read_military_action_placement(const MomentOwnerSource& source,const StableId& actor)const{return return_actor_placement(common_,military_,state_,elapsed_,context_,*this,source,actor);}
Outcome<std::optional<MilitaryActionPlacement>> NativeReturnSourceRead::read_military_action_placement(const MomentOwnerSource& source,const StableId& actor)const{return return_actor_placement(common_,military_,state_,elapsed_,context_,*this,source,actor);}
Outcome<CheckpointVector<MilitaryActionPlacement>> NativeReturnPlacementRead::read_military_action_placement_history(const StableId& actor)const{return return_placement_history(common_,military_,state_,elapsed_,context_,*this,actor);}
Outcome<CheckpointVector<MilitaryActionPlacement>> NativeReturnSourceRead::read_military_action_placement_history(const StableId& actor)const{return return_placement_history(common_,military_,state_,elapsed_,context_,*this,actor);}
Outcome<std::vector<BoundSpatialLocationWrite>> adopt_exact_return_spatial_bindings_in_candidate(const CommonStore& c,MovementExactState& movement,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactReturnReadContext& x,const StableId& id,Integer first){return guard<std::vector<BoundSpatialLocationWrite>>([&]{
 const auto& f=fact(state,id);if(!f.terminal_source_ref||!f.completed_at||!same_moment(*f.completed_at,take(x.clock.read_current_moment()),x.budget)||first<1)fail("MilitaryReturnActualPlacementRequired",id);
 const MomentOwnerSource source{*f.terminal_source_ref,2};source_local_return(c,m,state,elapsed,x,source);
 NativeReturnPlacementRead original(c,m,state,movement,elapsed,x);
 const auto people=placed_actors(c,m,state,x,f,original);const auto anchor=original_place(x,f);const SpatialCarrierRef carrier=ArmyCarrierRef{f.original_terms.army_id};const auto key=spatial_carrier_key(carrier);
 const auto* prior=movement.spatial_bindings.find(key);if(!prior||!std::holds_alternative<StableMovementWriter>(prior->writer))fail("MilitaryReturnStableArmyWriterRequired",f.original_terms.army_id);
 if(!take(same_actual_movement_position(std::get<StableMovementWriter>(prior->writer).anchor,anchor,x.budget)))fail("MilitaryReturnActualArmyPlaceMismatch",f.original_terms.army_id);
 const auto& drafts=c.actor_location_execution_drafts();if(!c.actor_location_capture_identity()||std::size_t(first-1)>drafts.size()||drafts.size()-std::size_t(first-1)!=people.values.size())fail("MilitaryReturnActualLocationCoverage",id);
 std::vector<BoundSpatialLocationWrite> out;std::size_t sequence=std::size_t(first);
 for(const auto& actor:people.values){
  const auto& draft=drafts[sequence-1];const auto* before=draft.before?std::get_if<InArmyLocation>(&*draft.before):nullptr;
  const auto* prior_actor=movement.spatial_bindings.find("actor:"+actor);const auto* carried=prior_actor?std::get_if<CarriedMovementWriter>(&prior_actor->writer):nullptr;
  if(!before||before->army_id!=f.original_terms.army_id||!carried||spatial_carrier_key(carried->carrier_ref)!=key)fail("MilitaryReturnActorWriterMismatch",actor);
  if(sequence>std::size_t(std::numeric_limits<Integer>::max()))fail("MilitaryActionPlacementSequenceOverflow",actor);
  movement.spatial_bindings.set("actor:"+actor,SpatialWriterBinding{"actor",actor,StableMovementWriter{anchor},source.source_ref});
  out.push_back(take(bind_actual_military_action_location_write(c,movement,original,actor,source,anchor,Integer(sequence),x.budget)));++sequence;
 }
 // The closed Army keeps its actual last anchor; no further live journey can
 // obtain authority from this retained binding alone.
 movement.spatial_bindings.set(key,SpatialWriterBinding{"army",f.original_terms.army_id,StableMovementWriter{anchor},source.source_ref});
 return out;
});}
}
