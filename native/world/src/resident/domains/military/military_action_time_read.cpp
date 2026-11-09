#include "military_action_time_read.hpp"
#include "hanmo/resident_world_api.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include <type_traits>
namespace hanmo::resident {using namespace movement_detail;namespace {
// A borrowed forwarding gate, not another owner collection. Emitted IDs are
// checked against their retained original key before reaching the shared sink.
template<class Check>class CheckedMilitaryOwnerIdSink final:public ActionTimeOwnerIdSink {
 Check& check_;ActionTimeOwnerIdSink& output_;
public:
 CheckedMilitaryOwnerIdSink(Check& check,ActionTimeOwnerIdSink& output):check_(check),output_(output){}
 Status append(std::string_view id)override{auto status=check_(id);if(!status.ok)return status;return output_.append(id);}
};
ActionTimeRegistration registration(const StableId& kind){
 if(kind=="office_accept")return exact_office_action_registration();
 if(kind=="army_form")return exact_formation_action_registration();
 if(kind=="military_train")return exact_military_training_registration();
 if(kind=="army_return")return exact_return_action_registration();
 if(kind=="army_resupply")return exact_resupply_action_registration();
 throw Failure{common_failure("MilitaryActionTimeOriginalKindRequired",kind,ErrorKind::unsupported_capability).error};
}
void same_registration(const ActionTimeRegistration& a,const ActionTimeRegistration& z){
 if(a.action_kind!=z.action_kind||a.owner_domain!=z.owner_domain||a.profile_id!=z.profile_id||a.profile_version!=z.profile_version||a.source_schema!=z.source_schema||a.mode!=z.mode||a.remaining_meaning!=z.remaining_meaning||a.shared_gate_kind!=z.shared_gate_kind||a.retained_wait_phases!=z.retained_wait_phases)fail("MilitaryActionTimeRegistrationMismatch",a.action_kind);
}
}
void NativeMilitaryActionTimeRead::check_bindings()const{
 if(selected_.office_accept!=bool(contexts_.office)||selected_.army_form!=bool(contexts_.formation)||selected_.military_train!=bool(contexts_.training)||selected_.army_return!=bool(contexts_.returning)||selected_.army_resupply!=bool(contexts_.resupply))throw Failure{common_failure("MilitaryActionTimeSelectedOriginalReadRequired",{},ErrorKind::unsupported_capability).error};
 const MilitaryCatalog* catalog=nullptr;const MilitaryAccess* access=nullptr;const TimeScaleRef* scale=nullptr;const CurrentMomentRead* clock=nullptr;
 auto check=[&](const auto& optional){if(!optional)return;const auto& x=*optional;
  if((catalog&&catalog!=&x.catalog)||(access&&access!=&x.access)||(scale&&scale!=&x.scale)||(clock&&clock!=&x.clock))fail("CrossCandidateMilitaryActionTimeRead");
  catalog=&x.catalog;access=&x.access;scale=&x.scale;clock=&x.clock;
  if constexpr(std::is_same_v<std::decay_t<decltype(x)>,ExactTrainingReadContext>){if(!x.original.bound_to(common_,military_))fail("CrossCandidateMilitaryActionTimeOriginal");}
  else if(!x.original.bound_to(common_,military_,state_))fail("CrossCandidateMilitaryActionTimeOriginal");
 };
 check(contexts_.office);check(contexts_.formation);check(contexts_.training);check(contexts_.returning);check(contexts_.resupply);
}
NativeMilitaryActionTimeRead::Reader NativeMilitaryActionTimeRead::reader(const StableId& kind)const{
 check_bindings();
 if(kind=="office_accept"&&selected_.office_accept)return NativeOfficeActionTimeRead(common_,military_,state_,elapsed_,*contexts_.office);
 if(kind=="army_form"&&selected_.army_form)return NativeFormationActionTimeRead(common_,military_,state_,elapsed_,*contexts_.formation);
 if(kind=="military_train"&&selected_.military_train)return NativeMilitaryTrainingActionTimeRead(common_,military_,state_,elapsed_,*contexts_.training);
 if(kind=="army_return"&&selected_.army_return)return NativeReturnActionTimeRead(common_,military_,state_,elapsed_,*contexts_.returning);
 if(kind=="army_resupply"&&selected_.army_resupply)return NativeResupplyActionTimeRead(common_,military_,state_,elapsed_,*contexts_.resupply);
 throw Failure{common_failure("MilitaryActionTimeSelectedKindRequired",kind,ErrorKind::unsupported_capability).error};
}
StableId NativeMilitaryActionTimeRead::original_action_kind(const StableId& id)const{
 if(id.empty())fail("MilitaryActionTimeOriginalActionRequired",id);const auto* action=state_.dynamic_action_facts.find(id);const auto* logistics=state_.dynamic_logistics_facts.find(id);const auto* training=state_.dynamic_training_facts.find(id);
 if(Integer(bool(action))+Integer(bool(logistics))+Integer(bool(training))!=1)fail("MilitaryActionTimeOriginalOwnerAmbiguous",id);
 for(const auto& old:military_.action_facts)if(old.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 for(const auto& old:military_.logistics_facts)if(old.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 for(const auto& old:military_.training_facts)if(old.id==id)fail("DuplicateLegacyExactMilitaryAction",id);
 if(action){if(action->id!=id||(action->command!="office_accept"&&action->command!="army_form"))fail("MilitaryActionTimeOriginalActionKindMismatch",id);return action->command;}
 if(logistics){if(logistics->id!=id||(logistics->command!="army_return"&&logistics->command!="army_resupply"))fail("MilitaryActionTimeOriginalActionKindMismatch",id);return logistics->command;}
 if(training->id!=id)fail("MilitaryActionTimeOriginalActionKeyMismatch",id);return "military_train";
}
NativeMilitaryActionTimeRead::Reader NativeMilitaryActionTimeRead::registered_reader(const ActionTimeRegistration& r)const{same_registration(r,registration(r.action_kind));return reader(r.action_kind);}
Status NativeMilitaryActionTimeRead::validate_selected_sources()const{return status([&]{
 check_bindings();auto check=[&](const auto& rows){for(const auto& row:rows.rows()){const auto kind=original_action_kind(row.first);auto original=reader(kind);take(std::visit([&](const auto& value){return value.read_action_time_owner(registration(kind),row.first);},original));}};
 check(state_.dynamic_action_facts);check(state_.dynamic_logistics_facts);check(state_.dynamic_training_facts);
 for(const auto& item:common_.state().facts.rows())if(item.second.domain=="military"){
  const auto& source=item.second;Integer owners=0;
  if(source.kind=="military_action"){
   owners+=bool(state_.dynamic_action_facts.find(item.first));owners+=bool(state_.dynamic_logistics_facts.find(item.first));
   for(const auto& old:military_.action_facts)if(old.id==item.first)++owners;
   for(const auto& old:military_.logistics_facts)if(old.id==item.first)++owners;
  }else if(source.kind=="military_office_completion_applied"){
   if(!selected_.office_accept||!contexts_.office)fail("MilitaryActionTimeUnselectedOfficeSource",item.first);
   for(const auto& row:state_.dynamic_action_facts.rows())if(row.second.terminal_source_ref&&row.second.terminal_source_ref->id==item.first){
    const auto* original=take(read_exact_office_completion_application(common_,military_,state_,elapsed_,*contexts_.office,*row.second.terminal_source_ref,2));
    if(original!=&row.second)fail("MilitaryActionTimeSourceHeaderCoverage",item.first);++owners;
   }
  }else if(source.kind=="military_training"){
   owners+=bool(state_.dynamic_training_facts.find(item.first));for(const auto& old:military_.training_facts)if(old.id==item.first)++owners;
  }else continue;
  if(source.id!=item.first||owners!=1)fail("MilitaryActionTimeSourceHeaderCoverage",item.first);
 }
 for(const auto& item:common_.state().actions.rows()){
  const auto& action=item.second;const bool selected=(action.kind=="office_accept"&&selected_.office_accept)||(action.kind=="army_form"&&selected_.army_form)||(action.kind=="military_train"&&selected_.military_train)||(action.kind=="army_return"&&selected_.army_return)||(action.kind=="army_resupply"&&selected_.army_resupply);
  if(selected&&(action.id!=item.first||original_action_kind(item.first)!=action.kind))fail("MilitaryActionTimeActiveOwnerCoverage",item.first);
 }
 for(const auto& row:state_.training_participation_spans.rows())if(original_action_kind(row.second.action_ref)!="military_train"||!selected_.military_train)fail("MilitaryActionTimeUnselectedTrainingSource",row.first);
 for(const auto& row:state_.training_period_witnesses.rows())if(original_action_kind(row.second.action_ref)!="military_train"||!selected_.military_train)fail("MilitaryActionTimeUnselectedTrainingSource",row.first);
 for(const auto& row:state_.drill_freeze_sources.rows())if(original_action_kind(row.second.action_ref)!="military_train"||!selected_.military_train)fail("MilitaryActionTimeUnselectedTrainingSource",row.first);
 for(const auto& row:state_.dynamic_return_facts.rows())if(original_action_kind(row.second.logistics_action_ref)!="army_return"||!selected_.army_return)fail("MilitaryActionTimeUnselectedReturnSource",row.first);
});}
Status NativeMilitaryActionTimeRead::enumerate_action_time_owners(const ActionTimeRegistration& r,ActionTimeOwnerIdSink& output)const{return status([&]{
 auto original=registered_reader(r);
 auto check=[&](std::string_view id){return status([&]{
  const StableId* actual=nullptr;
  auto locate=[&](const auto& rows){for(const auto& row:rows.rows())if(row.first==id){if(actual)fail("MilitaryActionTimeOriginalOwnerAmbiguous",row.first);actual=&row.first;}};
  locate(state_.dynamic_action_facts);locate(state_.dynamic_logistics_facts);locate(state_.dynamic_training_facts);
  if(!actual)fail("MilitaryActionTimeOriginalActionRequired");
  if(original_action_kind(*actual)!=r.action_kind)fail("MilitaryActionTimeOriginalOwnerMismatch",*actual);
 });};
 CheckedMilitaryOwnerIdSink<decltype(check)> sink(check,output);
 take(std::visit([&](const auto& value){return value.enumerate_action_time_owners(r,sink);},original));
});}
Outcome<ActionTimeOwnerProof> NativeMilitaryActionTimeRead::read_action_time_owner(const ActionTimeRegistration& r,const StableId& id)const{return guard<ActionTimeOwnerProof>([&]{auto original=registered_reader(r);if(original_action_kind(id)!=r.action_kind)fail("MilitaryActionTimeOriginalOwnerMismatch",id);return take(std::visit([&](const auto& value){return value.read_action_time_owner(r,id);},original));});}
Outcome<WorldMoment> NativeMilitaryActionTimeRead::read_action_time_source(const ElapsedTimeSource& source)const{return guard<WorldMoment>([&]{
 const auto* owned=std::get_if<MomentOwnerSource>(&source);if(!owned||owned->source_schema!=2||owned->source_ref.domain!="military")fail("MilitaryActionTimeOriginalMomentSourceRequired");const auto& ref=owned->source_ref;
 if(ref.kind=="military_office_completion_applied"){
  auto original=reader("office_accept");return take(std::visit([&](const auto& value){return value.read_action_time_source(source);},original));
 }
 const auto kind=original_action_kind(ref.id);const bool training=kind=="military_train",logistics=kind=="army_return"||kind=="army_resupply";
 if(training?(ref.kind!="military_training"&&ref.kind!="military_training_completed"):(logistics?(ref.kind!="military_action"&&ref.kind!="military_logistics_completed"):(ref.kind!="military_action"&&ref.kind!="military_action_completed")))fail("MilitaryActionTimeOriginalMomentKindMismatch",ref.id);
 auto original=reader(kind);return take(std::visit([&](const auto& value){return value.read_action_time_source(source);},original));
});}
Status NativeMilitaryActionTimeRead::validate_fixed_action_sources(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const ActionElapsedState& e,const std::vector<FixedServiceTransition>& t)const{return status([&]{auto original=registered_reader(r);if(original_action_kind(e.action_ref)!=r.action_kind)fail("MilitaryActionTimeOriginalOwnerMismatch",e.action_ref);take(std::visit([&](const auto& value){return value.validate_fixed_action_sources(r,p,e,t);},original));});}
Status NativeMilitaryActionTimeRead::validate_shared_action_sources(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const SharedWorkElapsedState& e)const{return status([&]{auto original=registered_reader(r);take(std::visit([&](const auto& value){return value.validate_shared_action_sources(r,p,e);},original));});}
Outcome<RunStop> NativeMilitaryActionTimeRead::read_action_continuation_stop(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& at)const{return guard<RunStop>([&]{auto original=registered_reader(r);if(original_action_kind(p.action_ref)!=r.action_kind)fail("MilitaryActionTimeOriginalOwnerMismatch",p.action_ref);return take(std::visit([&](const auto& value){return value.read_action_continuation_stop(r,p,at);},original));});}
Outcome<OriginalActionRemaining> NativeMilitaryActionTimeRead::read_original_action_remaining(const ActionTimeRegistration& r,const ActionTimeOwnerProof& p,const WorldMoment& at)const{return guard<OriginalActionRemaining>([&]{auto original=registered_reader(r);if(original_action_kind(p.action_ref)!=r.action_kind)fail("MilitaryActionTimeOriginalOwnerMismatch",p.action_ref);return take(std::visit([&](const auto& value){return value.read_original_action_remaining(r,p,at);},original));});}
Outcome<StableId> NativeMilitaryActionTimeRead::read_gate_result_id(const SharedWorkGateResult&)const{return {false,{},common_failure("MilitaryActionTimeDoesNotOwnSharedWork",{},ErrorKind::unsupported_capability).error};}
Outcome<SharedWorkGateResult> NativeMilitaryActionTimeRead::read_original_gate_result(const StableId&)const{return {false,{},common_failure("MilitaryActionTimeDoesNotOwnSharedWork",{},ErrorKind::unsupported_capability).error};}
Status NativeMilitaryActionTimeRead::validate_original_gate_result(const SharedWorkGateResult&)const{return common_failure("MilitaryActionTimeDoesNotOwnSharedWork",{},ErrorKind::unsupported_capability);}
std::vector<ActionTimeRegistration> selected_military_action_time_registrations(const MilitaryActionTimeReadSelection& selected){std::vector<ActionTimeRegistration> out;if(selected.office_accept)out.push_back(exact_office_action_registration());if(selected.army_form)out.push_back(exact_formation_action_registration());if(selected.military_train)out.push_back(exact_military_training_registration());if(selected.army_return)out.push_back(exact_return_action_registration());if(selected.army_resupply)out.push_back(exact_resupply_action_registration());return out;}
}
