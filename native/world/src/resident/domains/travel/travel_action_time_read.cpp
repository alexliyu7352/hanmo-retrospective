#include "travel_action_time_read.hpp"
#include "movement_exact_detail.hpp"
#include "hanmo/resident_world_api.hpp"
namespace hanmo::resident {using namespace movement_detail;
const ActionTimeOriginalSourceRead& NativeTravelActionTimeRead::owner(const ActionTimeRegistration& r)const{
 if(r.owner_domain!="travel")fail("TravelActionTimeOwnerMismatch",r.owner_domain);
 if(r.action_kind=="travel"){if(!movement_)fail("MovementActionTimeReaderRequired");return *movement_;}
 if(r.action_kind=="cargo_handling"){if(!cargo_)fail("CargoHandlingActionTimeReaderRequired");return *cargo_;}
 fail("UnsupportedTravelActionTimeKind",r.action_kind);
}
Status NativeTravelActionTimeRead::enumerate_action_time_owners(const ActionTimeRegistration& r,ActionTimeOwnerIdSink& output)const{return status([&]{take(owner(r).enumerate_action_time_owners(r,output));});}
Outcome<ActionTimeOwnerProof> NativeTravelActionTimeRead::read_action_time_owner(const ActionTimeRegistration& r,const StableId& id)const{return guard<ActionTimeOwnerProof>([&]{return take(owner(r).read_action_time_owner(r,id));});}
Outcome<WorldMoment> NativeTravelActionTimeRead::read_action_time_source(const ElapsedTimeSource& source)const{return guard<WorldMoment>([&]{
 const auto* actual=std::get_if<MomentOwnerSource>(&source);if(!actual||actual->source_schema!=1||actual->source_ref.domain!="travel")fail("TravelActualTimeSourceRequired");const auto& kind=actual->source_ref.kind;
 if(kind=="cargo_handling_execution"||kind=="cargo_handling_work_result"||kind=="cargo_handling_result"){if(!cargo_)fail("CargoHandlingActionTimeReaderRequired");return take(cargo_->read_action_time_source(source));}
 if(kind=="personal_movement_terminal"||kind=="travel_exertion_basis"||kind=="movement_arrival"||kind=="travel_exertion"||kind=="movement_contact"||kind=="movement_expense"||kind=="terrain_credit_disposition"||kind=="movement_unit_close"){if(!movement_)fail("MovementActionTimeReaderRequired");return take(movement_->read_action_time_source(source));}
 fail("UnsupportedTravelActionTimeSource",kind);
});}
Status NativeTravelActionTimeRead::validate_fixed_action_sources(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const ActionElapsedState& e,const std::vector<FixedServiceTransition>& t)const{return status([&]{take(owner(r).validate_fixed_action_sources(r,p,e,t));});}
Status NativeTravelActionTimeRead::validate_shared_action_sources(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const SharedWorkElapsedState& e)const{return status([&]{take(owner(r).validate_shared_action_sources(r,p,e));});}
Outcome<RunStop> NativeTravelActionTimeRead::read_action_continuation_stop(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& at)const{return guard<RunStop>([&]{return take(owner(r).read_action_continuation_stop(r,p,at));});}
Outcome<OriginalActionRemaining> NativeTravelActionTimeRead::read_original_action_remaining(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& at)const{return guard<OriginalActionRemaining>([&]{return take(owner(r).read_original_action_remaining(r,p,at));});}
Outcome<StableId> NativeTravelActionTimeRead::read_gate_result_id(const SharedWorkGateResult&)const{return {false,{},common_failure("TravelDoesNotOwnSharedWorkGate").error};}
Outcome<SharedWorkGateResult> NativeTravelActionTimeRead::read_original_gate_result(const StableId&)const{return {false,{},common_failure("TravelDoesNotOwnSharedWorkGate").error};}
Status NativeTravelActionTimeRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("TravelDoesNotOwnSharedWorkGate");}
}
