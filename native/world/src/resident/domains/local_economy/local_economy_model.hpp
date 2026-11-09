// 地方人口、周期义务与生产的唯一类型状态；钱货只引用共同容器，不保存可花余额。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include "household_provisioning.hpp"
#include "membership_wages.hpp"
#include "production_management.hpp"
#include "resident/kernel/source_ref.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct PopulationMilitaryReadContext;
class PopulationTransferLaborCapture;
enum class PopulationStatus {
  civilian,
  training,
  trained_reserve,
  serving,
  wounded,
  captive,
  displaced
};
enum class ObligationKind { ration, wage, contract_payment };
enum class ProvisionOwner { macro, estate, military };
// 家庭资产容器由稳定家庭身份持有，群体拆分只复用引用。
struct HouseholdGroup {
  StableId id, wallet_id, inventory_id;
};
struct PopulationCohort {
  StableId id, site_id, household_group_id;
  PopulationStatus status = PopulationStatus::civilian;
  Integer headcount = 0, labor_capacity = 0;
  StableId private_wallet_id, private_inventory_id;
  std::optional<StableId> affiliation_id, assignment_id;
};
struct LaborAllocation {
  StableId id, cohort_id, activity_or_production_id;
  Integer workers = 0, from_tick = 0, through_tick = 0;
  StableId commitment_id;
};
struct PopulationTransfer {
  StableId id, from_cohort_id, to_cohort_id;
  Integer count = 0, labor_eligible_count = 0;
  StableId reason, source_fact_id;
  Integer effective_tick = 0;
};
// 军事死亡只由真实逐成员伤亡事实证明；伤、俘、离队不进入此人口损失表。
struct PopulationLoss {
  StableId id, cohort_id, source_fact_id, casualty_fact_id;
  Integer count = 0, labor_eligible_count = 0, effective_tick = 0;
  std::vector<StableId> member_ids;
};
// S5 consumes the actual S4 Named birth once. This is not an anonymous
// cohort increment or a second actor/population balance.
struct NamedBirthPopulationTransition {
  StableId id;
  SourceRef birth_fact_ref;
  StableId child_actor_id;
  WorldMoment occurred_at;
};
// 有理生产率逐周期保留余数；投入和输出均使用非负整数，不用浮点回写资产。
struct RecipeQuantity {
  StableId item_id;
  Integer numerator = 0, denominator = 1;
};
struct ProductionRecipe {
  StableId id;
  Integer period_ticks = 1, labor_per_batch = 1, region_factor_bp = 10000,
          season_factor_bp = 10000;
  std::vector<RecipeQuantity> inputs, outputs;
};
struct ProductionUnit {
  StableId id, site_id;
  Owner owner;
  std::vector<StableId> input_inventory_ids;
  StableId output_inventory_id;
  std::vector<StableId> labor_allocation_ids;
  Integer capacity = 0;
  StableId recipe_id;
  Integer next_due_tick = 0;
  StableId last_settlement_id;
  Integer labor_remainder = 0, factor_remainder = 0;
  std::vector<Integer> input_remainders, output_remainders;
};
// 工资应付按到期日冻结；汇总欠额仅由 ArrearRecord 派生。
struct RecurringObligation {
  StableId id;
  SubjectRef principal, beneficiary;
  ObligationKind kind = ObligationKind::wage;
  StableId source_wallet_or_inventory, destination, rate_profile, authority_ref,
      item_id;
  Integer period_ticks = 1, next_due_tick = 0;
  StableId last_receipt_id;
  bool active = true;
};
struct ObligationRate {
  StableId id;
  Integer numerator = 0, denominator = 1;
  bool per_capita = false;
};
struct ArrearRecord {
  StableId id, obligation_id;
  Integer due_tick = 0, original_due = 0, paid_amount = 0;
};
struct ProvisionBinding {
  StableId id;
  SubjectRef subject;
  Integer from_tick = 0, through_tick = 0;
  StableId payer_inventory, obligation_id;
  ProvisionOwner policy_owner = ProvisionOwner::macro;
  // 军事来源在当前单位、野战军或羁押方上解析；不冻结可能已失效的付款库存。
  StableId military_source_id;
};
struct ProductionBinding {
  StableId unit_id;
  ProvisionOwner policy_owner = ProvisionOwner::macro;
};
struct PaymentReceipt {
  StableId id, arrear_id, obligation_id, from_wallet_id, to_wallet_id;
  Integer tick = 0, due_tick = 0, amount = 0;
  StableId authority_ref;
};
struct RationReceipt {
  StableId id, obligation_id, inventory_id, item_id;
  SubjectRef beneficiary;
  Integer tick = 0, due_tick = 0, required = 0, consumed = 0, shortage = 0;
};
struct ProductionQuantity {
  StableId inventory_id, item_id;
  Integer quantity = 0;
};
struct ProductionReceipt {
  StableId id, unit_id, recipe_id;
  Integer tick = 0, due_tick = 0, allocated_workers = 0, effective_workers = 0,
          batches = 0;
  std::vector<ProductionQuantity> inputs, outputs, unrealized_output;
  std::vector<StableId> cause_fact_ids;
  std::optional<ProductionContext> production_context;
};
struct LocalSupplySource {
  StableId source_key, organization_id, site_id, inventory_id, item_id;
  Integer target_amount = 0;
  bool public_report = false;
};
struct LocalSupplyRegistration {
  StableId id, source_key, organization_id, site_id, inventory_id, item_id;
  Integer tick = 0;
};
struct LocalSupplyObservation {
  StableId id;
  SupplyObservation observation;
  Integer tick = 0;
  bool publicly_visible = false;
};
struct LocalEconomyCatalog {
  bool enabled = false;
  OrderedTable<HouseholdGroup> households;
  OrderedTable<PopulationCohort> cohorts;
  OrderedTable<LaborAllocation> labor_allocations;
  OrderedTable<ProductionRecipe> recipes;
  OrderedTable<ProductionUnit> production_units;
  OrderedTable<ObligationRate> rates;
  OrderedTable<RecurringObligation> obligations;
  OrderedTable<ProvisionBinding> provision_bindings;
  OrderedTable<ProductionBinding> production_bindings;
  OrderedTable<LocalSupplySource> supply_sources;
  std::vector<StableId> named_actors_excluded;
  std::optional<ProductionManagementCatalog> production_management;
  std::optional<HouseholdProvisionCatalog> household_provisioning;
};
struct LocalEconomyState {
  Integer last_step_tick = 0;
  OrderedTable<HouseholdGroup> households;
  OrderedTable<PopulationCohort> cohorts;
  OrderedTable<LaborAllocation> labor_allocations;
  OrderedTable<ProductionUnit> production_units;
  OrderedTable<RecurringObligation> obligations;
  OrderedTable<ProvisionBinding> provision_bindings;
  OrderedTable<ProductionBinding> production_bindings;
  OrderedTable<ArrearRecord> arrears;
  OrderedTable<PaymentReceipt> payments;
  OrderedTable<RationReceipt> rations;
  OrderedTable<ProductionReceipt> production_receipts;
  OrderedTable<PopulationTransfer> population_transfers;
  OrderedTable<PopulationLoss> population_losses;
  OrderedTable<Integer> obligation_remainders;
  OrderedTable<LocalSupplyRegistration> supply_registrations;
  OrderedTable<LocalSupplyObservation> supply_observations;
  OrderedTable<MembershipWageBinding> membership_wage_bindings;
  OrderedTable<MembershipTerminalFractionReceipt> membership_terminal_fractions;
  std::optional<ProductionManagementState> production_management;
  std::optional<HouseholdProvisionState> household_provisioning;
  OrderedTable<NamedBirthPopulationTransition>
      named_birth_population_transitions;
};
struct LocalEconomyEffects {
  std::vector<StableId> receipt_ids, fact_ids;
};
// 转移调用明确声明转走人数和劳力；责任拆分由同一操作完成，禁止复制家庭私产。
struct PopulationTransferRequest {
  StableId from_cohort_id, to_cohort_id, reason, source_fact_id;
  Integer count = 0, labor_eligible_count = 0, effective_tick = 0;
  PopulationStatus destination_status = PopulationStatus::training;
  std::optional<StableId> assignment_id;
  std::vector<StableId> release_allocation_ids;
};
Outcome<LocalEconomyState>
make_initial_local_economy(CommonStore &, const LocalEconomyCatalog &,
                           Integer initial_tick);
