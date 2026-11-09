#include "native_control_command_route.hpp"
#include "native_control_continuation.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/family/control_retirement_cancel.hpp"
namespace hanmo::resident {
namespace {
struct OriginalControlRead {
    // The phase and owning graph outlive every borrowed domain source.
    std::unique_ptr<PhaseCurrentReadScope> phase;
    std::unique_ptr<CompleteExactWorldBindingOwner> owner;
    const NativeContinuationOriginalSourceRead* source=nullptr;
};
Status selected(const WorldBundle& world){
    auto valid=validate_native_control_continuation_layout(world);if(!valid.ok)return valid;
    if(!world.control_continuation||world.catalog->control_continuation->selected_version!=3)
        return common_failure("ControlContinuation3Required",{},ErrorKind::unsupported_capability);
    return common_success();
}
Status request_kind(const CommandPayload& command){
    if((command.kind=="cancel_pending_retirement")!=std::holds_alternative<CancelPendingRetirement>(command.data)||
       (command.kind=="cancel_continuation_boundary")!=std::holds_alternative<CancelContinuationBoundary>(command.data)||
       !native_control_cancellation_command_kind(command.kind))
        return common_failure("InvalidContinuationCancellationCommand");
    return common_success();
}
Outcome<OriginalControlRead> original(const WorldBundle& world,const CompleteExactWorldReadBindings* actual){
    auto valid=selected(world);if(!valid.ok)return {false,{},std::move(valid.error)};
    OriginalControlRead read;
    if(!actual){
        auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
        if(!world.has_active_read_phase()){
            auto phase=world.enter_phase_read_context("control_command","control_continuation",
                {now.value,now.value},now.value);
            if(!phase.ok)return {false,{},std::move(phase.error)};
            read.phase=std::move(phase.value);
        }
        auto owner=world.complete_exact_time_bindings();if(!owner.ok)return {false,{},std::move(owner.error)};
        read.owner=std::move(owner.value);
        if(!read.owner)return {false,{},common_failure("ControlOriginalOwningLeaseRequired",{},ErrorKind::internal).error};
        actual=&read.owner->reads();
    }
    if(!world.catalog->complete_exact_time_runtime||
       &actual->runtime!=world.catalog->complete_exact_time_runtime.get()||
       !actual->continuation_sources||
       !actual->continuation_sources->bound_to(world.common,world.family,*world.control_continuation))
        return {false,{},common_failure("ControlContinuationSameOriginalOwnerRequired",{},ErrorKind::internal).error};
    read.source=actual->continuation_sources;
    return {true,std::move(read),{}};
}
}
bool native_control_cancellation_command_kind(const StableId& kind){
    return kind=="cancel_pending_retirement"||kind=="cancel_continuation_boundary";
}
Status guard_native_control_cancellation(const WorldBundle& world,const StableId& actor){
    auto valid=selected(world);if(!valid.ok)return valid;
    if(!world.common.actor(actor))return common_failure("UnknownActor");
    if(actor!=world.common.state().metadata.player_actor_id||
       actor!=world.control_continuation->control_binding.actor_id)
        return common_failure("ContinuationPersonalCancellationRequired");
    // CancelBoundary is also available to the original after-death controller;
    // CancelRetirement's original gate separately requires a living predecessor.
    return common_success();
}
Status preview_native_control_cancellation(const WorldBundle& world,const StableId& actor,
    const CommandPayload& command,const CompleteExactWorldReadBindings* actual){
    auto valid=request_kind(command);if(!valid.ok)return valid;
    valid=guard_native_control_cancellation(world,actor);if(!valid.ok)return valid;
    auto read=original(world,actual);if(!read.ok)return {false,{},std::move(read.error)};
    if(const auto* request=std::get_if<CancelPendingRetirement>(&command.data)){
        auto prepared=read_cancel_pending_retirement(world.common,world.family,*world.control_continuation,
            *world.catalog->control_continuation,world.catalog->identity,*read.value.source,actor,*request);
        return prepared.ok?common_success():Status{false,{},std::move(prepared.error)};
    }
    auto prepared=read_cancel_continuation_boundary(world.common,*world.control_continuation,
        *world.catalog->control_continuation,world.catalog->identity,*read.value.source,actor,
        std::get<CancelContinuationBoundary>(command.data));
    return prepared.ok?common_success():Status{false,{},std::move(prepared.error)};
}
Outcome<ReceiptDetails> apply_native_control_cancellation(WorldBundle& world,const StableId& actor,
    const PreparedDomainOp& operation,const CompleteExactWorldReadBindings* actual){
    auto valid=request_kind(operation.command);if(!valid.ok)return {false,{},std::move(valid.error)};
    const auto* duration=std::get_if<DurationQuote>(&operation.quote.data);
    if(operation.quote.kind!=operation.command.kind||!duration||!duration->duration||*duration->duration!=0)
        return {false,{},common_failure("InvalidContinuationCancellationQuote").error};
    valid=guard_native_control_cancellation(world,actor);if(!valid.ok)return {false,{},std::move(valid.error)};
    if(!world.active_exact_write_context()||!world.common.actor_condition_capture_identity())
        return {false,{},common_failure("ControlCancellationActualCaptureRequired",{},ErrorKind::internal).error};
    auto read=original(world,actual);if(!read.ok)return {false,{},std::move(read.error)};
    if(const auto* request=std::get_if<CancelPendingRetirement>(&operation.command.data)){
        auto produced=cancel_pending_retirement_in_candidate(world.common,world.family,*world.control_continuation,
            *world.catalog->control_continuation,world.catalog->identity,*read.value.source,actor,*request);
        if(!produced.ok)return {false,{},std::move(produced.error)};
        // The true outgoing token remains alive throughout this separately
        // precharged immutable-receipt payload copy.
        return {true,ReceiptDetails(produced.value.receipt()),{}};
    }
    auto produced=cancel_continuation_boundary_in_candidate(world.common,*world.control_continuation,
        *world.catalog->control_continuation,world.catalog->identity,*read.value.source,actor,
        std::get<CancelContinuationBoundary>(operation.command.data));
    if(!produced.ok)return {false,{},std::move(produced.error)};
    return {true,ReceiptDetails(produced.value.receipt()),{}};
}
Status bind_native_control_command_result(WorldBundle& world,const CommitResult& result){
    const bool cancellation=native_control_cancellation_command_kind(result.kind);
    const bool stabilization=result.kind=="stabilize_continuation_boundary";
    if(!cancellation&&!stabilization)return common_success();
    auto read=original(world,nullptr);if(!read.ok)return {false,{},std::move(read.error)};
    const SourceRef source{"common","command_result",result.result_id};
    auto actual=read_common_command_source(world.common,source);if(!actual.ok)return {false,{},std::move(actual.error)};
    if(actual.value.result!=&result||!result.details)
        return common_failure("ControlActualSealedResultRequired",result.result_id,ErrorKind::internal);
    if(result.kind=="cancel_pending_retirement"){
        const auto* receipt=std::get_if<PendingRetirementCancelled>(&result.details->data);
        if(!receipt)return common_failure("RetirementCancellationReceiptKindMismatch",result.result_id);
        auto original=read_original_pending_retirement_withdrawal(world.common,world.family,
            *world.control_continuation,receipt->pending_id);
        if(!original.ok)return {false,{},std::move(original.error)};
        if(original.value.result!=&result)
            return common_failure("ContinuationWithdrawalCommandMismatch",result.result_id);
    }else if(result.kind=="cancel_continuation_boundary"){
        auto original=read_actual_continuation_boundary_cancellation(world.common,*world.control_continuation,source);
        if(!original.ok)return {false,{},std::move(original.error)};
    }else{
        auto valid=validate_original_continuation_boundary_receipt(world.common,*world.control_continuation,
            *read.value.source,source);if(!valid.ok)return valid;
    }
    return validate_control_continuation_v3(world.common,*world.control_continuation,
        *world.catalog->control_continuation,world.catalog->identity,*read.value.source);
}
}
