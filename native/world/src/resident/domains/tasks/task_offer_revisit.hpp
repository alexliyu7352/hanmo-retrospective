// 回访只读取本人实际婉拒和当时在场接收人的记忆来源，不建立第二任务账或NPC意愿。
#pragma once
#include "task_offer_response.hpp"
namespace hanmo::resident {
// 这是过去的明确响应；后来应承、履约或离开不会把它变成永久拒绝状态。
struct TaskOfferRevisit {
    WorldIdentity source_identity;
    StableId actor_id, provider_actor_id, session_id, offer_id;
    Integer response_tick=0;
    InteractionContext response_context;
    KnowledgeSource discovered_source;
};
// 仅当前真实会面原接收人能回想该响应；无记录和另一接收人均返回空。
// stepping只供原对白branch推进读取，不能用此读口确认接受、收费或取得别人的同意。
Outcome<std::optional<TaskOfferRevisit>> read_task_offer_revisit(
    const CommonStore&, const TaskCatalog&, const InteractionState&,
    const InteractionCatalog&, const InteractionAccess&, const TaskOfferResponseState&,
    const StableId& actor_id, const StableId& offer_id);
BoundaryValue encode_task_offer_revisit(const TaskOfferRevisit&);
// 作者只能问“这位当面人是否曾收到本人婉拒”，不能自报响应时间、来源或另一位人物。
struct TaskOfferRevisitCondition {StableId offer_id;bool equals=true;};
Outcome<TaskOfferRevisitCondition> decode_task_offer_revisit_condition(const BoundaryValue&);
BoundaryValue encode_task_offer_revisit_condition(const TaskOfferRevisitCondition&);
// 只用于已建立真实会面的branch/choice，不用于会面前的全局事件资格筛选。
Outcome<bool> evaluate_task_offer_revisit_condition(
    const CommonStore&, const TaskCatalog&, const InteractionState&,
    const InteractionCatalog&, const InteractionAccess&, const TaskOfferResponseState&,
    const StableId& actor_id, const TaskOfferRevisitCondition&);
}
