// 具名配置与读取沿同一候选的CommonStore/机构授权；不重建账本、不推进另一时钟。
#include "economic_world_integration.hpp"
#include "household_provision_integration.hpp"
#include "occupation_fiscal_integration.hpp"
#include "political_takeover_execution_world.hpp"
#include "resident/domains/local_economy/disruptions/production_disruptions_view.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_need_view.hpp"
#include "resident/domains/local_economy/fiscal/fiscal_taxpayer_continuity_view.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_adapter.hpp"
#include "resident/domains/local_economy/wages/cohort_wage_need_view.hpp"
#include "resident/domains/organization/organization_office_history.hpp"
#include "resident/domains/politics/politics_fiscal.hpp"
#include "resident/storage/common_checkpoint.hpp"
#include "world_bundle.hpp"
#include <set>
namespace hanmo::resident {
namespace {
// 旧地方经济仍可合法读账；生产管理能力另由明确v1块开启。
bool economy_available(const WorldBundle &world) {
  return world.catalog && world.dynamic_enabled() &&
         world.catalog->local_economy &&
         world.catalog->local_economy->enabled && world.catalog->organization;
}
} // namespace
// 新能力不借全局capability摘要默认开启，缺块或未知版本都保持关闭。
bool production_management_enabled(const WorldBundle &world) {
  return economy_available(world) &&
         world.catalog->local_economy->production_management &&
         world.catalog->local_economy->production_management->version == 1;
}
// 外层actor必须匹配业务发令者；NPC和玩家均复用同一领域资格与资源检查。
Outcome<ProductionConfigurationQuote>
prepare_economic_configuration(const WorldBundle &world, const StableId &actor,
                               const ConfigureLocalProductionCommand &command) {
  if (actor != command.actor_id)
    return {false, {}, common_failure("CommandActorMismatch").error};
  if (!production_management_enabled(world))
    return {false,
            {},
            common_failure("ProductionManagementUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto organizations = world.services.organization_access();
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  return preview_configure_local_production(
      world.common, world.local_economy, *world.catalog->local_economy,
      organizations, command, takeover.value->economy());
}
// 应用再次复验同候选；外层仍持request去重及整世界保存点，本域只提交自己已通过的经济写集，不嵌套共同保存点。
Outcome<LocalEconomyEffects>
apply_economic_configuration(WorldBundle &world, const StableId &actor,
                             const ConfigureLocalProductionCommand &command) {
  auto prepared = prepare_economic_configuration(world, actor, command);
  if (!prepared.ok)
    return {false, {}, prepared.error};
  auto organizations = world.services.organization_access();
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  std::optional<EconomicTakeoverPhaseContext> execution;
  if (takeover.value->economy())
    execution.emplace(EconomicTakeoverPhaseContext{
        *takeover.value->economy(), world.economic_takeover_execution});
  return apply_configure_local_production_in_candidate(
      world.common, world.local_economy, *world.catalog->local_economy,
      organizations, command, execution ? &*execution : nullptr);
}
// 索引只保存当前本人及其真实授权的源ID；构建不改变知情、资产或随机流。
LocalEconomyIndex rebuild_economic_world_index(const WorldBundle &world) {
  if (!economy_available(world)) {
    LocalEconomyIndex empty;
    empty.identity = world.common.state().identity;
    empty.observer_actor_id = world.common.state().metadata.player_actor_id;
    return empty;
  }
  auto organizations = world.services.organization_access();
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
  if (!takeover.ok) {
    LocalEconomyIndex empty;
    empty.identity = world.common.state().identity;
    empty.observer_actor_id = world.common.state().metadata.player_actor_id;
    return empty;
  }
  auto index = build_local_economy_index(
      world.common, world.local_economy, *world.catalog->local_economy,
      world.organization, organizations, takeover.value->economy());
  if (world.catalog->affairs &&
      world.catalog->affairs->cohort_wage_need_policies)
    bind_cohort_wage_need_references(index, world.affairs);
  if (world.production_disruptions_enabled())
    index_production_disruptions(world.common,
                                 *world.catalog->production_disruptions,
                                 world.production_disruptions, index);
  return index;
}
// 显式接收派生Index，避免读取尚未登记的WorldBundle字段或按查询临时全世界扫描。
Outcome<LocalEconomyPage> read_economic_world(const WorldBundle &world,
                                              const LocalEconomyIndex &index,
                                              const LocalEconomyQuery &query) {
  if (!economy_available(world))
    return {false,
            {},
            common_failure("LocalEconomyUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto organizations = world.services.organization_access();
  return query_local_economy(world.common, world.local_economy,
                             *world.catalog->local_economy, organizations,
                             index, query);
}
// observer始终取当前world元数据，place仍由真正位置、index身份与领域授权共同复核。
Outcome<BoundaryValue> query_economic_world(const WorldBundle &world,
                                            const LocalEconomyIndex &index,
                                            const BoundaryValue &args) {
  auto query = decode_local_economy_query(
      args, world.common.state().metadata.player_actor_id);
  if (!query.ok)
    return {false, {}, query.error};
  auto page = read_economic_world(world, index, query.value);
  if (!page.ok)
    return {false, {}, page.error};
  auto encoded = encode_local_economy_page(page.value);
  if (world.production_disruptions_enabled()) {
    auto annotated = encode_economy_with_production_disruptions(
        world.common, *world.catalog->production_disruptions,
        world.production_disruptions, index, page.value);
    if (!annotated.ok)
      return annotated;
    encoded = std::move(annotated.value);
  }
  if (world.economic_supply_policies_enabled())
    encoded.as<BoundaryValue::Dict>()->emplace_back(
        "economic_supply_policies_available", BoundaryValue{true});
  encoded.as<BoundaryValue::Dict>()->emplace_back(
      "configuration_available",
      BoundaryValue{world.production_management_enabled()});
  encoded.as<BoundaryValue::Dict>()->emplace_back(
      "fiscal_available", BoundaryValue{world.fiscal_settlement_enabled()});
  if (world.occupation_fiscal_enabled())
    encoded.as<BoundaryValue::Dict>()->emplace_back(
        "occupation_fiscal_available", BoundaryValue{true});
  if (world.fiscal_taxpayer_continuity_enabled())
    encoded.as<BoundaryValue::Dict>()->emplace_back(
        "fiscal_taxpayer_continuity_available", BoundaryValue{true});
  encoded.as<BoundaryValue::Dict>()->emplace_back(
      "household_provision_management_available",
      BoundaryValue{household_provision_management_enabled(world)});
  return {true, std::move(encoded), {}};
}
// 地区公库来自S6唯一登记，明确拒绝把同一公共钱包循环税给自己。
Status validate_economic_fiscal_world(const WorldBundle &world) {
  if (!world.fiscal_settlement_enabled())
    return common_success();
  for (const auto &row : world.catalog->fiscal_settlement->districts.rows()) {
    const Jurisdiction *jurisdiction = nullptr;
    for (const auto &j : world.politics.jurisdictions.rows())
      if (j.second.seat_place_id == row.second.site_id) {
        if (jurisdiction)
          return common_failure("DuplicateFiscalJurisdiction", row.first);
        jurisdiction = &j.second;
      }
    if (!jurisdiction)
      return common_failure("UnknownFiscalJurisdiction", row.first);
    for (const auto &source : row.second.sources.rows())
      if (source.second.wallet_id == jurisdiction->public_wallet_id ||
          std::find(jurisdiction->public_inventory_ids.begin(),
                    jurisdiction->public_inventory_ids.end(),
                    source.second.inventory_id) !=
              jurisdiction->public_inventory_ids.end())
        return common_failure("FiscalPublicSelfTaxSource", source.first);
  }
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  auto organizations = world.services.organization_access();
  FiscalPolicyAccess policy(world.common, *world.catalog->common,
                            world.politics, world.political_fiscal,
                            *world.catalog->political_fiscal,
                            takeover.value->fiscal());
  std::optional<FiscalTaxpayerContinuityContext> continuity;
  if (world.fiscal_taxpayer_continuity_enabled()) {
    if (!takeover.value->taxpayer())
      return common_failure("FiscalTaxpayerContinuityReaderRequired");
    continuity.emplace(FiscalTaxpayerContinuityContext{
        *takeover.value->taxpayer(), world.fiscal_taxpayer_continuity});
  }
  auto legacy = validate_fiscal_settlement(
      world.common, world.local_economy, *world.catalog->local_economy,
      organizations, policy, *world.catalog->fiscal_settlement,
      world.fiscal_settlement, takeover.value->economy(),
      continuity ? &*continuity : nullptr);
  if (!legacy.ok)
    return legacy;
  if (!world.occupation_fiscal_enabled())
    return common_success();
  auto current = OccupationFiscalSettlementWorldAccess::create(world);
  if (!current.ok)
    return {false, {}, current.error};
  return validate_occupation_fiscal_settlement(
      current.value->context(), world.occupation_fiscal_settlement);
}
// 原地方生产先完成，明确的阶段读才允许取to_tick政策；不暂改世界tick。
Outcome<LocalEconomyEffects>
step_economic_fiscal_world(WorldBundle &world, const DynamicStepInput &input) {
  if (!world.fiscal_settlement_enabled())
    return {true, {}, {}};
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world, &input);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  auto organizations = world.services.organization_access();
  FiscalPolicyAccess policy(world.common, *world.catalog->common,
                            world.politics, world.political_fiscal,
                            *world.catalog->political_fiscal, input,
                            takeover.value->fiscal());
  std::optional<FiscalTaxpayerContinuityExecutionContext> continuity;
  if (world.fiscal_taxpayer_continuity_enabled()) {
    if (!takeover.value->taxpayer())
      return {false,
              {},
              common_failure("FiscalTaxpayerContinuityReaderRequired").error};
    continuity.emplace(*takeover.value->taxpayer(),
                       world.fiscal_taxpayer_continuity, &input);
  }
  auto legacy = step_fiscal_settlement(
      world.common, world.local_economy, *world.catalog->local_economy,
      organizations, policy, *world.catalog->fiscal_settlement,
      world.fiscal_settlement, input, takeover.value->economy(),
      continuity ? &*continuity : nullptr);
  if (!legacy.ok || !world.occupation_fiscal_enabled())
    return legacy;
  auto current = OccupationFiscalSettlementWorldAccess::create(world, &input);
  if (!current.ok)
    return {false, {}, current.error};
  auto step = step_occupation_fiscal_settlement(
      world.common, world.occupation_fiscal_settlement,
      current.value->context(), input);
  if (!step.ok)
    return step;
  legacy.value.fact_ids.insert(legacy.value.fact_ids.end(),
                               step.value.fact_ids.begin(),
                               step.value.fact_ids.end());
  legacy.value.receipt_ids.insert(legacy.value.receipt_ids.end(),
                                  step.value.receipt_ids.begin(),
                                  step.value.receipt_ids.end());
  return legacy;
}
// 所有身份字段最终就位后只派生合法ID序列，索引从不进冷存档。
FiscalSettlementIndex rebuild_economic_fiscal_index(const WorldBundle &world) {
  if (!world.fiscal_settlement_enabled()) {
    FiscalSettlementIndex out;
    out.identity = world.common.state().identity;
    out.observer_actor_id = world.common.state().metadata.player_actor_id;
    return out;
  }
  auto organizations = world.services.organization_access();
  auto index = build_fiscal_settlement_index(
      world.common, world.fiscal_settlement, world.organization, organizations);
  if (world.catalog->affairs && world.catalog->affairs->fiscal_need_policies)
    bind_fiscal_need_references(index, world.affairs);
  return index;
}
// 与生产页同一严格本人/现场入口；财政查询只读原应付和实付，不调用阶段未来读取。
Outcome<BoundaryValue>
query_economic_fiscal_world(const WorldBundle &world,
                            const FiscalSettlementIndex &index,
                            const BoundaryValue &args) {
  if (!world.fiscal_settlement_enabled())
    return {false,
            {},
            common_failure("FiscalSettlementUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto query = decode_local_economy_query(
      args, world.common.state().metadata.player_actor_id);
  if (!query.ok)
    return {false, {}, query.error};
  auto organizations = world.services.organization_access();
  auto page = query_fiscal_settlement(world.common, world.fiscal_settlement,
                                      *world.catalog->fiscal_settlement,
                                      organizations, index, query.value);
  if (!page.ok)
    return {false, {}, page.error};
  return {true, encode_fiscal_settlement_page(page.value), {}};
}
// 新连续性页使用原公职及真实来源，所有只读借口在本次调用栈存活。
Outcome<BoundaryValue>
query_fiscal_taxpayer_continuity_world(const WorldBundle &world,
                                       const BoundaryValue &args) {
  if (!world.fiscal_taxpayer_continuity_enabled())
    return {
        false, {}, common_failure("FiscalTaxpayerContinuityUnavailable").error};
  auto query = decode_local_economy_query(
      args, world.common.state().metadata.player_actor_id);
  if (!query.ok)
    return {false, {}, query.error};
  auto takeover = PoliticalTakeoverExecutionWorldAccess::create(world);
  if (!takeover.ok)
    return {false, {}, takeover.error};
  if (!takeover.value->taxpayer() || !takeover.value->economy())
    return {false,
            {},
            common_failure("FiscalTaxpayerContinuityReaderRequired").error};
  auto organizations = world.services.organization_access();
  FiscalPolicyAccess policy(world.common, *world.catalog->common,
                            world.politics, world.political_fiscal,
                            *world.catalog->political_fiscal,
                            takeover.value->fiscal());
  FiscalTaxpayerContinuityContext continuity{*takeover.value->taxpayer(),
                                             world.fiscal_taxpayer_continuity};
  FiscalTaxpayerContinuityViewContext context{world.common,
                                              world.local_economy,
                                              *world.catalog->local_economy,
                                              world.organization,
                                              organizations,
                                              policy,
                                              *world.catalog->fiscal_settlement,
                                              world.fiscal_settlement,
                                              *takeover.value->economy(),
                                              continuity};
  auto index = build_fiscal_taxpayer_continuity_index(context);
  if (!index.ok)
    return {false, {}, index.error};
  auto page =
      query_fiscal_taxpayer_continuity(context, index.value, query.value);
  if (!page.ok)
    return {false, {}, page.error};
  return {true, encode_fiscal_taxpayer_continuity_page(page.value), {}};
}
// 未知字段或无真实来源的空回执不能成为成功配置结果。
Outcome<LocalEconomyEffects>
decode_economic_configuration_effects(const BoundaryValue &value) {
  try {
    boundary::fields(value, {"receipt_ids", "fact_ids"}, {},
                     "/production_configuration");
    LocalEconomyEffects out;
    out.receipt_ids = boundary::strings(
        boundary::field(value, "receipt_ids", ""), "/receipt_ids");
    out.fact_ids =
        boundary::strings(boundary::field(value, "fact_ids", ""), "/fact_ids");
    if (out.fact_ids.empty() || out.fact_ids.size() > 2 ||
        out.fact_ids != out.receipt_ids)
      boundary::fail("InvalidProductionConfigurationReceipt",
                     "/production_configuration");
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
// 是否允许新tag由中央共同feature门决定，这里严格逐字段保存两个引用序列。
Status visit_economic_configuration_effects(CodecVisitor &codec,
                                            LocalEconomyEffects &effects) {
  common_checkpoint::strings(codec, "receipt_ids", effects.receipt_ids);
  auto result = common_checkpoint::strings(codec, "fact_ids", effects.fact_ids);
  if (!result.ok)
    return result;
  if (effects.fact_ids.empty() || effects.fact_ids.size() > 2 ||
      effects.fact_ids != effects.receipt_ids)
    return codec.reject("InvalidProductionConfigurationReceipt");
  return codec.status();
}
// 缓存成功结果的每个ID须属于同一次历史分配区间，不能借别人的旧计划伪造配置回执。
Status validate_economic_configuration_receipts(const WorldBundle &world) {
  std::set<StableId> used;
  for (const auto &row : world.common.state().results.rows()) {
    if (!row.second)
      continue;
    const auto &result = *row.second;
    const auto *effects =
        result.details ? std::get_if<LocalEconomyEffects>(&result.details->data)
                       : nullptr;
    if (world.economic_supply_policies_enabled() &&
        result.kind == "political_issue_supply_policy")
      continue;
    if (!effects) {
      if (result.kind == "configure_local_production")
        return common_failure("ProductionConfigurationReceiptMismatch",
                              row.first);
      continue;
    }
    if (!world.production_management_enabled() ||
        result.kind != "configure_local_production" ||
        effects->fact_ids.empty() || effects->fact_ids.size() > 2 ||
        effects->fact_ids != effects->receipt_ids)
      return common_failure("ProductionConfigurationReceiptMismatch",
                            row.first);
    const auto split = row.first.rfind(':');
    if (split == StableId::npos)
      return common_failure("ProductionConfigurationReceiptMismatch",
                            row.first);
    const auto actor = row.first.substr(0, split);
    const HistoryRecord *history = nullptr;
    const HistoryRecord *before = nullptr;
    const HistoryRecord *previous = nullptr;
    for (const auto &h : world.common.state().history) {
      if (h.result_id == result.result_id) {
        if (history)
          return common_failure("ProductionConfigurationReceiptMismatch",
                                row.first);
        history = &h;
        before = previous;
      }
      previous = &h;
    }
    if (!history || history->actor_id != actor ||
        history->kind != result.kind || history->tick != result.tick)
      return common_failure("ProductionConfigurationReceiptMismatch",
                            row.first);
    const auto upper =
        organization_instance_sequence(result.result_id, "result");
    const auto lower =
        before ? organization_instance_sequence(before->result_id, "result")
               : std::optional<Integer>{0};
    for (const auto &id : effects->fact_ids) {
      const auto sequence =
          organization_instance_sequence(id, "production_configuration");
      if (!upper || !lower || !sequence || *sequence <= *lower ||
          *sequence >= *upper || !used.insert(id).second)
        return common_failure(
            "ProductionConfigurationReceiptAllocationMismatch", id);
      const auto &state = *world.local_economy.production_management;
      const auto *config = state.configurations.find(id);
      const auto *resolved = state.results.find(id);
      if (config) {
        if (config->actor_id != actor || config->requested_tick != result.tick)
          return common_failure("ProductionConfigurationReceiptMismatch", id);
      } else if (!resolved || resolved->tick != result.tick ||
                 (resolved->kind != "cancelled" &&
                  resolved->kind != "superseded"))
        return common_failure("ProductionConfigurationReceiptMismatch", id);
    }
  }
  return common_success();
}

Outcome<BoundaryValue>
query_economic_production_history(const WorldBundle &world,
                                  const LocalEconomyIndex &index,
                                  const BoundaryValue &args) {
  if (!economy_available(world))
    return {false,
            {},
            common_failure("LocalEconomyUnavailable", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto query = decode_production_history_query(
      args, world.common.state().metadata.player_actor_id);
  if (!query.ok)
    return {false, {}, query.error};
  auto organizations = world.services.organization_access();
  auto page = query_production_history(world.common, world.local_economy,
                                       *world.catalog->local_economy,
                                       organizations, index, query.value);
  if (!page.ok)
    return {false, {}, page.error};
  if (world.production_disruptions_enabled())
    return encode_history_with_production_disruptions(
        world.common, *world.catalog->production_disruptions,
        world.production_disruptions, index, page.value);
  return {true, encode_production_history_page(page.value), {}};
}

} // namespace hanmo::resident