Outcome<LocalEconomyEffects>
step_local_economy(CommonStore &, LocalEconomyState &,
                   const LocalEconomyCatalog &, const OrganizationRead &,
                   const ActivityRead &, const DynamicStepInput &);
struct HouseholdProvisionManagementContext;
Outcome<LocalEconomyEffects>
step_local_economy(CommonStore &, LocalEconomyState &,
                   const LocalEconomyCatalog &, const OrganizationRead &,
                   const ActivityRead &, const DynamicStepInput &,
                   const HouseholdProvisionManagementContext *);
struct ProductionDisruptionContext;
// 显式减产段与原生产同候选；旧入口不启用该段。
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ActivityRead &, const DynamicStepInput &,
    const HouseholdProvisionManagementContext *, ProductionDisruptionContext *);
struct EconomicTakeoverPhaseContext;
// 接管段与原经济/减产同一事务；旧入口完全不启用新执行语义。
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ActivityRead &, const DynamicStepInput &,
    const HouseholdProvisionManagementContext *, ProductionDisruptionContext *,
    EconomicTakeoverPhaseContext *);
struct EstateEmploymentContext;
// 私人工资只由紧随Estate的真实服务结算；显式段关闭普通日历对该合同的重复应计。
Outcome<LocalEconomyEffects> step_local_economy(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ActivityRead &, const DynamicStepInput &,
    const HouseholdProvisionManagementContext *, ProductionDisruptionContext *,
    EconomicTakeoverPhaseContext *, const EstateEmploymentContext *);
