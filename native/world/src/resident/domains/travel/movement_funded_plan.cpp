// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
#include "movement_funded_plan_detail.hpp"
#include "movement_command.hpp"
#include "movement_exact_phase.hpp"
#include "movement_exact_detail.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {using namespace movement_detail;
Outcome<AcceptedTaskPersonalMovementFunding> movement_funded_plan_detail::accepted_basis(const TaskPersonalMovementFundingContext& x,const SourceRef& plan,const StableId& actor_id,const TravelDestination& destination,const StableId& pending){return guard<AcceptedTaskPersonalMovementFunding>([&]{
 const auto& common=x.funding.common;if(!x.movement_actions.bound_to(common,x.movement))fail("TaskMovementFundingCandidateMismatch",actor_id);if(take(x.travel_services.active_army(actor_id)))fail("ArmyGroupMovementEntryRequired",actor_id);
 const auto* site=std::get_if<StableId>(&destination);const auto* city=site?x.travel_catalog.settlements.find(*site):nullptr;if(!city||city->entry_place.empty())fail("TaskPersonalFundingDestinationSiteRequired",actor_id);
 const auto actual=take(read_accepted_task_personal_movement_funding(x,plan,actor_id,OriginalMovementAnchor{SettlementLocation{*site,city->entry_place},{"travel","catalog_settlement",*site}}));
 if(!pending.empty()){const auto* action=common.action(pending);const auto* members=common.members(actor_id);if(!action||action->kind!="travel"||action->actor_id!=actor_id||!members||action->participants!=*members||action->start_tick!=actual.actual_at.half_day_tick||x.travel.actions.find(pending)||x.travel.road_actions.find(pending))fail("TaskPersonalNewActionRequired",pending);for(const auto& row:x.movement.exertion_bases.rows())if(const auto* execution=std::get_if<PersonalMovementExecution>(&row.second.execution))if(execution->action_id==pending)fail("TaskPersonalExecutionAlreadyInstalled",pending);}
 return actual;
});}
Outcome<TravelQuote> plan_task_funded_personal_movement(const TaskPersonalMovementFundingContext& x,const SourceRef& plan,const StableId& actor,const TravelDestination& destination,const std::vector<StableId>& via){if(x.travel_catalog.world_map_id.empty())return movement_funded_plan_detail::road(x,plan,actor,destination,via,{});return movement_funded_plan_detail::terrain(x,plan,actor,destination,via,{});}
Outcome<StableId> begin_task_funded_personal_movement_in_candidate(CommonStore& common,TravelState& travel,MovementExactState& movement,const TaskPersonalMovementFundingContext& x,const MotionExactAuthorSelection& selection,const StableId& action_id,const SourceRef& accepted_plan,const TravelCommand& command,std::vector<BoundSpatialLocationWrite>& writes,ExactArithmeticBudget budget){return guard<StableId>([&]{
 if(selection.travel_schema!=5||selection.private_transport_funding_schema!=1)fail("TaskPersonalFundingProfileRequired",action_id);
 if(&x.funding.common!=&common||&x.travel!=&travel||&x.movement!=&movement||!x.movement_actions.bound_to(common,movement))fail("TaskMovementFundingCandidateMismatch",action_id);
 const auto* action=common.action(action_id);if(!action||action->kind!="travel"||!command.has_destination)fail("TaskPersonalNewActionRequired",action_id);const auto actor_id=action->actor_id;
 const auto accepted=take(movement_funded_plan_detail::accepted_basis(x,accepted_plan,actor_id,command.destination,action_id));
 const auto quote=take(x.travel_catalog.world_map_id.empty()?movement_funded_plan_detail::road(x,accepted_plan,actor_id,command.destination,command.via,action_id):movement_funded_plan_detail::terrain(x,accepted_plan,actor_id,command.destination,command.via,action_id));
 if(action->remaining_ticks!=quote.duration)fail("TaskPersonalPlannedActionDurationMismatch",action_id);
 const auto& actual_payer=quote.payer_wallet_id;if(actual_payer!=accepted.payer_wallet_id)fail("TaskPersonalFundingPayerMismatch",action_id);
 const auto prefix_id=take(begin_exact_personal_movement_in_candidate(common,travel,movement,x.travel_catalog,x.travel_services,x.funding.clock,action_id,quote,&command,writes,budget));
 const auto* prefix=movement.prefixes.find(prefix_id);const auto* unit=prefix?movement.units.find(prefix->unit_ref):nullptr;
 auto* basis=unit&&unit->effort_basis_refs.size()==1?movement.exertion_bases.find(unit->effort_basis_refs.front().id):nullptr;
 const auto* execution=basis?std::get_if<PersonalMovementExecution>(&basis->execution):nullptr;
 if(!unit||unit->ordinal!=0||!basis||!execution||execution->action_id!=action_id||execution->segment_ordinal!=0||basis->accepted_funding_plan||!same_moment(basis->opened_at,accepted.actual_at,budget))fail("TaskPersonalFundingOpeningMismatch",action_id);
 basis->accepted_funding_plan=MomentOwnerSource{accepted.accepted_plan,1};
 const auto proof=take(x.movement_actions.read_original_execution_funding_plan(action_id));if(!proof||!same_source(proof->source_ref,accepted.accepted_plan)||proof->source_schema!=1)fail("TaskPersonalFundingOpeningMismatch",action_id);
 return prefix_id;
});}
}
