// 商铺记录只存租约、资格来源与原LE账引用；资产仍在共同容器，成立本身不冒充实际营业日。
#pragma once
#include "private_shop_assets.hpp"
#include "private_shop_eligibility.hpp"
#include "private_shop_order_contract.hpp"
#include "private_shop_staff_contract.hpp"
#include "resident/domains/family/family_death.hpp"
#include "resident/domains/interaction/dialogue_provider_source.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
namespace hanmo::resident {
struct HouseholdRetailCatalog;
struct HouseholdRetailState;
struct PrivateShopOperationSourceContext;
struct PrivateShopAgeSources3;
class OriginalWorldReadBoundary;
struct PrivateShopDailyRentCatalog;
struct PrivateShopDailyRentState;
class PrivateShopRentVoluntaryAuthorityRead;
class TaskShopOrderContactRead;
class TaskPrivateShopAssetResponsibilityRead;
class TaskShopOrderAcceptanceRead;
struct TaskCatalog;
struct FamilyDeathState;
struct GrowthCatalog;
class GrowthLifeRead;
class PrivateShopMarketHistoryRead;
struct EstateCatalog;
struct EstateState;
struct EstateEmploymentState;
struct EstateAccess;
struct PrivateShopCatalog {
  Integer version = 1;
  OrderedTable<PrivateShopPremises> premises;
  PrivateShopStaffCatalog staff;
  PrivateShopOrderCatalog orders;
};
// 本域冻结履约引用，S3只读DTO后继不能悄悄改变本段持久化布局。
struct PrivateShopClientQualification {
  Owner original_client;
  SubjectRef delivery_beneficiary;
  StableId task_id, settlement_id, template_id, payment_fact_id;
  Integer completed_tick = 0;
  std::vector<StableId> objective_fact_ids;
};
// 历史资格只存不可变来源，不把报价epoch/revision持久化为永久许可证。
struct PrivateShopQualification {
  Integer minimum_commerce_xp = 0, minimum_commerce_level = 0;
  std::vector<StableId> commerce_study_source_ids;
  std::vector<PrivateShopClientQualification> original_clients;
  StableId permit_fact_id, clearance_fact_id;
};
struct PrivateShopRecord {
  StableId id, status = "preparing", wallet_id, inventory_id,
               rent_obligation_id;
  PrivateShopOpeningTerms terms;
  PrivateShopMeetingSource meeting;
  PrivateShopQualification qualification;
  Integer opened_tick = 0, markup_basis_points = 0;
  // 显式 /3 原开设时刻；旧 /1-/2 不编码且必须为空，不能从后来的营业时刻回填。
  std::optional<WorldMoment> opened_at;
};
// 本金与标价单独保原事实；本金只改变Common资金，永不进入经营收入或净收益。
struct PrivateShopCapitalFact {
  StableId id, shop_id, actor_id, from_wallet_id, to_wallet_id;
  Integer tick = 0, amount = 0;
};
struct PrivateShopMarkupFact {
  StableId id, shop_id, actor_id;
  Integer tick = 0, before_basis_points = 0, after_basis_points = 0;
};
// 入库只认原真实trade与本次新批次；旧背包货不按FIFO冒充本次购入成本。
struct PrivateShopStockLot {
  StableId lot_id;
  Integer quantity = 0;
};
struct PrivateShopPurchaseFact {
  StableId id, shop_id, trade_fact_id, item_id, market_shop_id,
      from_inventory_id, to_inventory_id;
  Integer tick = 0, quantity = 0, purchase_cash = 0;
  std::vector<PrivateShopStockLot> purchased_lots, stocked_lots;
};
// 每个营业日占本人两个真实半日；租金与原LE来源关联，取消不抹已发生的占铺费用。
struct PrivateShopOperation {
  StableId id, shop_id, actor_id, item_id, rent_arrear_id, rent_payment_id,
      result_fact_id, staff_contract_id;
  Integer start_tick = 0, markup_basis_points = 0;
  std::vector<Integer> attendance_ticks;
};
struct PrivateShopRetailLot {
  StableId purchase_fact_id, lot_id;
  Integer quantity = 0, cost = 0;
};
// 民间消费是明确的开放经济来源，价格保实际观测两数，原库存和本金不能冒收入。
struct PrivateShopOrderSettlement {
  StableId id, order_id, task_id, task_settlement_id, receipt_fact_id, outcome;
  Integer tick = 0, income = 0, cost_of_goods = 0;
  std::vector<PrivateShopOrderLotTransfer> delivered_lots;
  std::vector<PrivateShopRetailLot> consumed_lots;
};
struct PrivateShopDayResult {
  StableId id, operation_id, shop_id, actor_id, item_id, reason;
  Integer tick = 0, market_observed_tick = 0, market_revision = 0,
          market_stock = 0, base_price = 0, midpoint = 0;
  Integer quantity = 0, income = 0, cost_of_goods = 0, rent_paid = 0,
          wage_paid = 0, net_income = 0;
  bool completed = false;
  std::vector<PrivateShopRetailLot> consumed_lots;
};
// 关店只归还未售实物和真实剩余现金；不删除原工资、租金和订单历史。
struct PrivateShopClosingLot {
  StableId original_lot_id, destination_lot_id, item_id;
  Integer quantity = 0;
};
struct PrivateShopClosingRecord {
  StableId id, shop_id, actor_id, from_wallet_id, to_wallet_id,
      from_inventory_id, to_inventory_id;
  Integer tick = 0, returned_cash = 0;
  std::vector<PrivateShopClosingLot> returned_lots;
};
struct PrivateShopState {
  Integer version = 1, initialized_tick = 0, last_step_tick = 0;
  PrivateShopStaffState staff;
  PrivateShopOrderState orders;
  OrderedTable<PrivateShopRecord> shops;
  OrderedTable<PrivateShopClosingRecord> closings;
  OrderedTable<PrivateShopOrderSettlement> order_settlements;
  OrderedTable<PrivateShopOperation> operations;
  OrderedTable<PrivateShopDayResult> operating_results;
  OrderedTable<PrivateShopCapitalFact> capital;
  OrderedTable<PrivateShopMarkupFact> markups;
  OrderedTable<PrivateShopPurchaseFact> purchases;
};
// 所有域引用必须来自同一候选；纯客户/法律读各自证明历史，禁止调用者传任意准入布尔值。
struct PrivateShopContext {
  const CommonStore &common;
  const CommonCatalog &common_catalog;
  const FamilyCatalog &family_catalog;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const PrivateShopState &state;
  const PrivateShopCatalog &catalog;
  const InteractionCatalog &interaction_catalog;
  const TaskFulfilledClientRead &clients;
  const ActivityRead &activity;
  const PrivateShopPermissionRead *permissions = nullptr;
  const GrowthCatalog *markets = nullptr;
  const GrowthLifeRead *trades = nullptr;
  const DynamicStepInput *phase = nullptr;
  const PrivateShopMarketHistoryRead *market_history = nullptr;
  const EstateState *estates = nullptr;
  const EstateEmploymentState *estate_employment = nullptr;
  const EstateAccess *estate_access = nullptr;
  const TaskShopOrderAcceptanceRead *order_acceptance = nullptr;
  const TaskCatalog *order_tasks = nullptr;
  const FamilyDeathState *family_deaths = nullptr;
  const PrivateShopOrderExecutionRead *order_execution = nullptr;
  const GrowthState *study_history = nullptr;
  const TaskPrivateShopAssetResponsibilityRead *closing_tasks = nullptr;
  const EstateCatalog *estate_catalog = nullptr;
  // World借用同候选已完整核验的Family/Creation/Birth初源；旧独立域调用仍显式保原作者范围。
  const GrowthSkillOriginContext *skill_origins = nullptr;
  const PrivateShopDailyRentCatalog *daily_rent_catalog = nullptr;
  const PrivateShopDailyRentState *daily_rent = nullptr;
  const PrivateShopRentVoluntaryAuthorityRead *rent_executors = nullptr;
  FamilyDeathOriginRead rent_death_origin;
  // 仅从不可变作者接触profile派生，不入旧Shop存档；启用时缺真实只读服务必须拒绝。
  const TaskShopOrderContactRead *order_contacts = nullptr;
  bool order_contacts_required = false;
  // 仅已选真实买方profile借同候选原成交；不进入旧PrivateShop存档。
  const HouseholdRetailCatalog *retail_catalog = nullptr;
  const HouseholdRetailState *retail = nullptr;
  // Explicit paired exact operation/P29 selection. No field enters the
  // legacy private_shops/2 payload or reinterprets legacy attendance.
  const PrivateShopOperationSourceContext *exact_operations = nullptr;
  // Shop3 borrows original age basis, never a birthday or age-check ledger.
  const PrivateShopAgeSources3 *age_sources3 = nullptr;
  // 同 World 原 S0 范围证明，不是第二时钟或伪 DynamicStepInput。
  const OriginalWorldReadBoundary *read_boundary3 = nullptr;
};
Outcome<WorldMoment>
read_private_shop_observed_moment3(const PrivateShopContext &);
Status validate_private_shop_committed_read3(const PrivateShopContext &);
struct PrivateShopMeetingContext {
  const InteractionState &state;
  const InteractionAccess &access;
};
// 仅当显式日租启用时的原开设报价说明；不写旧Shop目录/记录或增加旧存档字段。
struct PrivateShopOpeningRentTerms {
  StableId profile, arrears_policy;
  Integer prepaid_from_initial_capital = 0, from_tick = 0, through_tick = 0;
  bool operator==(const PrivateShopOpeningRentTerms &other) const {
    return profile == other.profile && arrears_policy == other.arrears_policy &&
           prepaid_from_initial_capital == other.prepaid_from_initial_capital &&
           from_tick == other.from_tick && through_tick == other.through_tick;
  }
};
struct PrivateShopOpeningQuote {
  WorldIdentity source_identity;
  StableId actor_id, premises_id;
  PrivateShopOpeningAssetQuote assets;
  PrivateShopEligibility eligibility;
  AuthoredProviderSourceProof agreement;
  std::optional<PrivateShopOpeningRentTerms> daily_rent;
};
struct PrivateShopOpeningReceipt {
  StableId shop_id, wallet_id, inventory_id, rent_obligation_id;
};
Status validate_private_shop_catalog(const PrivateShopContext &);
// Original immutable opening/lease qualification only. Never calls the full
// shop/cash/retail/action validators, and grants no later title or occupancy.
Outcome<const PrivateShopRecord *>
read_original_private_shop_opening(const PrivateShopContext &,
                                   const SourceRef &);
// 本步严格验证已成立的Preparing租约；后续经营/雇员/关店记录须各自扩完整历史，不允许未知状态。
Status validate_private_shops(const PrivateShopContext &);
Outcome<PrivateShopOpeningQuote>
preview_private_shop_opening(const PrivateShopContext &,
                             const PrivateShopMeetingContext &,
                             const StableId &actor, const PrivateShopOpen &);
// 此口借外层Common保存点；所有失败必须由外层回滚整个World，不允许单独裸调后继续发布。
Outcome<PrivateShopOpeningReceipt>
open_private_shop_in_candidate(CommonStore &, LocalEconomyState &,
                               PrivateShopState &, const PrivateShopContext &,
                               const PrivateShopMeetingContext &,
                               const PrivateShopOpeningQuote &,
                               PrivateShopDailyRentState *daily_rent = nullptr);
// 独立域调用拥有一个Common保存点；中央完整候选只用上面的具名入口，避免嵌套保存点。
Outcome<PrivateShopOpeningReceipt>
open_private_shop(CommonStore &, LocalEconomyState &, PrivateShopState &,
                  const PrivateShopContext &, const PrivateShopMeetingContext &,
                  const PrivateShopOpeningQuote &,
                  PrivateShopDailyRentState *daily_rent = nullptr);
using PrivateShopManagementCommand =
    std::variant<PrivateShopFund, PrivateShopSetMarkup>;
struct PrivateShopManagementQuote {
  WorldIdentity source_identity;
  StableId actor_id;
  PrivateShopManagementCommand command;
  std::vector<ReadVersion> versions;
};
Outcome<PrivateShopManagementQuote>
preview_private_shop_management(const PrivateShopContext &,
                                const StableId &actor,
                                const PrivateShopManagementCommand &);
Outcome<StableId>
manage_private_shop_in_candidate(CommonStore &, PrivateShopState &,
                                 const PrivateShopContext &,
                                 const PrivateShopManagementQuote &);
Outcome<StableId> manage_private_shop(CommonStore &, PrivateShopState &,
                                      const PrivateShopContext &,
                                      const PrivateShopManagementQuote &);
Outcome<PrivateShopCatalog> decode_private_shop_catalog(const BoundaryValue &);
// 作者启用块与不可变目录分开解码；只剥离已经严格核真的enabled，不容忍其余未知字段。
Outcome<PrivateShopCatalog> decode_private_shop_profile(const BoundaryValue &);
Outcome<std::optional<PrivateShopCatalog>>
decode_private_shop_profile(const BoundaryValue &scenario,
                            const std::vector<StableId> &capabilities);
Outcome<PrivateShopState> decode_private_shop_state(const BoundaryValue &);
class ProjectionWriter;
Status emit_private_shop_state_fields(ProjectionWriter &,
                                      const PrivateShopState &);
bool is_default_private_shop_state(const PrivateShopState &) noexcept;
BoundaryValue encode_private_shop_state(const PrivateShopState &);
Status visit_private_shop_checkpoint(CodecVisitor &, PrivateShopState &);
Outcome<PrivateShopState> make_initial_private_shops(const CommonStore &);
// Explicit fresh selected version only; never retags existing records.
Outcome<PrivateShopState> make_initial_private_shops(const CommonStore &,
                                                     Integer selected_version);
} // namespace hanmo::resident
