// 地方生活只共享有限目标、真实结果和来源引用；不持有第二钱包、库存或世界副本。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "resident/model/offer_ref.hpp"
namespace hanmo::resident {
struct CommonStore;struct DirectorState;struct DirectorCatalog;struct KnownSubjectView;
struct GrowthCatalog;struct GrowthAccess;struct TravelCatalog;struct TaskCommand;
// 作者声明的私人物资准备目标，不等同生理饥饿或新消费规则。
struct PrivateLivelihoodPolicy {
    StableId item_id;Integer target_quantity=0,cash_reserve=0,work_fatigue_limit=80;
    std::vector<StableId> known_service_role_ids;StableId return_place_id;
};
enum class ServiceKnowledgeKind { authored,local_observation };
// 远地只知道公开服务定义；此值不能携带实时余额、库存或占用。
struct KnownServiceRef {
    StableId role_id,place_id,settlement_id,activity,knowledge_source_id;
    ServiceKnowledgeKind knowledge_kind=ServiceKnowledgeKind::authored;Integer observed_tick=0;
};
// 持久计划只保存意图和已选来源，自有钱货始终重新读取共同存储。
struct LivelihoodIntent {
    StableId goal_id,item_id;Integer target_quantity=0;
    StableId selected_service_role_id,selected_shop_id,selected_source_id;
    Integer selected_tick=0;StableId return_place_id;
};
enum class LifeActivityKind { service,trade };
enum class LifeActivityPhase { active,completed,cancelled };
// 金钱和物品的双向实际来源分列；预留、已结算和行动完成不能混为一件事。
struct LifeActivityView {
    StableId fact_id,action_id,actor_id,provider_actor_id,activity;
    LifeActivityKind kind=LifeActivityKind::service;LifeActivityPhase phase=LifeActivityPhase::active;
    Integer start_tick=0,settlement_tick=-1,completed_tick=-1;bool success=false;StableId code;
    StableId item_id;Integer quantity=0,amount=0;
    StableId payer_wallet_id,payee_wallet_id,from_inventory_id,to_inventory_id;
    std::vector<StableId> participant_actor_ids;
};
// 只借当前候选中已登记的实际来源；不从动作消失或预期报酬推断完成。
class GrowthLifeRead {
public:
    virtual ~GrowthLifeRead()=default;
    virtual std::optional<LifeActivityView> life_result(const StableId& action_id) const=0;
    virtual std::vector<StableId> life_fact_ids(Integer after_tick,Integer through_tick) const=0;
};
// 公托结果只保存原任务、合同和真实结算引用；可回收回执不是第二结果权威。
struct RegionalTaskOutcomeRef {
    StableId task_id,contract_id,task_template_id,delivery_fact_id,transfer_fact_id;
    StableId settlement_id,receipt_id,action_id,state_digest;
};
// 当前本人合同的只读投影；累计款物来自TaskState，观察时刻不进入结果身份。
struct RegionalTaskOutcomeView {
    RegionalTaskOutcomeRef source;StableId item_id;Owner issuer;
    StableId report_role_id,task_status,contract_status,stop_reason;
    Integer agreed_quantity=0,delivered_quantity=0,eligible_delivered_quantity=0;
    Integer paid_reward=0,refunded_budget=0,refunded_reward=0,deadline_tick=0,observed_tick=0;
    bool execution_stopped=false,has_restitution=false;
};
enum class RegionalSourceKind { life_result,runtime_offer,task_outcome };
// 动态事务继续使用原OfferRef，不能把一个UI行号当成另一份机会或合同。
struct RegionalSourceRef {
    RegionalSourceKind kind=RegionalSourceKind::life_result;StableId fact_id,report_id,need_id;
    std::optional<OfferRef> offer_ref;Integer need_revision=0;
    std::optional<RegionalTaskOutcomeRef> task_outcome;
};
// 会话冻结当时来源及参与者；业务动作仅允许已验证的任务命令，不接收任意字典。
struct RegionalOpportunityBinding {
    StableId binding_id,topic_id,dialogue_id,observer_actor_id,provider_actor_id,role_id,place_id;
    RegionalSourceRef source;std::shared_ptr<const TaskCommand> task_command;
};
// 固定合同优先级之后调用；正式采用仍由中央重报价并事务提交。
Outcome<std::vector<DecisionProposal>> propose_private_livelihood(
    const CommonStore&,const DirectorState&,const DirectorCatalog&,const KnownSubjectView&,
    const GrowthCatalog&,const GrowthAccess&,const TravelCatalog&,const GrowthLifeRead&,Integer tick);
}
