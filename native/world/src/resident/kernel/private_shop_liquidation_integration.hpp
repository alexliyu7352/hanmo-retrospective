// 身后清店借同一候选原店/真实死亡/实收文书；自持对象先于所有借用Context构造。
#pragma once
#include "private_shop_integration.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_view.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation_cached_receipt.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
namespace hanmo::resident {
struct WorldBundle;
class PrivateShopLiquidationWorldAccess final {
public:
 static Outcome<std::unique_ptr<PrivateShopLiquidationWorldAccess>> create(const WorldBundle&);
 PrivateShopLiquidationWorldAccess(const PrivateShopLiquidationWorldAccess&)=delete;
 PrivateShopLiquidationWorldAccess& operator=(const PrivateShopLiquidationWorldAccess&)=delete;
 const PrivateShopLiquidationContext& context()const{return context_;}
 const PrivateShopWorldAccess& shops()const{return *shops_;}
 const PrivateShopEstateCancellationAccess& cancellations()const{return cancellations_;}
private:
 PrivateShopLiquidationWorldAccess(const WorldBundle&,std::unique_ptr<PrivateShopWorldAccess>,TaskMessageNoticeServices);
 const WorldBundle& world_;
 std::unique_ptr<PrivateShopWorldAccess> shops_;
 TaskMessageNoticeServices notices_;
 PrivateShopLiquidationContext context_;
 PrivateShopEstateCancellationAccess cancellations_;
};
Status initialize_private_shop_liquidation_world_in_candidate(WorldBundle&);
Status validate_private_shop_liquidation_world(const WorldBundle&);
Outcome<PrivateShopLiquidationQuote> prepare_private_shop_liquidation_world(const WorldBundle&,const StableId&,const PrivateShopLiquidationCommand&);
Outcome<PrivateShopWorldCommit> commit_private_shop_liquidation_world(WorldBundle&,const StableId&,const PrivateShopLiquidationCommand&,const PrivateShopLiquidationQuote&);
Outcome<BoundaryValue> query_private_shop_liquidation_world(const WorldBundle&,const PrivateShopLiquidationViewQuery&);
Outcome<TaskShopOrderEstateCancellationQuote> prepare_task_shop_order_estate_world(const WorldBundle&,const StableId&,const TaskShopOrderEstateCancel&);
Outcome<InteractionEffect> commit_task_shop_order_estate_world(WorldBundle&,const StableId&,const TaskShopOrderEstateCancel&,const TaskShopOrderEstateCancellationQuote&);
Status bind_task_shop_order_estate_result_world(WorldBundle&,const TaskShopOrderEstateCancellationState* before,const StableId& actor,Integer request_seq,const CommitResult&);
// 真正提交尾持前态，实际result入Common后调用；最终完整World验证必须在此之后。
Status bind_private_shop_liquidation_result_world(WorldBundle&,const PrivateShopLiquidationState& before,const StableId& actor,Integer request_seq,const CommitResult&);
}
