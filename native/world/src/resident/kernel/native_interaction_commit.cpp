#include "native_interaction_commit.hpp"
#include "native_interaction_world.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_native.hpp"
#include "resident/domains/interaction/appointments/personal_appointment_world.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_native.hpp"
#include "resident/domains/interaction/visit_appointments/visit_appointment_world.hpp"
namespace hanmo::resident {namespace {
// 原 phase 最晚销毁；总 owner 先持真实 body/people/control，局部 S3 只借用。
struct ActualCommitRead {
 std::unique_ptr<PhaseCurrentReadScope> phase;
 std::unique_ptr<CompleteExactWorldBindingOwner> owner;
};
Outcome<ActualCommitRead> read(const WorldBundle&w){
 auto at=w.actual_phase_moment();if(!at.ok)return {false,{},std::move(at.error)};ActualCommitRead out;
 if(!w.has_active_read_phase()){auto phase=w.enter_phase_read_context("interaction_commit_binding","interaction",{at.value,at.value},at.value);if(!phase.ok)return {false,{},std::move(phase.error)};out.phase=std::move(phase.value);}
 // complete_exact_time_bindings 构造真实 borrower，不能先对 pending event 作全 P10 capture。
 auto owner=w.complete_exact_time_bindings();if(!owner.ok)return {false,{},std::move(owner.error)};
 if(!owner.value)return {false,{},common_failure("InteractionCompleteOwningLeaseRequired",{},ErrorKind::unsupported_capability).error};out.owner=std::move(owner.value);return {true,std::move(out),{}};
}
}
Status bind_native_personal_appointment_commit_in_candidate(WorldBundle&w,const std::vector<StableId>&sources,const StableId&actor,Integer sequence,const CommitResult&result){
 if(!w.catalog->personal_appointments||w.catalog->personal_appointments->version!=2)return bind_personal_appointment_result_world(w,sources,actor,sequence,result);
 auto owning=read(w);if(!owning.ok)return {false,{},std::move(owning.error)};
 auto group=read_native_interaction_world(w,owning.value.owner->reads());if(!group.ok)return {false,{},std::move(group.error)};
 if(!group.value->personal())return common_failure("PersonalAppointmentActualOwningGroupRequired",{},ErrorKind::unsupported_capability);
 auto actual=group.value->personal()->context();if(!actual.ok)return {false,{},std::move(actual.error)};
 // 原 binder 自己反验已 seal result/history，任何失败由外层撤整候选。
 auto bound=bind_personal_appointment_result_world(w,sources,actor,sequence,result,actual.value);w.note_write_boundary();return bound;
}
Status bind_native_visit_appointment_commit_in_candidate(WorldBundle&w,const StableId&invitation,const std::vector<StableId>&meetings,const StableId&actor,Integer sequence,const CommitResult&result){
 if(!w.catalog->visit_appointment_invitations||w.catalog->visit_appointment_invitations->version!=2)return bind_visit_appointment_result_world(w,invitation,meetings,actor,sequence,result);
 auto owning=read(w);if(!owning.ok)return {false,{},std::move(owning.error)};const auto&original=owning.value.owner->reads();
 auto group=read_native_interaction_world(w,original);if(!group.ok)return {false,{},std::move(group.error)};
 if(!group.value->visit())return common_failure("VisitAppointmentActualOwningGroupRequired",{},ErrorKind::unsupported_capability);
 auto actual=group.value->visit()->context();if(!actual.ok)return {false,{},std::move(actual.error)};
 auto bound=bind_visit_appointment_result_world(w,invitation,meetings,actor,sequence,result,actual.value);w.note_write_boundary();return bound;
}
}