class MarketBusinessCaptureServices;
// 原World外层同时保存Common/LE/市场/捕获状态；每笔生产和口粮在原收据后立即绑定市场。
Outcome<LocalEconomyEffects> step_local_economy_in_candidate(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ActivityRead &, const DynamicStepInput &,
    const HouseholdProvisionManagementContext *, ProductionDisruptionContext *,
    EconomicTakeoverPhaseContext *, const EstateEmploymentContext *,
    MarketBusinessCaptureServices *);
// 私约仅原业务应计/支付；同候选具名来源证明后宏观LE才跳过这些精确责任。
struct PrivateShopResponsibilitySourceContext;
struct EconomicExactValidationContext;
Outcome<LocalEconomyEffects> step_local_economy_in_candidate(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ActivityRead &, const DynamicStepInput &,
    const HouseholdProvisionManagementContext *, ProductionDisruptionContext *,
    EconomicTakeoverPhaseContext *, const EstateEmploymentContext *,
    MarketBusinessCaptureServices *,
    const PrivateShopResponsibilitySourceContext *);
struct EconomicTakeoverReadContext;
Outcome<std::vector<SupplyObservation>>
observe_local_supply(const CommonStore &, const LocalEconomyState &,
                     const LocalEconomyCatalog &);
Outcome<std::vector<SupplyObservation>>
observe_local_supply(const CommonStore &, const LocalEconomyState &,
                     const LocalEconomyCatalog &,
                     const EconomicTakeoverReadContext *);
Outcome<std::vector<SupplyObservation>>
refresh_local_supply_observations(CommonStore &, LocalEconomyState &,
                                  const LocalEconomyCatalog &, Integer tick);
Outcome<std::vector<SupplyObservation>>
refresh_local_supply_observations(CommonStore &, LocalEconomyState &,
                                  const LocalEconomyCatalog &, Integer tick,
                                  const EconomicTakeoverReadContext *);
Outcome<StableId> transfer_population(
    CommonStore &, LocalEconomyState &, const PopulationTransferRequest &,
    PopulationTransferLaborCapture *actual_labor_capture = nullptr);
Status allocate_labor(const CommonStore &, LocalEconomyState &,
                      const LaborAllocation &);
Outcome<Integer> outstanding_arrears(const LocalEconomyState &,
                                     const StableId &obligation_id);
std::optional<DynamicFactView>
local_economy_dynamic_fact(const LocalEconomyState &, const StableId &);
Status
validate_local_economy(const CommonStore &, const LocalEconomyState &,
                       const LocalEconomyCatalog &, const OrganizationRead &,
                       const PopulationMilitaryReadContext *military = nullptr);
