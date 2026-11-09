// 限店清算独立保存生前双方决定；不改旧租约、订单主体、继产权源或任何资产Owner。
#pragma once
#include "private_shop_liquidation_contract.hpp"
#include "resident/domains/commerce/private_shop_closing.hpp"
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice.hpp"
namespace hanmo::resident {
// 作者只声明双方当面明确决定的入口，不能预制身后执行权或死亡通知。
struct PrivateShopExecutorPolicy {
    StableId id,premises_id,proprietor_actor_id,executor_actor_id;
    StableId proprietor_role_id,executor_role_id;
    StableId proprietor_dialogue_id,proprietor_appointment_node_id;
    StableId executor_dialogue_id,executor_acceptance_node_id;
};
struct PrivateShopLiquidationCatalog {Integer version=1;OrderedTable<PrivateShopExecutorPolicy> policies;};
struct PrivateShopExecutorMandate {
    StableId id,policy_id,shop_id,proprietor_actor_id,executor_actor_id;
    StableId shop_wallet_id,shop_inventory_id,return_wallet_id,return_inventory_id;
    StableId acceptance_fact_id;Integer appointed_tick=0;bool requested_by_executor=false;
    PrivateShopMeetingSource agreement;
};
struct PrivateShopExecutorAcceptance {StableId id,mandate_fact_id,executor_actor_id;Integer tick=0;};
struct PrivateShopExecutorRevocation {StableId id,mandate_fact_id,actor_id,decision;Integer tick=0;};
// 原旧关闭事实继续记录资产主体；本段另记录实际执行人及其生前/已收文书授权。
struct PrivateShopEstateClosingRecord {
    StableId id,closing_fact_id,shop_id,proprietor_actor_id,executor_actor_id;
    StableId mandate_fact_id,acceptance_fact_id,death_fact_id,death_cause_fact_id;
    StableId death_notice_binding_fact_id,death_notice_handoff_fact_id;Integer tick=0;
};
// 新段永久保存本次真实提交，不从被裁剪Common历史追认旧决定。
struct PrivateShopLiquidationResultBinding {StableId id,actor_id,command_kind,result_id;Integer request_seq=0,revision=0,tick=0;};
struct PrivateShopLiquidationState {
    Integer version=1;
    OrderedTable<PrivateShopExecutorMandate> mandates;
    OrderedTable<PrivateShopExecutorAcceptance> acceptances;
    OrderedTable<PrivateShopExecutorRevocation> revocations;
    OrderedTable<PrivateShopEstateClosingRecord> closings;
    OrderedTable<PrivateShopLiquidationResultBinding> result_bindings;
};
// 原店与生命、文书完整史先由World验证；本口只借本账与具名源，不回调完整Shop/Task/继产。
struct PrivateShopLiquidationContext {
    const CommonStore& common;const CommonCatalog& common_catalog;
    const PrivateShopState& shops;const PrivateShopCatalog& shop_catalog;
    const PrivateShopLiquidationState& state;const PrivateShopLiquidationCatalog& catalog;
    const InteractionCatalog& interaction_catalog;const FamilyDeathState& deaths;
    FamilyDeathOriginRead death_origin;TaskDeliveredDeathNoticeRead delivered_notice;
    // 严格单步仅复核旧声明前的原死因；实际新委托、偿租或关闭仍须已提交时钟。
    const DynamicStepInput* phase=nullptr;
};
struct PrivateShopAppointExecutor {StableId shop_id,policy_id;};
struct PrivateShopAcceptExecutorship {StableId shop_id,policy_id;};
struct PrivateShopRevokeExecutor {StableId mandate_fact_id,decision;};
using PrivateShopExecutorCommand=std::variant<PrivateShopAppointExecutor,PrivateShopAcceptExecutorship,PrivateShopRevokeExecutor>;
struct PrivateShopExecutorQuote {
    WorldIdentity source_identity;StableId actor_id;PrivateShopExecutorCommand command;
    StableId shop_id,policy_id,proprietor_actor_id,executor_actor_id;
    std::optional<AuthoredProviderSourceProof> agreement;
};
struct PrivateShopExecutorReceipt {StableId mandate_fact_id,acceptance_fact_id,revocation_fact_id;};
Status validate_private_shop_liquidation_catalog(const PrivateShopLiquidationContext&);
Status validate_private_shop_liquidation_authorizations(const PrivateShopLiquidationContext&);
Outcome<PrivateShopExecutorQuote> preview_private_shop_executor(const PrivateShopLiquidationContext&,const PrivateShopMeetingContext&,const ActivityRead&,const StableId& actor,const PrivateShopExecutorCommand&);
// 不建立内层Common点；两事实与本段必须随外层完整候选一起提交或撤回。
Outcome<PrivateShopExecutorReceipt> apply_private_shop_executor_in_candidate(CommonStore&,PrivateShopLiquidationState&,const PrivateShopLiquidationContext&,const PrivateShopMeetingContext&,const ActivityRead&,const PrivateShopExecutorQuote&);
struct PrivateShopEstateClose {StableId shop_id,mandate_fact_id,death_notice_binding_fact_id;};
struct PrivateShopEstateClosingQuote {WorldIdentity source_identity;StableId actor_id;PrivateShopEstateClose command;PrivateShopClosingQuote assets;PrivateShopEstateCancellationSource authority;};
// 共享证明DTO中的request.order_id必须为空；两个具名口只认原private_shop_closed前界，不接受Task头。
Outcome<PrivateShopEstateCancellationSource> read_private_shop_estate_closing_authority(const PrivateShopLiquidationContext&,const StableId& executor,const PrivateShopEstateClose&);
Outcome<PrivateShopEstateCancellationSource> read_private_shop_estate_closing_authority_before_fact(const PrivateShopLiquidationContext&,const StableId& executor,const PrivateShopEstateClose&,const StableId& actual_closing_fact,Integer tick);
Outcome<PrivateShopEstateClosingQuote> preview_private_shop_estate_closing(const PrivateShopContext&,const PrivateShopLiquidationContext&,const StableId& executor,const PrivateShopEstateClose&);
Outcome<StableId> close_private_shop_estate_in_candidate(CommonStore&,PrivateShopState&,PrivateShopLiquidationState&,const PrivateShopContext&,const PrivateShopLiquidationContext&,const PrivateShopEstateClosingQuote&);
Status validate_private_shop_estate_closings(const PrivateShopContext&,const PrivateShopLiquidationContext&);
class PrivateShopEstateCancellationAccess final:public PrivateShopEstateCancellationRead {
public:
    explicit PrivateShopEstateCancellationAccess(const PrivateShopLiquidationContext& context):context_(context){}
    PrivateShopEstateCancellationAccess(PrivateShopLiquidationContext&&)=delete;
    Outcome<PrivateShopEstateCancellationSource> private_shop_estate_cancellation(const PrivateShopEstateCancellationRequest&)const override;
    Outcome<PrivateShopEstateCancellationSource> private_shop_estate_cancellation_before_settlement(const PrivateShopEstateCancellationRequest&,const StableId&,Integer)const override;
private:
    const PrivateShopLiquidationContext& context_;
};
}
