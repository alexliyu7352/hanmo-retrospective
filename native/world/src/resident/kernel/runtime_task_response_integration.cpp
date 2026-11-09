// 当前Knowledge与FaceProof只在调用栈存活；永久来源由任务域记录，外层统一管理事务。
#include "runtime_task_response_integration.hpp"
#include "world_bundle.hpp"
#include "political_takeover_execution_world.hpp"
namespace hanmo::resident {
namespace {
// 版本、存在性和动态基础同时成立，缺域不隐式补空记录。
Status enabled(const WorldBundle& world){
    if(!world.catalog->task_offer_responses||world.catalog->task_offer_response_version!=2||!world.dynamic_enabled())return common_failure("RuntimeOfferResponseRequiresV2");
    if(!world.tasks.offer_responses||world.tasks.offer_responses->version!=2||!world.tasks.offer_responses->runtime)return common_failure("MissingRuntimeOfferResponses");
    return common_success();
}
// 公共TaskQuote保持既有编码，只向页面公开零耗时与真实办理上下文。
TaskQuote public_quote(const RuntimeTaskOfferResponseQuote& source){TaskQuote quote;quote.duration=0;quote.context=source.response.context;return quote;}
}
Outcome<TaskQuote> preview_runtime_task_response(const WorldBundle& world,const StableId& actor,const TaskCommand& command){
    auto gate=enabled(world);if(!gate.ok)return {false,{},gate.error};
    auto access=world.services.interaction_access();auto authority=world.services.organization_access();
    auto known=known_subject_view(world.common,world.social,authority,actor,&world.interaction);
    auto quote=preview_runtime_task_offer_decline(world.common,world.tasks,world.affairs,world.social,world.interaction,*world.catalog->interaction,access,known,actor,command);
    if(!quote.ok)return {false,{},quote.error};return {true,public_quote(quote.value),{}};
}
Status apply_runtime_task_response(WorldBundle& world,const StableId& actor,const TaskCommand& command,const TaskQuote& quoted){
    auto gate=enabled(world);if(!gate.ok)return gate;
    auto access=world.services.interaction_access();auto authority=world.services.organization_access();
    auto known=known_subject_view(world.common,world.social,authority,actor,&world.interaction);
    auto fresh=preview_runtime_task_offer_decline(world.common,world.tasks,world.affairs,world.social,world.interaction,*world.catalog->interaction,access,known,actor,command);
    if(!fresh.ok)return {false,{},fresh.error};
    auto before=render_boundary_json(encode_task_quote(quoted));auto current=render_boundary_json(encode_task_quote(public_quote(fresh.value)));
    if(!before.ok||!current.ok||before.value!=current.value)return common_failure("StaleRuntimeOfferResponseQuote");
    return record_runtime_task_offer_decline(world.common,world.tasks,world.affairs,world.social,world.interaction,*world.catalog->interaction,access,known,actor,command,fresh.value,*world.tasks.offer_responses->runtime);
}
Outcome<BoundaryValue> query_runtime_interaction_tasks(const WorldBundle& world,Integer cursor){
    auto gate=enabled(world);if(!gate.ok)return {false,{},gate.error};
    const auto& actor=world.common.state().metadata.player_actor_id;auto access=world.services.interaction_access();auto authority=world.services.organization_access();
    auto known=known_subject_view(world.common,world.social,authority,actor,&world.interaction);
    TaskConversationPreview preview=[&](const TaskCommand& command)->Outcome<TaskQuote>{
        auto prepared=prepare_command(world,actor,{command.kind,command});if(!prepared.ok)return {false,{},prepared.error};
        const auto* quote=prepared.value&&prepared.value->quote?std::get_if<TaskQuote>(&prepared.value->quote->data):nullptr;
        if(!quote)return {false,{},common_failure("MissingTaskConversationQuote",{},ErrorKind::internal).error};return {true,*quote,{}};
    };
    auto issued=PoliticalTakeoverExecutionWorldAccess::create(world);if(!issued.ok)return {false,{},issued.error};
    return query_runtime_task_offer_conversation(world.common,world.tasks,world.affairs,world.social,world.interaction,*world.catalog->interaction,access,known,*world.tasks.offer_responses->runtime,actor,cursor,preview,issued.value->issued_supply());
}
}
