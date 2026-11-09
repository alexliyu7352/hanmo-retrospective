#include "movement_control.hpp"
#include "movement_exact_detail.hpp"
#include "../../storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {using namespace movement_detail;
Outcome<SourceRef> control_exact_personal_movement_in_candidate(CommonStore& common,TravelState& travel,MovementExactState& movement,const TravelCatalog& catalog,const TravelServices& services,const StableId& actor_id,PersonalMovementControlOperation operation,const WorldMoment& at,ExactArithmeticBudget b){return guard<SourceRef>([&]{const auto* actor=common.actor(actor_id);const auto* action=actor?common.action(actor->action_id):nullptr;const auto* binding=movement.spatial_bindings.find("actor:"+actor_id);const auto* world=binding?std::get_if<WorldMovementWriter>(&binding->writer):nullptr;const auto* prefix=world?movement.prefixes.find(world->prefix_id):nullptr;const auto* unit=prefix?movement.units.find(prefix->unit_ref):nullptr;if(!action||!prefix||!unit||action->kind!="travel"||action->actor_id!=actor_id||execution_action(prefix->execution)!=action->id||unit->effort_basis_refs.empty()||!std::holds_alternative<MovementUnitOpen>(unit->status))fail("PersonalMovementControlSourceRequired",actor_id);if(operation!=PersonalMovementControlOperation::pause&&operation!=PersonalMovementControlOperation::resume&&operation!=PersonalMovementControlOperation::wait)fail("UnknownPersonalMovementControl",actor_id);const auto action_id=action->id,prefix_id=prefix->id;if(operation==PersonalMovementControlOperation::resume){take(can_step_travel(common,travel,catalog,services,*action,true));if(std::holds_alternative<MovementActive>(prefix->lifecycle))return take(read_personal_movement_immutable_head(common,movement,*prefix,actor_id,b)).source_ref;if(!std::holds_alternative<MovementHeld>(prefix->lifecycle))fail("MovementNotHeld",prefix_id);}else if(std::holds_alternative<MovementHeld>(prefix->lifecycle)){if(take(compare_world_moments(at,prefix->last_settled_at,b))<0)fail("InvalidMovementHold",prefix_id);return take(read_personal_movement_immutable_head(common,movement,*prefix,actor_id,b)).source_ref;}else if(!std::holds_alternative<MovementActive>(prefix->lifecycle))fail("MovementNotRunning",prefix_id);PersonalMovementControlSource source{take(common.next_id("personal_movement_control")),actor_id,action_id,prefix_id,unit->id,basis_id(prefix->basis),operation,at,prefix->cursor,unit->effort_basis_refs.front()};const auto ref=SourceRef{"travel","personal_movement_control",source.id};if(operation==PersonalMovementControlOperation::resume)take(resume_movement_prefix(movement,prefix_id,at,ref,b));else take(hold_movement_prefix(movement,prefix_id,at,ref,b));const bool pause=operation!=PersonalMovementControlOperation::resume;if(auto* params=travel.actions.find(action_id))params->paused=pause;else if(auto* params=travel.road_actions.find(action_id)){params->has_paused=true;params->paused=pause;}else fail("MovementOriginalPayloadRequired",action_id);const auto id=source.id;movement.personal_controls.set(id,std::move(source));take(common.append_fact({id,"personal_movement_control","travel"}));take(sync_exact_personal_action_time_projection(common,travel,movement,catalog,services,action_id,b));return ref;});}
namespace {
void validate_control_source(const CommonRead& common,const MovementExactState& movement,const StableId& key,const PersonalMovementControlSource& source,ExactArithmeticBudget b){
 const auto* unit=movement.units.find(source.unit_ref);const auto* effort=movement.exertion_bases.find(source.original_execution_source.id);
 if(key!=source.id||source.original_execution_source.domain!="travel"||source.original_execution_source.kind!="travel_exertion_basis"||!unit||!effort||effort->unit_ref!=unit->id||!effort->original_action||effort->original_action->id!=source.action_id||effort->original_action->actor_id!=source.actor_id||execution_action(unit->execution)!=source.action_id||take(compare_world_moments(effort->opened_at,source.occurred_at,b))>0)fail("PersonalMovementControlSourceMismatch",source.id);
 if(source.operation!=PersonalMovementControlOperation::pause&&source.operation!=PersonalMovementControlOperation::resume&&source.operation!=PersonalMovementControlOperation::wait)fail("UnknownPersonalMovementControl",source.id);
 const TravelExertionBasis* geometry=nullptr;for(const auto& ref:unit->effort_basis_refs){const auto* e=movement.exertion_bases.find(ref.id);if(e&&basis_id(e->actual_basis)==source.basis_ref)geometry=e;}
 if(!geometry||compare(source.cursor,basis_limit(geometry->actual_basis,b),b)>0)fail("MovementControlGeometryRequired",source.id);
 if(const auto* store=dynamic_cast<const CommonStore*>(&common)){const auto* head=store->state().facts.find(source.id);if(!head||head->kind!="personal_movement_control"||head->domain!="travel")fail("MovementControlHeadRequired",source.id);}
}
const PersonalMovementControlSource& read_control_source_row(const CommonRead& common,const MovementExactState& movement,const SourceRef& ref,Integer schema,ExactArithmeticBudget b){
 if(schema!=1||ref.domain!="travel"||ref.kind!="personal_movement_control")fail("PersonalMovementControlSourceRequired",ref.id);
 const auto* source=movement.personal_controls.find(ref.id);if(!source)fail("PersonalMovementControlSourceRequired",ref.id);
 validate_control_source(common,movement,ref.id,*source,b);return *source;
}
}
Outcome<PersonalMovementControlRead> read_personal_movement_control_source(const CommonRead& common,const MovementExactState& movement,const SourceRef& ref,Integer schema,ExactArithmeticBudget b){return guard<PersonalMovementControlRead>([&]{
 const auto& source=read_control_source_row(common,movement,ref,schema,b);
 return PersonalMovementControlRead{ref,schema,source.actor_id,source.action_id,source.operation,source.occurred_at,source.original_execution_source};
});}
Outcome<MomentOwnerSourceBuildView> read_personal_movement_immutable_head_view(const CommonRead& common,const MovementExactState& movement,const MovementPrefixState& prefix,const StableId& actor,ExactArithmeticBudget b){return guard<MomentOwnerSourceBuildView>([&]{
 const auto* unit=movement.units.find(prefix.unit_ref);if(!unit||execution_key(unit->execution)!=execution_key(prefix.execution))fail("ActorSpatialUnitRequired",actor);
 const TravelExertionBasis* effort=nullptr;const SourceRef* basis_ref=nullptr;
 for(const auto& ref:unit->effort_basis_refs){const auto* candidate=movement.exertion_bases.find(ref.id);if(candidate&&basis_id(candidate->actual_basis)==basis_id(prefix.basis)&&candidate->original_action&&candidate->original_action->actor_id==actor&&candidate->original_action->id==execution_action(prefix.execution)&&std::find(candidate->actual_participants.begin(),candidate->actual_participants.end(),actor)!=candidate->actual_participants.end()){if(effort)fail("AmbiguousActorSpatialEffort",actor);effort=candidate;basis_ref=&ref;}}
 if(!effort)fail("ActorSpatialEffortRequired",actor);
 const auto view=[](const SourceRef& ref){return MomentOwnerSourceBuildView{{ref.domain,ref.kind,ref.id},1};};
 if(const auto* held=std::get_if<HeldMovementMap>(&prefix.activity_world_map)){if(!source_valid(held->actual_hold_source))fail("ActorSpatialHoldSourceRequired",actor);return view(held->actual_hold_source);}
 const auto& running=std::get<RunningMovementMap>(prefix.activity_world_map);
 if(running.rate_source.domain=="travel"&&running.rate_source.kind=="personal_movement_control"){
  const auto& control=read_control_source_row(common,movement,running.rate_source,1,b);
  if(control.actor_id!=actor||control.operation!=PersonalMovementControlOperation::resume||control.prefix_id!=prefix.id||control.unit_ref!=unit->id||control.basis_ref!=basis_id(prefix.basis)||!same_moment(control.occurred_at,running.world_from,b)||compare(control.cursor,running.cursor_at_from,b))fail("ActorSpatialResumeHeadMismatch",actor);
  return view(running.rate_source);
 }
 return view(*basis_ref);
});}
Outcome<MomentOwnerSource> read_personal_movement_immutable_head(const CommonRead& common,const MovementExactState& movement,const MovementPrefixState& prefix,const StableId& actor,ExactArithmeticBudget b){return guard<MomentOwnerSource>([&]{
 const auto head=take(read_personal_movement_immutable_head_view(common,movement,prefix,actor,b));
 return MomentOwnerSource{{StableId(head.source_ref.domain),StableId(head.source_ref.kind),StableId(head.source_ref.id)},head.source_schema};
});}
Status validate_personal_movement_controls(const CommonRead& common,const MovementExactState& movement,ExactArithmeticBudget b){return status([&]{for(const auto& row:movement.personal_controls.rows())validate_control_source(common,movement,row.first,row.second,b);});}
}
