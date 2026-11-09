// 日租纯权源先于完整Shop读构造；权限只借原委托/死因/实收文书，不反建完整Shop链。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "private_shop_integration.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_view.hpp"
#include "resident/domains/commerce/daily_rent/private_shop_daily_rent_cached_receipt.hpp"
#include "resident/domains/commerce/liquidation/private_shop_liquidation.hpp"
#include "resident/domains/tasks/message_notices/task_message_notice_world.hpp"
namespace hanmo::resident {
// 原owner对象体进入同job分配账；成员深值仍由各自真实拥有路径计费。
class PrivateShopRentAuthorityWorldAccess final : public CheckpointAllocatedObject {
public:
 static Outcome<std::unique_ptr<PrivateShopRentAuthorityWorldAccess>> create(const WorldBundle&,const DynamicStepInput* phase=nullptr);
 PrivateShopRentAuthorityWorldAccess(const PrivateShopRentAuthorityWorldAccess&)=delete;
 PrivateShopRentAuthorityWorldAccess& operator=(const PrivateShopRentAuthorityWorldAccess&)=delete;
 const PrivateShopRentVoluntaryAuthorityRead* executors()const{return executors_?&*executors_:nullptr;}
 const FamilyDeathOriginRead& death_origin()const{return death_origin_;}
private:
 PrivateShopRentAuthorityWorldAccess(const WorldBundle&,std::optional<TaskMessageNoticeServices>,const DynamicStepInput*);
 const WorldBundle& world_;
 FamilyDeathOriginRead death_origin_;
 std::optional<TaskMessageNoticeServices> notices_;
 std::optional<PrivateShopLiquidationContext> authority_;
 std::optional<PrivateShopRentExecutorAccess> executors_;
};
Status initialize_private_shop_daily_rent_world_in_candidate(WorldBundle&);
Status validate_private_shop_daily_rent_world(const WorldBundle&);
// 最后完整控制史通过后调用；不回调Shop、继产或控制写者。
Status validate_private_shop_daily_rent_player_history_world(const WorldBundle&);
// 唯一clock提交后立即调用；早位LE不得替原私人租约自动偿付欠期。
Status reconcile_private_shop_daily_rent_world_in_candidate(WorldBundle&,const DynamicStepInput&);
Outcome<PrivateShopDailyRentQuote> prepare_private_shop_daily_rent_world(const WorldBundle&,const StableId&,const PrivateShopDailyRentCommand&);
Outcome<PrivateShopWorldCommit> commit_private_shop_daily_rent_world(WorldBundle&,const StableId&,const PrivateShopDailyRentCommand&,const PrivateShopDailyRentQuote&);
Outcome<BoundaryValue> query_private_shop_daily_rent_world(const WorldBundle&,const PrivateShopDailyRentQuery&);
Status bind_private_shop_daily_rent_result_world(WorldBundle&,const PrivateShopDailyRentState& before,const StableId&,Integer request_seq,const CommitResult& actual_result);
}
