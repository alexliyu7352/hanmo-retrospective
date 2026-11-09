// 宏观生产管理的显式v1扩展；土地设施是本域唯一生产占用，钱货仍在CommonStore。
#pragma once
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
struct EconomicTakeoverReadContext;
struct EconomicTakeoverPhaseContext;
struct EconomicWorkValidationContext;
struct LocalEconomyCatalog;
struct LocalEconomyState;
struct LocalEconomyEffects;
struct ProductionUnit;
enum class ProductionAssetKind { land, facility };
// 新宏观资产必须有唯一作者来源，既有estate作用域不在这里再次登记。
struct ProductionAsset {
  StableId id, site_id, source_key;
  Owner owner;
  ProductionAssetKind kind = ProductionAssetKind::land;
  Integer capacity_units = 0;
};
struct ProductionAssetUse {
  StableId asset_id;
  Integer units = 0, batches_per_unit = 0;
};
// 每段长度一致；有界季节表仅影响之后到期的周期，不模拟天气或改写历史收成。
struct ProductionSeasonProfile {
  StableId id;
  Integer origin_tick = 0, phase_ticks = 1;
  std::vector<Integer> factors_bp;
};
struct ProductionCapacityPlan {
  StableId unit_id, season_profile_id;
  std::vector<ProductionAssetUse> asset_uses;
};
struct ProductionManagementCatalog {
  Integer version = 1;
  OrderedTable<ProductionAsset> assets;
  OrderedTable<ProductionSeasonProfile> seasons;
  OrderedTable<ProductionCapacityPlan> plans;
};
// 收据冻结本次实际采用的有限系数与物理容量，查询不能拿当前配置回算旧周期。
struct ProductionContext {
  Integer capacity = 0, region_factor_bp = 10000, season_factor_bp = 10000,
          season_phase = -1;
  StableId season_profile_id;
  std::vector<StableId> asset_ids;
};
struct ProductionLaborTarget {
  StableId allocation_id;
  Integer workers = 0;
};
enum class ProductionConfigurationKind { configure, cancel_pending };
// 新计划完整覆盖该生产单元已承诺的劳力来源；0人就是合法停工，不抹工资或旧欠款。
struct ConfigureLocalProductionCommand {
  StableId actor_id, unit_id;
  std::optional<ActingAuthority> authority;
  ProductionConfigurationKind kind = ProductionConfigurationKind::configure;
  std::vector<ProductionLaborTarget> targets;
};
struct ProductionConfigurationQuote {
  ConfigureLocalProductionCommand command;
  Integer requested_tick = 0, effective_tick = 0, first_due_tick = 0;
  StableId previous_configuration_id;
};
// 增加的未来劳力引用真实LaborAllocation预留，征募不能在另一表重复占用。
struct ProductionConfigurationRecord {
  StableId id, unit_id, actor_id;
  Integer requested_tick = 0, effective_tick = 0, first_due_tick = 0;
  std::optional<ActingAuthority> authority;
  std::vector<ProductionLaborTarget> targets;
  std::vector<Integer> prior_workers;
  std::vector<StableId> reservation_allocation_ids;
};
struct ProductionConfigurationResult {
  StableId id, configuration_id, kind;
  Integer tick = 0;
};
struct ProductionManagementState {
  Integer version = 1;
  OrderedTable<ProductionConfigurationRecord> configurations;
  OrderedTable<ProductionConfigurationResult> results;
  OrderedTable<StableId> pending_by_unit;
};
// 暂定：未接真实治理来源前只接受中性输入；不改旧收据、余数或历史产量。
struct ProductionGovernanceInput {
  Integer factor_bp = 10000;
};
// 保留旧三参数入口及其二进制符号，内部显式采用中性治理输入。
Outcome<ProductionContext> production_context(const LocalEconomyCatalog &,
                                              const ProductionUnit &,
                                              Integer due_tick);
Outcome<ProductionContext>
production_context(const LocalEconomyCatalog &, const ProductionUnit &,
                   Integer due_tick, const ProductionGovernanceInput &);
Outcome<ProductionConfigurationQuote> preview_configure_local_production(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &);
Outcome<ProductionConfigurationQuote> preview_configure_local_production(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &,
    const EconomicTakeoverReadContext *);
Outcome<LocalEconomyEffects> apply_configure_local_production(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &);
Outcome<LocalEconomyEffects> apply_configure_local_production(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &,
    EconomicTakeoverPhaseContext *);
// 仅供已持完整WorldBundle候选或Common保存点的命令组合；失败时调用方必须撤回共同写集。
Outcome<LocalEconomyEffects> apply_configure_local_production_in_candidate(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &);
Outcome<LocalEconomyEffects> apply_configure_local_production_in_candidate(
    CommonStore &, LocalEconomyState &, const LocalEconomyCatalog &,
    const OrganizationRead &, const ConfigureLocalProductionCommand &,
    EconomicTakeoverPhaseContext *);
// 此钩子只在旧周期生产收据完成后激活新计划，失败由原经济阶段保存点全回滚。
Status activate_production_configuration(CommonStore &, LocalEconomyState &,
                                         const StableId &unit_id,
                                         Integer settled_due_tick,
                                         LocalEconomyEffects &);
Status validate_production_management(const CommonStore &,
                                      const LocalEconomyState &,
                                      const LocalEconomyCatalog &);
struct EconomicTakeoverExecutionState;
struct EconomicTakeoverReadContext;
struct EconomicTakeoverPhaseContext;
struct EconomicWorkValidationContext;
Status validate_production_management(const CommonStore &,
                                      const LocalEconomyState &,
                                      const LocalEconomyCatalog &,
                                      const EconomicTakeoverExecutionState *);
Status validate_production_management(
    const CommonStore &, const LocalEconomyState &, const LocalEconomyCatalog &,
    const EconomicTakeoverReadContext *, const EconomicTakeoverExecutionState *,
    const EconomicWorkValidationContext *exact = nullptr);
Outcome<ConfigureLocalProductionCommand>
decode_configure_local_production(const BoundaryValue &);
BoundaryValue
encode_configure_local_production(const ConfigureLocalProductionCommand &);
BoundaryValue
encode_production_configuration_quote(const ProductionConfigurationQuote &);
} // namespace hanmo::resident
