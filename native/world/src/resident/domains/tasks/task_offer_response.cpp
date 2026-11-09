// 婉拒必须是当面对原委托的显式选择；只改本人响应，不触发资金、奖惩、交付或世界时间。
#include "task_offer_response.hpp"
#include "task_model.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 原首次获知记录是唯一来源，不能用作者全目录代替本人知识。
const KnowledgeRecord* known_offer(const InteractionState& state,const StableId& actor,const StableId& offer){for(const auto& person:state.knowledge)if(person.actor_id==actor)for(const auto& known:person.offers)if(known.offer_id==offer)return &known;return nullptr;}
// 原作者声明约束引用；响应不会扩张某位说话者分享信息的资格。
bool authored_source(const InteractionCatalog& catalog,const StableId& offer,const KnowledgeSource& source){for(const auto& definition:catalog.discoveries)if(definition.offer_id==offer)for(const auto& allowed:definition.sources)if(allowed.kind==source.kind&&allowed.id==source.id)return true;return false;}
// 普通岗位上下文拒绝道路/叙事标签及畸形输入，不把记录步骤当作远程服务。
bool ordinary_context(const InteractionContext& context){return !context.kind&&!context.encounter_id&&!context.input_error&&!context.place_id.empty()&&!context.provider_actor_id.empty()&&!context.role_id.empty();}
}
// 读取只返回本人对应机会的响应，不扫描或推断其他人的意愿。
const TaskOfferResponse* task_offer_response(const TaskOfferResponseState& state,const StableId& actor,const StableId& offer){for(const auto& row:state.responses)if(row.actor_id==actor&&row.offer_id==offer)return &row;return nullptr;}
// 婉拒无需资金充足或目标可完成，但必须具备真实当面接洽资格且从未成立这份承诺。
Outcome<TaskQuote> preview_task_offer_decline(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& actor,const TaskCommand& command){
    if(command.kind!="decline_offer")return {false,{},common_failure("UnsupportedAction").error};
    if(command.offer_ref&&command.offer_ref->kind!=OfferRef::Kind::authored)return {false,{},common_failure("RuntimeResponseSourceUnavailable",{},ErrorKind::unsupported_capability).error};
    if(command.quantity||command.need_revision)return {false,{},common_failure("InvalidTaskOfferResponse").error};
    if(command.offer_ref&&!command.offer_id.empty()&&command.offer_id!=command.offer_ref->id)return {false,{},common_failure("OfferReferenceMismatch").error};
    const auto ref=task_command_offer(command);
    // 本人尚未知晓时不查询作者定义，让真实隐藏机会与不存在的ID得到同一来源拒绝。
    const auto* known=known_offer(interaction,actor,ref.id);if(!known||!authored_source(interactions,ref.id,known->source))return {false,{},common_failure("SourceUnknown").error};
    const auto* definition=catalog.offers.find(ref.id);if(!definition)return {false,{},common_failure("UnknownOffer").error};
    if(known->discovered_tick<0||known->discovered_tick>common.tick())return {false,{},common_failure("InvalidTaskOfferResponseSource").error};
    const auto* task=task_for(tasks,actor,ref.id);if(task&&!task_can_retry(common,tasks,*task))return {false,{},common_failure("OfferAlreadyAccepted").error};
    const InteractionSession* meeting=nullptr;for(const auto& row:interaction.interactions)if(row.actor_id==actor){meeting=&row;break;}
    // 作者显式婉拒可经原action确认；不能借接受、交易或另一offer的待办绕过当面来源。
    const bool explicit_confirmation=meeting&&meeting->phase=="action"&&meeting->pending_command&&meeting->pending_command->kind=="decline_offer"&&meeting->pending_command->offer_id==ref.id;
    if(!meeting||(meeting->phase!="say"&&meeting->phase!="choice"&&!explicit_confirmation))return {false,{},common_failure("NoConversation").error};
    auto current=validate_interaction(common,interaction,interactions,access,actor,meeting->context);if(!current.ok)return {false,{},current.error};
    if(!ordinary_context(command.context)||command.context.place_id!=meeting->context.place_id||command.context.provider_actor_id!=meeting->context.provider_actor_id)return {false,{},common_failure("StaleTaskConversation").error};
    auto contact=validate_interaction(common,interaction,interactions,access,actor,command.context,"accept_offer");if(!contact.ok)return {false,{},contact.error};
    if(contact.value.role_id!=definition->accept_role)return {false,{},common_failure("RoleNotAuthorized").error};
    if(definition->issuer.kind==Owner::Kind::actor&&contact.value.provider_actor_id!=definition->issuer.id)return {false,{},common_failure("PrivatePromiseNotDelegated").error};
    TaskQuote quote;quote.context=contact.value;quote.duration=0;return {true,std::move(quote),{}};
}
// 同一候选再次核资格后写首次响应；重复婉拒不重写来源、日期或分配世界编号。
Status record_task_offer_decline(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const InteractionAccess& access,const StableId& actor,const TaskCommand& command,TaskOfferResponseState& state){
    if(state.version!=1&&state.version!=2)return common_failure("UnsupportedTaskOfferResponseVersion");
    if((state.version==2)!=state.runtime.has_value())return common_failure("TaskOfferResponseVersionMismatch");
    auto quote=preview_task_offer_decline(common,tasks,catalog,interaction,interactions,access,actor,command);if(!quote.ok)return {false,{},quote.error};
    const auto offer=task_command_offer(command).id;if(task_offer_response(state,actor,offer))return common_success();
    const auto* known=known_offer(interaction,actor,offer);if(!known)return common_failure("SourceUnknown");
    TaskOfferResponse response;response.actor_id=actor;response.offer_id=offer;response.response_tick=common.tick();response.discovered_tick=known->discovered_tick;response.context=quote.value.context;response.source=known->source;state.responses.push_back(std::move(response));return common_success();
}
// 历史响应允许人物后来离岗或去世，但不得伪造来源、未来日期或同人同机会的重复记录。
Status validate_task_offer_responses(const CommonStore& common,const TaskCatalog& catalog,const InteractionState& interaction,const InteractionCatalog& interactions,const TaskOfferResponseState& state){
    if(state.version!=1)return common_failure("UnsupportedTaskOfferResponseVersion");
    if(state.runtime)return common_failure("RuntimeOfferResponseRequiresV2");
    if(!interactions.common)return common_failure("TaskConversationUnavailable");
    std::set<std::pair<StableId,StableId>> seen;
    for(const auto& row:state.responses){
        const auto* offer=catalog.offers.find(row.offer_id);const auto* known=known_offer(interaction,row.actor_id,row.offer_id);const auto* role=interactions.common->roles.find(row.context.role_id);
        if(!common.actor(row.actor_id)||!common.actor(row.context.provider_actor_id)||row.actor_id==row.context.provider_actor_id||!offer||!known||!role||!seen.emplace(row.actor_id,row.offer_id).second)return common_failure("InvalidTaskOfferResponse");
        if(row.discovered_tick!=known->discovered_tick||row.source.kind!=known->source.kind||row.source.id!=known->source.id||!authored_source(interactions,row.offer_id,row.source)||row.discovered_tick<0||row.response_tick<row.discovered_tick||row.response_tick>common.tick())return common_failure("InvalidTaskOfferResponseSource");
        if(!ordinary_context(row.context)||role->place_id!=row.context.place_id||row.context.role_id!=offer->accept_role||(offer->issuer.kind==Owner::Kind::actor&&row.context.provider_actor_id!=offer->issuer.id))return common_failure("InvalidTaskOfferResponseContext");
    }
    return common_success();
}
Status validate_task_offer_responses_with_runtime(const CommonStore& common,const TaskState& tasks,const TaskCatalog& catalog,const AffairsState& affairs,const SocialState& social,const InteractionState& interaction,const InteractionCatalog& interactions,const TaskOfferResponseState& state){
    if(state.version==1)return validate_task_offer_responses(common,catalog,interaction,interactions,state);
    if(state.version!=2)return common_failure("UnsupportedTaskOfferResponseVersion");
    if(!state.runtime)return common_failure("MissingRuntimeOfferResponses");
    TaskOfferResponseState authored;authored.responses=state.responses;
    auto valid=validate_task_offer_responses(common,catalog,interaction,interactions,authored);if(!valid.ok)return valid;
    return validate_runtime_task_offer_responses(common,tasks,affairs,social,interactions,*state.runtime);
}
}
