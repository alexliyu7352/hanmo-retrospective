#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "native_work_lifecycle_command_route.hpp"
#include "world_bundle.hpp"
#include "world_read_stamp.hpp"
#include "native_complete_time_catalog.hpp"
#include "native_task_work_catalog.hpp"
#include "native_governance_programs.hpp"
#include "resident/domains/tasks/work_execution/work_reassignment_sources.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
#include "resident/domains/organization/organization_lifecycle_codec_detail.hpp"
namespace hanmo::resident {
namespace {
Status selected(const WorldBundle& world){
 if(!world.catalog)return common_failure("WorkLifecycleCatalogRequired",{},ErrorKind::internal);
 auto valid=validate_native_task_work_layout(*world.catalog,world.tasks);if(!valid.ok)return valid;
 valid=validate_native_governance_program_layout(world);if(!valid.ok)return valid;
 if(!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time)||
    !world.tasks.work_execution||world.tasks.work_execution->version!=6||
    !world.governance_programs||world.governance_programs->schema_version!=3||
    !world.catalog->governance_program_catalog||world.catalog->governance_program_catalog->schema_version!=4||
    !world.organization_runtime)
  return common_failure("WorkLifecycleCommandProfileRequired",{},ErrorKind::unsupported_capability);
 return common_success();
}
Status request_binding(const CommandPayload& command,const StableId& actor){
 if(!native_work_lifecycle_command_kind(command.kind)||
   (command.kind=="governance_work_reassignment_approve")!=std::holds_alternative<ApproveGovernanceWorkReassignment>(command.data)||
   (command.kind=="work_reassignment_accept")!=std::holds_alternative<AcceptWorkReassignment>(command.data))
  return common_failure("InvalidWorkLifecycleCommand");
 if(const auto* request=std::get_if<ApproveGovernanceWorkReassignment>(&command.data);request&&request->authority.actor_id!=actor)
  return common_failure("WorkReassignmentAuthenticatedIssuerRequired",actor);
 return common_success();
}
Status actual_sources(const WorldBundle& world,const SupervisionSourceContext& execution){
 const auto* x=execution.reassignments;
 if(!x||!world.catalog||!world.governance_programs||!world.organization_runtime||
   !world.catalog->organization||!world.catalog->governance_program_catalog||
   &execution.common!=&world.common||&execution.tasks!=&world.tasks||
   &x->tasks.common!=&world.common||&x->tasks.tasks!=&world.tasks||
   &x->governance.history.tasks!=&world.tasks||
   &x->tasks.governance!=&x->governance.history.programs||
   &x->tasks.governance.common!=&world.common||
   &x->tasks.governance.organization!=&world.organization||
   &x->tasks.governance.organization_runtime!=&*world.organization_runtime||
   &x->tasks.governance.organization_catalog!=world.catalog->organization.get()||
   &x->tasks.governance.state!=&*world.governance_programs||
   &x->tasks.governance.catalog!=world.catalog->governance_program_catalog.get())
  return common_failure("WorkLifecycleCommandSourceRootMismatch",{},ErrorKind::internal);
 auto actual=world.actual_phase_moment();if(!actual.ok)return {false,{},std::move(actual.error)};
 auto current=execution.clock.read_current_moment();if(!current.ok)return {false,{},std::move(current.error)};
 auto same=compare_world_moments(actual.value,current.value);if(!same.ok)return {false,{},std::move(same.error)};
 if(same.value)return common_failure("WorkLifecycleCommandMomentMismatch",{},ErrorKind::internal);
 return validate_governance_work_lifecycle_bindings(x->governance,x->original_sources_stamp);
}
Status preflight(const WorkReassignmentSourceContext& x,const StableId& actor,const CommandPayload& command){
 if(const auto* request=std::get_if<ApproveGovernanceWorkReassignment>(&command.data))
  return validate_governance_work_reassignment_request(x.governance,*request,x.original_sources_stamp);
 return validate_work_reassignment_acceptance_request(x,std::get<AcceptWorkReassignment>(command.data).approval_ref,actor);
}
struct ReadAccess {
 // The read phase remains valid while the owner-backed inner lease is destroyed.
 std::unique_ptr<PhaseCurrentReadScope> phase;
 std::shared_ptr<const TaskWorkReadLease> lease;
};
Outcome<ReadAccess> read_access(const WorldBundle& world){
 ReadAccess out;
 auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 if(!world.has_active_read_phase()){
  auto phase=world.enter_phase_read_context("work_lifecycle_command","task_work_execution",{now.value,now.value},now.value);
  if(!phase.ok)return {false,{},std::move(phase.error)};out.phase=std::move(phase.value);
 }
 auto factory=read_native_complete_task_work_factory(*world.catalog);if(!factory.ok)return {false,{},std::move(factory.error)};
 if(!factory.value.read)return {false,{},common_failure("TaskWorkOriginalWorldFactoryRequired",{},ErrorKind::unsupported_capability).error};
 auto lease=factory.value.read(world,now.value);if(!lease.ok)return {false,{},std::move(lease.error)};
 if(!lease.value)return {false,{},common_failure("TaskWorkOwningReadLeaseRequired",{},ErrorKind::internal).error};
 out.lease=std::move(lease.value);return {true,std::move(out),{}};
}
}
bool native_work_lifecycle_command_kind(const StableId& kind)noexcept{
 return kind=="governance_work_reassignment_approve"||kind=="work_reassignment_accept";
}
bool native_work_lifecycle_command_payload(const CommandPayload& command)noexcept{
 return native_work_lifecycle_command_kind(command.kind)||std::holds_alternative<ApproveGovernanceWorkReassignment>(command.data)||std::holds_alternative<AcceptWorkReassignment>(command.data);
}
Outcome<CommandPayload> decode_native_work_lifecycle_command(const StableId& kind,const BoundaryValue& input){
 if(kind=="governance_work_reassignment_approve"){
  ActorFoundationJsonVisitor visitor(input);auto valid=visitor.prepare_default_values<ApproveGovernanceWorkReassignment>();if(!valid.ok)return {false,{},std::move(valid.error)};
  ApproveGovernanceWorkReassignment request;
  visitor.object("authority",[&](auto& v){return dynamic_codec::authority(v,request.authority);});
  visitor.field("work_id",request.work_id);visitor.field("new_actor_id",request.new_actor_id);
  visitor.object("expected_executor_predecessor_ref",[&](auto& v){
   v.object("source_ref",[&](auto& source){return organization_lifecycle_fields::source(source,request.expected_executor_predecessor_ref.source_ref);});
   return v.field("source_schema",request.expected_executor_predecessor_ref.source_schema);
  });
  visitor.field("expected_target_revision",request.expected_target_revision);
  valid=visitor.finish();if(!valid.ok)return {false,{},std::move(valid.error)};
  return {true,{kind,std::move(request)},{}};
 }
 if(kind=="work_reassignment_accept"){
  ActorFoundationJsonVisitor visitor(input);auto valid=visitor.prepare_default_values<AcceptWorkReassignment>();if(!valid.ok)return {false,{},std::move(valid.error)};
  AcceptWorkReassignment request;
  visitor.object("approval_ref",[&](auto& v){return organization_lifecycle_fields::source(v,request.approval_ref);});
  valid=visitor.finish();if(!valid.ok)return {false,{},std::move(valid.error)};
  return {true,{kind,std::move(request)},{}};
 }
 return {false,{},common_failure("UnsupportedWorkLifecycleCommand",kind,ErrorKind::unsupported_capability).error};
}
Status preview_native_work_lifecycle_command(const WorldBundle& world,const StableId& actor,const CommandPayload& command){
 auto valid=request_binding(command,actor);if(!valid.ok)return valid;
 valid=selected(world);if(!valid.ok)return valid;
 auto access=read_access(world);if(!access.ok)return {false,{},std::move(access.error)};
 const auto binding=access.value.lease->bindings();valid=actual_sources(world,binding.execution);if(!valid.ok)return valid;
 // Only the original read-only preflight runs; no next_id, source append,
 // Basis binding, acceptance or payment is performed here.
 return preflight(*binding.execution.reassignments,actor,command);
}
Outcome<ReceiptDetails> apply_native_work_lifecycle_command(WorldBundle& world,const StableId& actor,const PreparedDomainOp& operation){
 auto valid=request_binding(operation.command,actor);if(!valid.ok)return {false,{},std::move(valid.error)};
 const auto* quote=std::get_if<DurationQuote>(&operation.quote.data);
 if(operation.quote.kind!=operation.command.kind||!quote||!quote->duration||*quote->duration!=0)
  return {false,{},common_failure("InvalidWorkLifecycleCommandQuote").error};
 valid=command_common_guard(world,actor,operation.command.kind);if(!valid.ok)return {false,{},std::move(valid.error)};
 valid=selected(world);if(!valid.ok)return {false,{},std::move(valid.error)};
 if(!world.active_exact_write_context()||!world.common.actor_condition_capture_identity())
  return {false,{},common_failure("WorkLifecycleActualCaptureRequired",{},ErrorKind::internal).error};
 auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
 auto factory=read_native_complete_task_work_factory(*world.catalog);if(!factory.ok)return {false,{},std::move(factory.error)};
 if(!factory.value.write)return {false,{},common_failure("TaskWorkOriginalWorldFactoryRequired",{},ErrorKind::unsupported_capability).error};
 auto lease=factory.value.write(world,now.value);if(!lease.ok)return {false,{},std::move(lease.error)};
 if(!lease.value)return {false,{},common_failure("TaskWorkOwningWriteLeaseRequired",{},ErrorKind::internal).error};
 const auto binding=lease.value->bindings();valid=actual_sources(world,binding.execution);if(!valid.ok)return {false,{},std::move(valid.error)};
 if(binding.lifecycle_programs!=&*world.governance_programs)
  return {false,{},common_failure("WorkLifecycleWritableRootMismatch",{},ErrorKind::internal).error};
 const auto& sources=*binding.execution.reassignments;
 // Each original writer re-runs the same read-only core once in this candidate.
 if(const auto* request=std::get_if<ApproveGovernanceWorkReassignment>(&operation.command.data)){
  auto produced=approve_governance_work_reassignment_in_candidate(world.common,*world.organization_runtime,*binding.lifecycle_programs,sources.governance,*request,sources.original_sources_stamp);
  if(!produced.ok)return {false,{},std::move(produced.error)};
 }else{
  auto produced=accept_work_reassignment_in_candidate(world.common,world.tasks,sources,std::get<AcceptWorkReassignment>(operation.command.data).approval_ref,actor);
  if(!produced.ok)return {false,{},std::move(produced.error)};
 }
 // Actual sources were written above; the ordinary outer transaction emits its
 // one Common command result. Done grants no H, payment or replacement service.
 return {true,ReceiptDetails{Done{}},{}};
}
Status validate_native_work_lifecycle_command_history(const WorldBundle& world){
 for(const auto& row:world.common.state().results.rows()){
  if(!row.second||!native_work_lifecycle_command_kind(row.second->kind))continue;
  const auto& result=*row.second;
  auto valid=selected(world);if(!valid.ok)return valid;
  if(!result.details||!std::holds_alternative<Done>(result.details->data))
   return common_failure("WorkLifecycleCommandReceiptMismatch",row.first);
 }
 return common_success();
}
}
