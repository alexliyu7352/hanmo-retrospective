#include "formation_spatial.hpp"
#include "formation_original_sources.hpp"
#include "resident/kernel/world_copy_plan.hpp"
#include "formation_exact_detail.hpp"
#include "resident/storage/actor_location_execution_draft.hpp"
#include <algorithm>
#include <limits>
#include <charconv>
namespace hanmo::resident {using namespace formation_exact_detail;namespace {
const DynamicMilitaryActionFact2& source_row(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactFormationSourceReadContext& x,const MomentOwnerSource& source){
 if(source.source_schema!=2||source.source_ref.domain!="military"||source.source_ref.kind!="military_action_completion_applied")fail("MilitaryFormationPlacementSourceRequired",source.source_ref.id);
 const DynamicMilitaryActionFact2* result=nullptr;for(const auto& item:s.dynamic_action_facts.rows())if(item.second.terminal_source_ref&&same_source(*item.second.terminal_source_ref,source.source_ref)){if(item.first!=item.second.id)fail("MilitaryFormationFactKeyMismatch",item.first);if(result)fail("DuplicateMilitaryCompletionApplication",source.source_ref.id);result=&item.second;}
 if(!result||result->command!="army_form"||result->phase!="completed"||!result->success||!result->completed_at||!result->result)fail("MilitaryFormationPlacementResultRequired",source.source_ref.id);
 const auto* entry=find_fixed_elapsed(elapsed,result->id);const auto* terms=std::get_if<ArmyFormationTerms>(&result->original_terms);
 if(!entry||!terms||result->rule_version!=2||result->elapsed_source_ref.action_ref!=result->id||result->actor_id!=terms->actor_id||result->original_context||result->result->army_id.empty()||result->result->order_id!=terms->order_id||result->result->ok!=std::optional<bool>{true}||!result->result->office_id.empty()||!same_moment(*result->completed_at,due(*result,x.budget),x.budget)||entry->action_ref!=result->id||entry->execution_source_ref!=result->id||entry->owner_cost_state_ref!=result->id||entry->duration_definition_ref!="army_form_original_1_half_day_v2"||compare(entry->required_units.value,take(fixed_duration_work_units(1,x.scale.units_per_half_day,x.budget)).value,x.budget))fail("MilitaryFormationPlacementResultMismatch",result->id);
 header(c,result->id);validate_completion_header(c,s,*result);
 // Source-local numeric/body identity only. The complete formation validator
 // separately checks historical authority and service; calling it here would
 // recurse through the very actor spatial history it validates.
 const auto service=take(read_fixed_service_completion(elapsed,result->elapsed_source_ref,take(x.clock.read_current_moment()),x.scale,x.budget));if(!service.actual_service.complete||!same_moment(service.started_at,result->started_at,x.budget)||!same_moment(service.completed_at,*result->completed_at,x.budget))fail("ExactFormationFullActualServiceRequired",result->id);
 return *result;
}
const DynamicMilitaryActionFact2& source_row(const CommonStore& c,const MilitaryState& m,const MilitaryActionTimeState& s,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,const MomentOwnerSource& source){
 bound(c,m,s,x);return source_row(c,m,s,elapsed,ExactFormationSourceReadContext{x.access,x.scale,x.clock,x.budget},source);
}
template<class Context>const PlaceDefinition& original_place_definition(const Context& x,const DynamicMilitaryActionFact2& f){
 const auto& terms=std::get<ArmyFormationTerms>(f.original_terms);const auto* place=x.access.common_catalog?x.access.common_catalog->places.find(terms.place_id):nullptr;if(!place||!f.terminal_source_ref)fail("MilitaryFormationOriginalPlaceRequired",f.id);return *place;
}
MovementAnchor original_place(const ExactFormationReadContext& x,const DynamicMilitaryActionFact2& f){
 const auto& place=original_place_definition(x,f);
 return OriginalMovementAnchor{SettlementLocation{place.settlement_id,place.id},*f.terminal_source_ref};
}
}


namespace {
template<class Context>Outcome<std::optional<MilitaryFormationSourceView>> formation_source_read(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const StableId& army_id){return guard<std::optional<MilitaryFormationSourceView>>([&]()->std::optional<MilitaryFormationSourceView>{
 const ArmyRecord* army=nullptr;for(const auto& a:military_.armies)if(a.id==army_id){if(army)fail("MilitaryFormationArmyRepeated",army_id);army=&a;}if(!army)fail("MilitaryFormationArmyRequired",army_id);
 const auto* candidate=state_.dynamic_action_facts.find(army->formation_source);if(!candidate)return std::nullopt;
 if(!candidate->terminal_source_ref)fail("MilitaryFormationCompletionSourceRequired",candidate->id);
 const auto& terminal=*candidate->terminal_source_ref;
 const MomentOwnerSourceBuildView completed_view{{terminal.domain,terminal.kind,terminal.id},2};
 auto local_fields=CheckpointMemoryCharge(0);
 const MomentOwnerSource completed{terminal,2};
 const auto& f=source_row(common_,military_,state_,elapsed_,context_,completed);
 if(f.result->army_id!=army_id)fail("MilitaryFormationArmyIdentityMismatch",army_id);
 const auto& ref=*f.terminal_source_ref;const auto pos=ref.id.rfind(':');Integer ordinal=0;if(pos==StableId::npos)fail("MilitaryFormationOriginalSerialRequired",ref.id);const std::string_view text{ref.id.data()+pos+1,ref.id.size()-pos-1};const auto parsed=std::from_chars(text.data(),text.data()+text.size(),ordinal);if(text.empty()||text.front()=='0'||parsed.ec!=std::errc{}||parsed.ptr!=text.data()+text.size()||ordinal<1||ordinal>=common_.state().next_instance_id)fail("MilitaryFormationOriginalSerialRequired",ref.id);
 return MilitaryFormationSourceView{MilitaryFormationSourceBuildView{&std::get<ArmyFormationTerms>(f.original_terms),{{ref.domain,ref.kind,ref.id},2},{"military","military_action",f.id},*f.completed_at,ordinal}};
});}
template<class Context>Outcome<WorldMoment> formation_source_moment(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MomentOwnerSource& source){return guard<WorldMoment>([&]{return *source_row(common_,military_,state_,elapsed_,context_,source).completed_at;});}
template<class Context>Outcome<std::optional<MilitaryActionPlacement>> formation_actor_placement(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const MomentOwnerSource& source,const StableId& actor){return guard<std::optional<MilitaryActionPlacement>>([&]()->std::optional<MilitaryActionPlacement>{
 if(!common_.actor(actor))fail("SpatialActorRequired",actor);const auto& f=source_row(common_,military_,state_,elapsed_,context_,source);const auto& terms=std::get<ArmyFormationTerms>(f.original_terms);
 if(actor==f.actor_id||std::find(terms.member_actor_ids.begin(),terms.member_actor_ids.end(),actor)==terms.member_actor_ids.end())return std::nullopt;
 const auto& place=original_place_definition(context_,f);const auto& terminal=*f.terminal_source_ref;
 return MilitaryActionPlacement{MilitaryActionPlacementBuildView{actor,*f.completed_at,
  {{source.source_ref.domain,source.source_ref.kind,source.source_ref.id},source.source_schema},
  InArmyLocationBuildView{f.result->army_id},{{place.settlement_id,place.id},{terminal.domain,terminal.kind,terminal.id}}}};
});}
template<class Context>Outcome<CheckpointVector<MilitaryActionPlacement>> formation_placement_history(const CommonStore& common_,const MilitaryState& military_,const MilitaryActionTimeState& state_,const ActionElapsedStore& elapsed_,const Context& context_,const StableId& actor){return guard<CheckpointVector<MilitaryActionPlacement>>([&]{
 if(!common_.actor(actor))fail("SpatialActorRequired",actor);CheckpointVector<MilitaryActionPlacement> out;
 out.reserve(state_.dynamic_action_facts.rows().size());
 for(const auto& item:state_.dynamic_action_facts.rows()){
  const auto& f=item.second;if(f.command!="army_form"||f.phase!="completed"||!f.success)continue;
  if(!f.terminal_source_ref)fail("MilitaryFormationPlacementSourceRequired",f.id);const auto& terminal=*f.terminal_source_ref;
  auto local_fields=CheckpointMemoryCharge(0);
  const MomentOwnerSource source{terminal,2};auto actual=take(formation_actor_placement(common_,military_,state_,elapsed_,context_,source,actor));if(actual)out.push_back(std::move(*actual));
 }return out;
});}
}
Outcome<std::optional<MilitaryFormationSourceView>> NativeFormationPlacementRead::read_exact_army_formation(const StableId& army_id)const{return formation_source_read(common_,military_,state_,elapsed_,context_,army_id);}
Outcome<std::optional<MilitaryFormationSourceView>> NativeFormationSourceRead::read_exact_army_formation(const StableId& army_id)const{return formation_source_read(common_,military_,state_,elapsed_,context_,army_id);}
Outcome<WorldMoment> NativeFormationPlacementRead::read_military_action_placement_source_moment(const MomentOwnerSource& source)const{return formation_source_moment(common_,military_,state_,elapsed_,context_,source);}
Outcome<WorldMoment> NativeFormationSourceRead::read_military_action_placement_source_moment(const MomentOwnerSource& source)const{return formation_source_moment(common_,military_,state_,elapsed_,context_,source);}
Outcome<std::optional<MilitaryActionPlacement>> NativeFormationPlacementRead::read_military_action_placement(const MomentOwnerSource& source,const StableId& actor)const{return formation_actor_placement(common_,military_,state_,elapsed_,context_,source,actor);}
Outcome<std::optional<MilitaryActionPlacement>> NativeFormationSourceRead::read_military_action_placement(const MomentOwnerSource& source,const StableId& actor)const{return formation_actor_placement(common_,military_,state_,elapsed_,context_,source,actor);}
Outcome<CheckpointVector<MilitaryActionPlacement>> NativeFormationPlacementRead::read_military_action_placement_history(const StableId& actor)const{return formation_placement_history(common_,military_,state_,elapsed_,context_,actor);}
Outcome<CheckpointVector<MilitaryActionPlacement>> NativeFormationSourceRead::read_military_action_placement_history(const StableId& actor)const{return formation_placement_history(common_,military_,state_,elapsed_,context_,actor);}
Outcome<std::vector<BoundSpatialLocationWrite>> adopt_exact_formation_spatial_bindings_in_candidate(const CommonStore& c,MovementExactState& movement,const MilitaryState& m,const MilitaryActionTimeState& state,const ActionElapsedStore& elapsed,const ExactFormationReadContext& x,const StableId& id,Integer first){return guard<std::vector<BoundSpatialLocationWrite>>([&]{
 const auto& f=fact(state,id);if(!f.terminal_source_ref||!f.completed_at||!same_moment(*f.completed_at,take(x.clock.read_current_moment()),x.budget)||first<1)fail("MilitaryFormationActualPlacementRequired",id);
 const MomentOwnerSource source{*f.terminal_source_ref,2};source_row(c,m,state,elapsed,x,source);const auto& terms=std::get<ArmyFormationTerms>(f.original_terms);const auto anchor=original_place(x,f);const SpatialCarrierRef carrier=ArmyCarrierRef{f.result->army_id};
 const auto army_key=spatial_carrier_key(carrier);if(movement.spatial_bindings.find(army_key))fail("ArmyFormationSpatialWriterAlreadyExists",f.result->army_id);
 const auto& drafts=c.actor_location_execution_drafts();if(!c.actor_location_capture_identity()||std::size_t(first-1)>drafts.size())fail("MilitaryFormationActualLocationCaptureRequired",id);
 NativeFormationPlacementRead original(c,m,state,movement,elapsed,x);std::vector<BoundSpatialLocationWrite> out;std::size_t sequence=std::size_t(first);
 movement.spatial_bindings.set(army_key,SpatialWriterBinding{"army",f.result->army_id,StableMovementWriter{anchor},source.source_ref});
 for(const auto& actor:terms.member_actor_ids)if(actor!=f.actor_id){
  const auto placement=take(original.read_military_action_placement(source,actor));if(!placement)fail("MilitaryFormationActualPlacementRequired",actor);
  const auto* previous=movement.spatial_bindings.find("actor:"+actor);if(previous&&!std::holds_alternative<StableMovementWriter>(previous->writer))fail("MilitaryFormationActorWriterBusy",actor);
  movement.spatial_bindings.set("actor:"+actor,SpatialWriterBinding{"actor",actor,CarriedMovementWriter{carrier},source.source_ref});
  if(sequence>std::size_t(std::numeric_limits<Integer>::max()))fail("MilitaryActionPlacementSequenceOverflow",actor);out.push_back(take(bind_actual_military_action_location_write(c,movement,original,actor,source,placement->actual_anchor,Integer(sequence),x.budget)));++sequence;
 }
 if(std::size_t(sequence-1)!=drafts.size())fail("MilitaryFormationActualLocationCoverage",id);return out;
});}
}
