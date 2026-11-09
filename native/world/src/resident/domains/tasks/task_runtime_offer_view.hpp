// 当前会面的动态机会和当时暂拒分列；历史不因Need后来失效而变为不存在。
#pragma once
#include "task_runtime_offer_response.hpp"
#include "task_lifecycle_view.hpp"
namespace hanmo::resident {
class EconomicSupplyPolicyRead;
Outcome<BoundaryValue> query_runtime_task_offer_conversation(
    const CommonStore&,const TaskState&,const AffairsState&,const SocialState&,
    const InteractionState&,const InteractionCatalog&,const InteractionAccess&,const KnownSubjectView&,
    const RuntimeTaskOfferResponses&,const StableId& actor,Integer cursor,const TaskConversationPreview&,
    const EconomicSupplyPolicyRead* issued_supply=nullptr);
}
