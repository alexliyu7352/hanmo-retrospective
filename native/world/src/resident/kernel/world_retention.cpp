// 不再遍历任何字节容量；真实共享 owner、JSON 封印与同 job 资源仍沿原入口检查。
#include "world_retention.hpp"
#include "world_bundle.hpp"
#include "catalog_copy_plan.hpp"
#include "catalog_retention.hpp"
#include "json_decoded_world_seed.hpp"
#include "resident/domains/travel/travel.hpp"
namespace hanmo::resident {
namespace {
// 工厂凭据只证明真实 owner；业务来源完整核验仍由 WorldBundle::validate 等原入口负责。
template<class T>
Status retain_original_owner(const std::shared_ptr<T>& value, CheckpointRetentionCensus& c) {
  if (!value) return common_success();
  const auto* copied = catalog_copy_detail::owner(value);
  auto owner = copied ? copied->allocator.allocation_owner() : checkpoint_shared_allocation_owner(value);
  if (!owner) return common_failure("CheckpointUntrackedStateOwner", {}, ErrorKind::internal);
  c.owner(owner);
  return common_success();
}
}
Status retain_prepared_catalog_owned_graph(const std::shared_ptr<const PreparedCatalog>& value, CheckpointRetentionCensus& c) {
  auto result=retain_original_owner(value,c);
  if (!result.ok && result.error.original.code=="CheckpointUntrackedStateOwner")
    return common_failure("CheckpointUntrackedCatalogOwner",{},ErrorKind::internal);
  if (!result.ok || !value) return result;
  // 原保留图同时负责缓存封存；这是异步只读安全，不是内存计量，必须保留。
  if (value->travel) {
    auto travel_owner = retain_original_owner(value->travel, c);
    if (!travel_owner.ok) return common_failure("CheckpointUntrackedCatalogOwner", {}, ErrorKind::internal);
    if (value->travel->route_cache) {
      auto cache_owner = retain_original_owner(value->travel->route_cache, c);
      if (!cache_owner.ok) return common_failure("CheckpointUntrackedCatalogOwner", {}, ErrorKind::internal);
      value->travel->route_cache->seal_checkpoint_retention(c);
    }
  }
  return result;
}
Status retain_initial_world_seed_graph(const InitialWorldSeed& seed, const CommonState*, CheckpointRetentionCensus& c) {
  { auto checked=retain_original_owner(seed.card_acquisitions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.profile_deliveries,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.social_period_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.actor_foundation,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.character_injury_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_relations,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_passage,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_safe_exit,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.growth_permanent,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.life_age_checks,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.production_cycles,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.world_timeline,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.action_elapsed,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.facility_claims,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.military_exact,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.military_action_time,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.field_battle,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.public_supply_inspection,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.item_instances,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.lot_properties,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.craft_jobs,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.economic_facility_capacity_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.production_facility_conditions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.governance_work_expense,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_budgets,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_treaty_terms,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.budget_execution,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.population_transport,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.household_purchases,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.household_retail_settlements,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.movement_exact,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.spatial_carriers,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.group_movement_executions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.transport_registrations,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.economic_transport_expenses,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_transport_funding,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.military_tactic_learning,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.treaty_passage,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_military_appointments,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_field_appointments,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.occupation_fiscal,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.occupation_fiscal_settlement,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.fiscal_taxpayer_continuity,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_takeover,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.economic_takeover_execution,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.control_continuation,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_life,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_legacy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_voluntary_paths,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_bequest,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_inheritance,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_marriage,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_apprenticeship,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_disciple_legacy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_biological_legacy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_collateral_legacy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family_birth,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.child_care_relief,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.estate_employment,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.economic_supply_policies,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_supply_adoptions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.growth_market_history,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.personal_recovery,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.growth_tactic_lessons,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shops,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_operation_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_operation_capabilities,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_household_custody,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_daily_rent,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_command_results,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.private_shop_liquidations,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_shop_licensing,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_shop_licensing_receipts,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_governance_continuations,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.governance_invitation_replies,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_head_handovers,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_successor_rounds,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_term_handovers,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_office_receptions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_office_resignations,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_subordinate_appointments,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_dismissal_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_governance_agencies,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_successions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_shop_registry_successions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.household_provision_management,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.production_disruptions,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.naval,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.envoy_tasks,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.diplomacy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.politics_governance,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.governance_tasks,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.politics,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.political_fiscal,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.fiscal_settlement,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.public_relief,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.travel,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.military,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.interaction,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.social,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.estate,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.tasks,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.family,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.personal,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.growth,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.population_military,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.player_plan,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.local_economy,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.economic_obligation_sources,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.governance_programs,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.organization,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.organization_runtime,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.affairs,c); if(!checked.ok)return checked; }
  { auto checked=retain_original_owner(seed.director,c); if(!checked.ok)return checked; }
  return common_success();
}
Status retain_world_checkpoint_graph(const WorldBundle& world, CheckpointRetentionCensus& c) {
  auto json=retain_json_decoded_seed_owner(world.checkpoint_json_preimage,world.common.state(),c);
  if(!json.ok)return json;
  auto catalog=retain_prepared_catalog(world.catalog,c);if(!catalog.ok)return catalog;
  c.resource(world.common.checkpoint_copy_memory().resource());
  world.common.retain_checkpoint_read_cache(c);
  // 真 read stamp owner 仍由世界持有；这里仅验证其 allocator 生命周期，不删除业务 PIN。
  world.read_stamps->retain_checkpoint_memory(c);
  world.services.retain_checkpoint_read_cache(c);
  c.resource(world.checkpoint_copy_memory.resource());
  return common_success();
}
}
