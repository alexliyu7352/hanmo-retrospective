#include "resident/domains/tasks/shop_order_contacts/task_shop_order_contact_world.hpp"
// 完整私铺经营读链在一次命令/阶段栈内保活；早政治责任另走最小来源Context。
#pragma once
#include "growth_market_history_integration.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "political_shop_licensing_integration.hpp"
#include "resident/domains/commerce/private_shop_asset_responsibilities.hpp"
#include "resident/domains/commerce/private_shop_boundary.hpp"
#include "resident/domains/commerce/private_shop_business_view.hpp"
#include "resident/domains/growth/growth_skill_origin.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_asset_responsibilities.hpp"
#include "resident/domains/tasks/shop_orders/task_shop_order_business.hpp"
#include "world_read_stamp.hpp"
namespace hanmo::resident {
class ProjectionWriter;
struct WorldBundle;
class PrivateShopRentAuthorityWorldAccess;
struct NativeCompleteWorldCoreSources;
class NativePrivateShopExactSources;
// 原owner对象体进入同job分配账；成员深值仍由各自真实拥有路径计费。
class PrivateShopWorldAccess final : public CheckpointAllocatedObject {
public:
  static Outcome<std::unique_ptr<PrivateShopWorldAccess>>
  create(const WorldBundle &, const DynamicStepInput *phase = nullptr,
         const NativeEconomicFifoRead *actual_market = nullptr);
  // Complete前序显式传入；构造内部绝不再取完整World工厂。
  static Outcome<std::unique_ptr<PrivateShopWorldAccess>>
  create_for_complete(const NativeCompleteWorldCoreSources &,
                      const DynamicStepInput *phase = nullptr,
                      const NativeEconomicFifoRead *actual_market = nullptr);
  ~PrivateShopWorldAccess();
  PrivateShopWorldAccess(const PrivateShopWorldAccess &) = delete;
  PrivateShopWorldAccess &operator=(const PrivateShopWorldAccess &) = delete;
  const PrivateShopContext &context() const { return context_; }
  const PrivateShopMeetingContext &meeting() const { return meeting_; }
  PrivateShopCommandContext command() const;
  const PrivateShopOrderAccess &orders() const { return orders_; }
  TaskShopOrderViewAccess task_view() const;

private:
  static Outcome<std::unique_ptr<PrivateShopWorldAccess>>
  create_impl(const WorldBundle &, const DynamicStepInput *,
              const NativeEconomicFifoRead *,
              const NativeCompleteWorldCoreSources *);
  PrivateShopWorldAccess(const WorldBundle &,
                         std::unique_ptr<NativePrivateShopExactSources>,
                         std::unique_ptr<PoliticalShopLicensingWorldAccess>,
                         std::unique_ptr<PrivateShopRentAuthorityWorldAccess>,
                         const DynamicStepInput *,
                         const NativeEconomicFifoRead *);
  const WorldBundle &world_;
  // exact来源先于Law/Shop存活；后续context不借factory局部对象。
  std::unique_ptr<NativePrivateShopExactSources> exact_;
  std::unique_ptr<PoliticalShopLicensingWorldAccess> law_;
  GrowthMarketHistoryWorldRead market_;
  WorldCurrentMomentRead task_clock_;
  TaskFulfilledClientAccess clients_;
  EstateAccess estate_;
  InteractionAccess interaction_;
  GrowthAccess growth_;
  std::optional<TaskShopOrderAcceptanceAccess> acceptance_;
  std::optional<TaskShopOrderExecutionAccess> execution_;
  std::optional<TaskShopOrderContactAccess> order_contacts_;
  TaskPrivateShopAssetResponsibilityAccess closing_tasks_;
  GrowthSkillOriginContext skill_origins_;
  std::unique_ptr<PrivateShopRentAuthorityWorldAccess> rent_authority_;
  PrivateShopContext context_;
  PrivateShopMeetingContext meeting_;
  PrivateShopOrderAccess orders_;
  TaskShopOrderAcceptanceMeeting order_meeting_;
};
struct PrivateShopWorldCommit {
  StableId source_fact_id, action_id;
};
Status initialize_private_shop_world_in_candidate(WorldBundle &);
// 仅新局在首全验前按market、法律、私铺顺序初始化；恢复必须提供全部独立段。
Status initialize_private_commerce_world_in_candidate(WorldBundle &);
Status validate_growth_market_world(const WorldBundle &);
Status validate_private_shop_world(const WorldBundle &,
                                   ProjectionWriter *projection = nullptr);
Status validate_task_shop_orders_world(const WorldBundle &);
Outcome<PrivateShopCommandQuote>
prepare_private_shop_world(const WorldBundle &, const StableId &,
                           const PrivateShopCommand &);
Outcome<PrivateShopWorldCommit>
commit_private_shop_world(WorldBundle &, const StableId &,
                          const PrivateShopCommand &,
                          const PrivateShopCommandQuote &);
Outcome<TaskShopOrderQuote>
prepare_task_shop_order_world(const WorldBundle &, const StableId &,
                              const TaskShopOrderCommand &);
Outcome<PrivateShopWorldCommit>
commit_task_shop_order_world(WorldBundle &, const StableId &,
                             const TaskShopOrderCommand &,
                             const TaskShopOrderQuote &);
Outcome<BoundaryValue>
query_private_shop_world(const WorldBundle &, const PrivateShopBusinessQuery &);
Outcome<BoundaryValue>
query_task_shop_orders_world(const WorldBundle &, Integer cursor = 0,
                             const StableId &order_id = {});
Outcome<std::optional<StableId>>
step_private_shop_action_world(WorldBundle &, const StableId &,
                               const DynamicStepInput &);
Outcome<TaskEffect> step_task_shop_order_action_world(WorldBundle &,
                                                      const StableId &,
                                                      const DynamicStepInput &);
Status finish_private_shop_boundary_world(WorldBundle &);
} // namespace hanmo::resident