struct EconomicTakeoverExecutionState;
// 显式接管合同只放宽有真实历史来源的Owner变化；当前支付仍逐决定核验。
Status validate_local_economy(const CommonStore &, const LocalEconomyState &,
                              const LocalEconomyCatalog &,
                              const OrganizationRead &,
                              const PopulationMilitaryReadContext *,
                              const EconomicTakeoverReadContext *,
                              const EconomicTakeoverExecutionState *);
// 新雇用义务必须具名反查完整同候选来源；旧重载遇到该账仍严格拒绝。
Status validate_local_economy(const CommonStore &, const LocalEconomyState &,
                              const LocalEconomyCatalog &,
                              const OrganizationRead &,
                              const PopulationMilitaryReadContext *,
                              const EconomicTakeoverReadContext *,
                              const EconomicTakeoverExecutionState *,
                              const EstateEmploymentContext *);
// 私租/私工资必须借同候选原业务来源，早位不依赖经营资格或法律全验。
struct PrivateShopResponsibilitySourceContext;
struct EconomicExactValidationContext;
Status validate_local_economy(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const PopulationMilitaryReadContext *,
    const EconomicTakeoverReadContext *, const EconomicTakeoverExecutionState *,
    const EstateEmploymentContext *,
    const PrivateShopResponsibilitySourceContext *,
    std::optional<Integer> unselected_clock_origin = {},
    bool exact_supply_source_scope = false,
    const EconomicExactValidationContext *exact = nullptr);
Outcome<LocalEconomyCatalog> decode_local_economy_catalog(
    const BoundaryValue &, bool military_adapter = false,
    bool named_membership = false, bool production_management = false,
    bool household_provisioning = false);
Outcome<LocalEconomyState>
decode_local_economy_state(const BoundaryValue &, bool military_adapter = false,
                           bool named_membership = false,
                           bool production_management = false,
                           bool household_provisioning = false);
class ProjectionWriter;
Status emit_local_economy_state_fields(ProjectionWriter &,
                                       const LocalEconomyState &,
                                       bool military_adapter = false,
                                       bool named_membership = false,
                                       bool production_management = false,
                                       bool household_provisioning = false);
BoundaryValue encode_local_economy_state(const LocalEconomyState &,
                                         bool military_adapter = false,
                                         bool named_membership = false,
                                         bool production_management = false,
                                         bool household_provisioning = false);
Status visit_local_economy_checkpoint(CodecVisitor &, LocalEconomyState &,
                                      bool military_adapter = false,
                                      bool named_membership = false,
                                      bool production_management = false,
                                      bool household_provisioning = false);
// Explicit selected semantic version. Versions B and B+5 retain exactly the
// original B optional-field flags. The World owner separately proves the real
// selected source-module pairing and supplies the actual checkpoint version.
Status validate_local_economy_codec_selection(Integer selected_version,
                                              bool military_adapter,
                                              bool named_membership,
                                              bool production_management,
                                              bool household_provisioning);
Outcome<LocalEconomyCatalog>
decode_local_economy_catalog(const BoundaryValue &, bool military_adapter,
                             bool named_membership, bool production_management,
                             bool household_provisioning,
                             Integer selected_version);
Outcome<LocalEconomyState>
decode_local_economy_state(const BoundaryValue &, bool military_adapter,
                           bool named_membership, bool production_management,
                           bool household_provisioning,
                           Integer selected_version);
Status emit_local_economy_state_fields(
    ProjectionWriter &, const LocalEconomyState &, bool military_adapter,
    bool named_membership, bool production_management,
    bool household_provisioning, Integer selected_version);
BoundaryValue encode_local_economy_state(const LocalEconomyState &,
                                         bool military_adapter,
                                         bool named_membership,
                                         bool production_management,
                                         bool household_provisioning,
                                         Integer selected_version);
Status visit_local_economy_checkpoint(CodecVisitor &, LocalEconomyState &,
                                      bool military_adapter,
                                      bool named_membership,
                                      bool production_management,
                                      bool household_provisioning,
                                      Integer selected_version);
} // namespace hanmo::resident
