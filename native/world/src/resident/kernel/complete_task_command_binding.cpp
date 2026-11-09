#include "complete_task_command_binding.hpp"
#include "common_command_sources.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/tasks/task_claim_establishment.hpp"
#include "resident/domains/tasks/task_quantity_time2.hpp"
namespace hanmo::resident {
Status bind_complete_task_command_result(WorldBundle& world,const CommitResult& result){
    if(!world.catalog||!world.catalog->exact_time||!exact_complete_selected(*world.catalog->exact_time))
        return common_success();
    if(!world.tasks.exact_quantities)return common_success();
    if(!world.active_exact_write_context()||!world.common.actor_condition_capture_identity())
        return common_failure("CompleteTaskActualCaptureRequired",result.result_id,ErrorKind::internal);
    const SourceRef source{"common","command_result",result.result_id};
    auto actual=read_common_command_source(world.common,source);
    if(!actual.ok)return {false,{},std::move(actual.error)};
    if(actual.value.result!=&result||!result.details)
        return common_failure("CompleteTaskActualSealedResultRequired",result.result_id,ErrorKind::internal);
    if(const auto* started=std::get_if<ActionStarted>(&result.details->data);started&&result.kind=="accept_offer"){
        const auto* action=world.common.action(started->action_id);
        const auto* parameters=world.tasks.actions.find(started->action_id);
        const auto* task=parameters?world.tasks.tasks.find(parameters->task_id):nullptr;
        if(!action||action->kind!="accept_offer"||action->actor_id!=actual.value.history->actor_id||!task)
            return common_failure("CompleteTaskActualAcceptanceRequired",started->action_id,ErrorKind::internal);
        if(task->record_kind==TaskRecordKind::exact_quantity){
            auto bound=bind_task_claim_established_result_in_candidate(world.common,world.tasks,
                world.affairs,started->action_id,source);
            if(!bound.ok)return bound;
            world.note_write_boundary();
        }
    }
    if(std::holds_alternative<AdvanceDetails>(result.details->data)){
        auto bound=bind_exact_quantity_advance_result_in_candidate(world.common,world.tasks,source);
        if(!bound.ok)return bound;
        world.note_write_boundary();
    }
    return common_success();
}
}
