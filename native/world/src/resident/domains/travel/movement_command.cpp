#include "movement_command.hpp"
#include "movement_exact_phase.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {using namespace movement_detail;
Outcome<StableId> begin_exact_personal_movement_in_candidate(CommonStore& common,TravelState& travel,MovementExactState& movement,const TravelCatalog& catalog,const TravelServices& services,const CurrentMomentRead& clock,const StableId& action_id,const TravelQuote& quote,const TravelCommand* command,std::vector<BoundSpatialLocationWrite>& writes,ExactArithmeticBudget b){return guard<StableId>([&]{
 if(!common.actor_location_capture_identity())fail("SpatialLocationCaptureRequired",action_id);
 const auto* action=common.action(action_id);if(!action||action->kind!="travel"||quote.duration<=0)fail("ExactTravelOriginalActionRequired",action_id);const auto actor_id=action->actor_id;
 if(take(services.active_army(actor_id)))fail("ArmyGroupMovementEntryRequired",actor_id);
 const auto at=take(clock.read_current_moment());if(action->start_tick!=at.half_day_tick)fail("ExactTravelActionStartMismatch",action_id);
 const auto before_count=common.actor_location_execution_drafts().size();std::optional<TravelPlanInstallationDraft> original;
 take(begin_travel(common,travel,catalog,action_id,quote,command,&original));if(!original||original->before.action.id!=action_id||original->after.action.id!=action_id)fail("TravelPlanInstallationSourceRequired",action_id);
 const auto prefix_id=take(install_exact_personal_movement_after_begin_in_candidate(common,travel,movement,catalog,services,action_id,at,b));
 const auto* prefix=movement.prefixes.find(prefix_id);const auto* unit=prefix?movement.units.find(prefix->unit_ref):nullptr;
 if(!prefix||!unit||unit->effort_basis_refs.size()!=1||common.actor_location_execution_drafts().size()!=before_count+1)fail("TravelOpeningLocationCoverageMismatch",action_id);
 const auto actual=take(read_movement_position(*prefix,b));writes.push_back(take(bind_actual_spatial_location_write(common,movement,actor_id,unit->effort_basis_refs.front(),1,actual,Integer(before_count+1),b)));
 return prefix_id;
});}
Outcome<SourceRef> apply_exact_personal_control_command_in_candidate(CommonStore& common,TravelState& travel,MovementExactState& movement,const TravelCatalog& catalog,const TravelServices& services,const CurrentMomentRead& clock,const PreparedTravelOp& prepared,ExactArithmeticBudget b){return guard<SourceRef>([&]{
 PersonalMovementControlOperation operation;
 if(std::holds_alternative<PauseTravelCommand>(prepared.command))operation=PersonalMovementControlOperation::pause;
 else if(std::holds_alternative<ResumeTravelCommand>(prepared.command))operation=PersonalMovementControlOperation::resume;
 else if(std::holds_alternative<TravelWaitCommand>(prepared.command))operation=PersonalMovementControlOperation::wait;
 else fail("ExactTravelControlOperationRequired",prepared.actor_id);
 if(prepared.quote)fail("UnexpectedTravelControlQuote",prepared.actor_id);
 // Repeat the original current eligibility and error order before any write.
 take(prepare_travel_control(common,travel,catalog,services,prepared.actor_id,prepared.command));
 return take(control_exact_personal_movement_in_candidate(common,travel,movement,catalog,services,prepared.actor_id,operation,take(clock.read_current_moment()),b));
});}
}
