// 确认仅执行当前已保存作者请求；外层版本与道路门保留，内部没有递归commit。
#include "interaction_integration.hpp"
#include "complete_command_capture_dispatch.hpp"
#include "resident/domains/interaction/interaction_actions.hpp"
#include "resident/domains/interaction/regional_life_dialogue.hpp"
#include "resident/domains/interaction/interaction_action_confirmation.hpp"
#include "exact_phase_graph.hpp"
namespace hanmo::resident {
namespace {
// 共享需求直接复用冻结typed任务；旧作者命令才经过其原有限编码，不从新的机会改绑。
Outcome<CommandPayload> current_dialogue_command(const WorldBundle& world,const StableId& actor){
    if(world.regional_life_enabled()){auto access=world.services.interaction_access();auto task=pending_regional_task(world.common,world.interaction,*world.catalog->interaction,access,actor);if(!task.ok)return {false,{},task.error};if(task.value)return {true,{task.value->kind,*task.value},{}};}
    auto request=pending_dialogue_request(world.interaction,actor);if(!request.ok)return {false,{},request.error};
    if(request.value.kind=="dialogue_confirm")return {false,{},common_failure("DialogueCommandMismatch").error};
    try{return decode_resident_command(request.value.kind,interaction_codec::encode_command(request.value));}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}
}
}

// 先走原公共守卫，再从会话取请求；报价kind保内部领域，外部回执身份仍是dialogue_confirm。
Outcome<std::shared_ptr<const PreviewResult>> prepare_dialogue_confirmation(const WorldBundle& world,const StableId& actor,const CommandPayload& outer) {
    if(outer.kind!="dialogue_confirm")return {false,{},common_failure("UnsupportedAction").error};
    auto guard=command_common_guard(world,actor,outer.kind);
    if(!guard.ok)return {false,{},guard.error};
    auto request=current_dialogue_command(world,actor);
    if(!request.ok)return {false,{},request.error};
    // 原作者pending合同不允许确认再嵌确认，畸形typed调用不能制造无界递归。
    if(request.value.kind=="dialogue_confirm")return {false,{},common_failure("DialogueCommandMismatch").error};
    try {
        auto prepared=prepare_command(world,actor,request.value);
        if(!prepared.ok)return prepared;
        if(!prepared.value||!prepared.value->operation||!prepared.value->quote)
            return {false,{},common_failure("MissingPreparedDialogueAction",{},ErrorKind::internal).error};
        auto operation=std::make_shared<PreparedDomainOp>();
        operation->command=outer;
        operation->quote=*prepared.value->quote;
        operation->nested=prepared.value->operation;
        auto result=std::make_shared<PreviewResult>();
        result->source=world.common.state().identity;
        result->actor_id=actor;
        result->kind=outer.kind;
        result->operation=operation;
        result->quote=prepared.value->quote;
        return {true,std::move(result),{}};
    } catch(const boundary::DecodeFailure& error) {
        return {false,{},error.error};
    }
}
// 同候选执行一次原动作，实际ActionStarted保等待；即时完成按旧分支返回对白继续的空明细。
Outcome<ReceiptDetails> apply_dialogue_confirmation(WorldBundle& world,const StableId& actor,const PreparedDomainOp& operation,CompleteExactCommandCapture* capture) {
    if(operation.command.kind!="dialogue_confirm"||!operation.nested)
        return {false,{},common_failure("MissingPreparedDialogueAction",{},ErrorKind::internal).error};
    auto request=current_dialogue_command(world,actor);
    if(!request.ok)return {false,{},request.error};
    if(request.value.kind=="dialogue_confirm"||request.value.kind!=operation.nested->command.kind)
        return {false,{},common_failure("DialogueCommandMismatch").error};
    if(const auto* task=std::get_if<TaskCommand>(&request.value.data)){const auto* frozen=std::get_if<TaskCommand>(&operation.nested->command.data);if(!frozen||!compare_boundary(encode_task_command(*task),encode_task_command(*frozen)).exact())return {false,{},common_failure("DialogueCommandMismatch").error};}
    InteractionActionConfirmationPending* confirmation=nullptr;
    if(interaction_action_confirmation_schema_version(world.catalog->interaction->response_sources_version)){
        auto* writes=world.active_exact_write_context();
        if(!writes||!writes->pending_interaction_confirmation||
           !writes->pending_interaction_confirmation->bound_to(world.common,world.interaction,*world.catalog->interaction))
            return {false,{},common_failure("InteractionConfirmationActualCaptureRequired",{},ErrorKind::internal).error};
        confirmation=writes->pending_interaction_confirmation;
        auto now=world.actual_phase_moment();if(!now.ok)return {false,{},std::move(now.error)};
        auto captured=prepare_interaction_action_confirmation_in_candidate(world.common,world.interaction,
            *world.catalog->interaction,actor,now.value,*confirmation);
        if(!captured.ok)return {false,{},std::move(captured.error)};
    }
    auto applied=capture?apply_command_with_complete_capture(world,actor,*operation.nested,*capture):apply_command(world,actor,*operation.nested);
    if(!applied.ok)return applied;
    if(confirmation){
        auto produced=record_interaction_action_confirmation_produced_in_candidate(world.common,world.interaction,
            *world.catalog->interaction,*confirmation,applied.value);
        if(!produced.ok)return {false,{},std::move(produced.error)};
    }
    // 动作可能按原规则清除会话；此时原confirm直接返回已发生的动作结果。
    bool present=false;
    for(const auto& session:world.interaction.interactions)if(session.actor_id==actor)present=true;
    if(!present)return applied;
    std::optional<StableId> action_id;
    if(const auto* action=std::get_if<ActionStarted>(&applied.value.data))action_id=action->action_id;
    auto access=world.services.interaction_access();
    auto continued=finish_dialogue_confirmation(world.common,world.interaction,*world.catalog->interaction,access,actor,request.value.kind,action_id);
    if(!continued.ok)return {false,{},continued.error};
    if(request.value.kind=="open_trade"||action_id)return applied;
    ReceiptDetails details;details.data=Done{};
    return {true,std::move(details),{}};
}
}
