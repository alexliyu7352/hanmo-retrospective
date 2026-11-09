// 本人对已知未承诺委托的响应记录；不拥有合同、钱物、任务进度或关系数值。
#pragma once
#include "resident/domains/interaction/interaction_model.hpp"
#include "task_runtime_offer_response.hpp"
namespace hanmo::resident {
struct TaskState;struct TaskCatalog;struct TaskCommand;struct TaskQuote;struct BoundaryValue;
// “这次暂不承接”是可再询问的个人事实，不是取消合同或永久封禁机会。
struct TaskOfferResponse {
    StableId actor_id,offer_id;
    Integer response_tick=0,discovered_tick=0;
    InteractionContext context;
    KnowledgeSource source;
};
// 一人一条作者机会最多保留首次明确婉拒；重复打开、退出对白与读取都不建立记录。
struct TaskOfferResponseState { Integer version=1;std::vector<TaskOfferResponse> responses;std::optional<RuntimeTaskOfferResponses> runtime; };
const TaskOfferResponse* task_offer_response(const TaskOfferResponseState&,const StableId& actor,const StableId& offer);
Outcome<TaskQuote> preview_task_offer_decline(const CommonStore&,const TaskState&,const TaskCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const TaskCommand&);
Status record_task_offer_decline(const CommonStore&,const TaskState&,const TaskCatalog&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const StableId&,const TaskCommand&,TaskOfferResponseState&);
Status validate_task_offer_responses(const CommonStore&,const TaskCatalog&,const InteractionState&,
    const InteractionCatalog&,const TaskOfferResponseState&);
// v2完整校验显式借原Task/Need/社会报告；旧入口不能误把动态回应当已验证。
Status validate_task_offer_responses_with_runtime(const CommonStore&,const TaskState&,const TaskCatalog&,
    const AffairsState&,const SocialState&,const InteractionState&,const InteractionCatalog&,
    const TaskOfferResponseState&);
Status visit_task_offer_response_checkpoint(CodecVisitor&,TaskOfferResponseState&);
Outcome<TaskOfferResponseState> decode_task_offer_response_state(const BoundaryValue&);
BoundaryValue encode_task_offer_response_state(const TaskOfferResponseState&);
}
