#include "field_exit_policy.hpp"
#include "hanmo/resident/command_contract.hpp"
#include "field_author_data.hpp"
#include "field_contact_moment.hpp"
#include "field_exact_session_build.hpp"
#include "field_segment_settlement.hpp"
#include "resident/domains/politics/relations/political_relations_read.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/domains/travel/movement_exact.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/domains/military/military_model.hpp"
#include "resident/domains/military/occupation_time.hpp"
#include "resident/domains/actor_foundation/character_condition_read.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
namespace hanmo::resident {
// 有限 Field-only 分支：没有占领正文或原头时，历史控制就是原据点目录归属。
// 运行强据点字段只能核对原初始化，不能作为历史来源，也不能消去占领残留。
Outcome<StableId> read_field_initial_settlement_controller(
    const CommonStore &common, const MilitaryState &military,
    const MilitaryCatalog &catalog, const StableId &settlement) {
  auto fail = [&](const char *code) -> Outcome<StableId> {
    return {false, {}, common_failure(code, settlement).error};
  };
  if (!military.occupation_facts.empty())
    return fail("FieldSettlementOccupationOriginalOwnerRequired");
  for (const auto &row : common.state().facts.rows())
    if (row.second.kind == "military_occupation")
      return fail("FieldSettlementOccupationOriginalOwnerRequired");
  // 占领只由原fact正文生产；错放到动作/历史/缓存的同名残留也不能解锁初态。
  for (const auto &row : common.state().actions.rows())
    if (row.second.kind == "military_occupation")
      return fail("FieldSettlementOccupationOriginalOwnerRequired");
  for (const auto &row : common.state().history)
    if (row.kind == "military_occupation")
      return fail("FieldSettlementOccupationOriginalOwnerRequired");
  for (const auto &row : common.state().results.rows())
    if (row.second && row.second->kind == "military_occupation")
      return fail("FieldSettlementOccupationOriginalOwnerRequired");
  const StrongholdDefinition *source = nullptr;
  for (const auto &row : catalog.strongholds)
    if (row.settlement_id == settlement) {
      if (source || row.id.empty() || row.garrison_id.empty())
        return fail("FieldSettlementControllerSourceAmbiguous");
      source = &row;
    }
  if (!source)
    return fail("FieldSettlementControllerOriginalSourceRequired");
  const GarrisonDefinition *garrison = nullptr;
  for (const auto &row : catalog.garrisons)
    if (row.id == source->garrison_id) {
      if (garrison || row.organization_id.empty())
        return fail("FieldSettlementControllerSourceAmbiguous");
      garrison = &row;
    }
  if (!garrison)
    return fail("FieldSettlementControllerOriginalSourceRequired");
  const StrongholdRecord *runtime = nullptr;
  for (const auto &row : military.strongholds)
    if (row.id == source->id) {
      if (runtime)
        return fail("FieldSettlementControllerSourceAmbiguous");
      runtime = &row;
    }
  if (!runtime || !runtime->occupation_id.empty() ||
      runtime->owner_organization_id != garrison->organization_id)
    return fail("FieldSettlementInitialControllerMismatch");
  return {true, garrison->organization_id, {}};
}
namespace {
// 聚落只映射原据点/驻军；实际控制交原占领owner按真实接触cut反读，不从今日旗号倒填。
Outcome<StableId> read_original_settlement_controller(
    const FieldExactContactReadContext &x, const FieldBattleCatalog &catalog,
    const StableId &settlement, const SourceRef *historical_contact,
    ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<StableId> {
    return {false, {}, common_failure(code, settlement).error};
  };
  const auto *access = x.original_military_access;
  if (!access || !x.conditions.shared ||
      &x.conditions.common != &x.common || x.conditions.military != &x.military ||
      access->common_catalog != x.conditions.shared ||
      access->travel_catalog != &x.travel)
    return fail("FieldSettlementOriginalMilitaryOwnerRequired");
  Integer schema = 1;
  bool selected = false;
  for (const auto &module : catalog.identity.module_versions)
    if (module.first == "military_occupation") {
      if (selected || module.second != "2")
        return fail("FieldSettlementOccupationProfileMismatch");
      selected = true;
      schema = 2;
    }
  if (access->occupation_source_schema != schema)
    return fail("FieldSettlementOccupationProfileMismatch");
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  if (schema == 2) {
    if (access->occupation_common != &x.common || !access->occupation_clock)
      return fail("FieldSettlementOccupationOriginalClockRequired");
    auto observed = access->occupation_clock->read_current_moment();
    if (!observed.ok)
      return {false, {}, observed.error};
    auto same = compare_world_moments(observed.value, now.value, budget);
    if (!same.ok)
      return {false, {}, same.error};
    if (same.value != 0)
      return fail("FieldSettlementOccupationClockMismatch");
  }
  // 无正文也须反扫原头与其它Common容器，不能把孤占领源当作初始世界。
  if (x.military.occupation_facts.empty()) {
    auto initial = read_field_initial_settlement_controller(
        x.common, x.military, x.military_catalog, settlement);
    if (!initial.ok)
      return initial;
  } else {
    // occupation本来不是命令/动作结果，错置同名残留不能与真实receipt并存。
    for (const auto &row : x.common.state().actions.rows())
      if (row.second.kind == "military_occupation")
        return fail("FieldSettlementOccupationOriginalOwnerRequired");
    for (const auto &row : x.common.state().history)
      if (row.kind == "military_occupation")
        return fail("FieldSettlementOccupationOriginalOwnerRequired");
    for (const auto &row : x.common.state().results.rows())
      if (row.second && row.second->kind == "military_occupation")
        return fail("FieldSettlementOccupationOriginalOwnerRequired");
  }
  const StrongholdDefinition *stronghold = nullptr;
  for (const auto &row : x.military_catalog.strongholds)
    if (row.settlement_id == settlement) {
      if (stronghold || row.id.empty() || row.garrison_id.empty())
        return fail("FieldSettlementControllerSourceAmbiguous");
      stronghold = &row;
    }
  if (!stronghold)
    return fail("FieldSettlementControllerOriginalSourceRequired");
  const GarrisonDefinition *garrison = nullptr;
  for (const auto &row : x.military_catalog.garrisons)
    if (row.id == stronghold->garrison_id) {
      if (garrison || row.organization_id.empty())
        return fail("FieldSettlementControllerSourceAmbiguous");
      garrison = &row;
    }
  if (!garrison)
    return fail("FieldSettlementControllerOriginalSourceRequired");
  std::optional<SourceRefBuildView> boundary;
  WorldMoment at = now.value;
  if (historical_contact) {
    auto contact = read_field_initial_contact_actual_moment(
        x.common, x.military, x.field, catalog, *historical_contact, x, budget);
    if (!contact.ok)
      return {false, {}, contact.error};
    if (!contact.value.occurred_at)
      return fail("FieldSettlementOriginalContactMomentRequired");
    at = *contact.value.occurred_at;
    boundary = SourceRefBuildView{historical_contact->domain,
                                  historical_contact->kind,
                                  historical_contact->id};
  }
  auto owner = read_military_garrison_owner_before_actual(
      x.common, x.military, x.military_catalog, *access, garrison->id,
      boundary, at, now.value, schema, budget);
  if (!owner.ok)
    return {false, {}, owner.error};
  return {true, owner.value.organization_id(), {}};
}
Outcome<FieldQualifiedExitRead>
read_exits(const FieldExactContactReadContext &x,
           const PoliticalRelationsReadContext &relations,
           const PoliticalTakeoverTerritoryAccess *territory,
           const FieldBattleCatalog &catalog, const SourceRef &movement_contact,
           Integer side, const SourceRef *historical_contact,
           ExactArithmeticBudget budget) {
  auto fail = [&](const char *code) -> Outcome<FieldQualifiedExitRead> {
    return {false, {}, common_failure(code, movement_contact.id).error};
  };
  if (side < 0 || side > 1 || &relations.common != &x.common ||
      &relations.military != &x.military || &relations.field != &x.field ||
      &relations.formations != &x.formations ||
      &relations.actual_clock != &x.clock ||
      !x.clock.bound_to(x.common, x.military, x.field))
    return fail("CrossCandidateFieldExitPolicy");
  auto assignment =
      read_field_exact_side_assignment(x, catalog, movement_contact, budget);
  if (!assignment.ok)
    return {false, {}, assignment.error};
  auto generated =
      regenerate_field_exact_road_battle(x, movement_contact, catalog, budget);
  if (!generated.ok)
    return {false, {}, generated.error};
  const auto &field = generated.value;
  const auto &zone = field.sides[assignment.value.geometry_end_by_side[side]];
  const auto &faction = *assignment.value.faction_by_side[side];
  FieldQualifiedExitRead result;
  result.cells = zone.exit_cells;
  if (zone.road_exit_settlement_id) {
    const auto &id = *zone.road_exit_settlement_id;
    const auto town = x.travel.settlements.find(id);
    const auto kind = catalog.settlement_kinds.find(id);
    if (!town || town->id != id || town->entry_place.empty() ||
        kind == catalog.settlement_kinds.end() ||
        (kind->second != "city" && kind->second != "village"))
      return fail("FieldSettlementExitOriginalCatalogRequired");
    Outcome<StableId> controller;
    if (territory) {
      auto jurisdiction = territory->military_settlement_jurisdiction(id);
      if (!jurisdiction.ok)
        return {false, {}, jurisdiction.error};
      auto control = historical_contact
          ? territory->military_jurisdiction_control_before_initial_field_contact(
                x, catalog, *historical_contact, *jurisdiction.value)
          : territory->military_jurisdiction_control_before_field_contact_install(
                x, *jurisdiction.value);
      if (!control.ok)
        return {false, {}, control.error};
      controller = {true, control.value.controlling_organization_id, {}};
    } else {
      // 已选接管仍必须提供其原owner；不能以尚无占领事实冒充未选。
      for (const auto &module : catalog.identity.module_versions)
        if (module.first == "political_takeover")
          return fail("FieldExitTerritoryOriginalOwnerRequired");
      controller = read_original_settlement_controller(
          x, catalog, id, historical_contact, budget);
    }
    if (!controller.ok)
      return {false, {}, controller.error};
    if (controller.value.empty())
      return fail("FieldSettlementControllerRequired");
    bool allowed = controller.value == faction;
    if (!allowed) {
      auto relation =
          historical_contact
              ? read_political_relation_original_view_before_field_source(
                    relations, *historical_contact, faction,
                    controller.value)
              : read_current_political_relation_original_view(
                    relations, faction,
                    controller.value);
      if (!relation.ok)
        return {false, {}, relation.error};
      allowed = relation.value.relation != PoliticalRelation::hostile;
    }
    if (allowed) {
      const auto &p = field.parameters;
      const auto at = zone.road_exit_dm;
      if (p.cell_m <= 0 || at.x < -p.length_m * 5 || at.x >= p.length_m * 5 ||
          at.y < -p.width_m * 5 || at.y >= p.width_m * 5)
        return fail("FieldSettlementExitOriginalCellRequired");
      const auto cell =
          std::size_t((at.y + p.width_m * 5) / (p.cell_m * 10) * field.columns +
                      (at.x + p.length_m * 5) / (p.cell_m * 10));
      const auto expected = kind->second == "city" ? FieldTerrainKind::Gate
                                                   : FieldTerrainKind::Village;
      if (!zone.settlement_exit_cell || *zone.settlement_exit_cell != cell ||
          cell >= field.cells.size() || field.cells[cell].terrain != expected)
        return fail("FieldSettlementExitOriginalCellRequired");
      result.settlement_id = id;
      result.settlement_cell = cell;
      result.cells.push_back(cell);
      std::sort(result.cells.begin(), result.cells.end());
      result.cells.erase(std::unique(result.cells.begin(), result.cells.end()),
                         result.cells.end());
    }
  }
  if (result.cells.empty()) {
    auto fallback =
        read_field_boundary_exit_cells(field, zone.road_exit_dm, true);
    if (!fallback.ok)
      return {false, {}, fallback.error};
    result.cells = std::move(fallback.value);
  }
  return {true, std::move(result), {}};
}
} // namespace
Outcome<FieldQualifiedExitRead> read_field_exits_before_contact_install(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess *territory,
    const FieldBattleCatalog &catalog, const SourceRef &source, Integer side,
    ExactArithmeticBudget budget) {
  auto actual = read_field_exact_road_contact_source(
      x, source, catalog.parameters, budget);
  if (!actual.ok)
    return {false, {}, actual.error};
  auto now = x.clock.read_current_moment();
  if (!now.ok)
    return {false, {}, now.error};
  auto same = compare_world_moments(now.value,
                                    actual.value.contact->occurred_at, budget);
  if (!same.ok)
    return {false, {}, same.error};
  if (same.value != 0)
    return {
        false,
        {},
        common_failure("FieldExitPolicyInstallationMomentRequired", source.id)
            .error};
  return read_exits(x, relations, territory, catalog, source, side, nullptr,
                    budget);
}
Outcome<FieldQualifiedExitRead> read_field_exits_at_original_contact(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess *territory,
    const FieldBattleCatalog &catalog, const SourceRef &source, Integer side,
    ExactArithmeticBudget budget) {
  auto actual = read_field_initial_contact_actual_moment(
      x.common, x.military, x.field, catalog, source, x, budget);
  if (!actual.ok)
    return {false, {}, actual.error};
  return read_exits(x, relations, territory, catalog,
                    *actual.value.original_movement_contact, side, &source,
                    budget);
}
Outcome<FieldQualifiedExitRead> read_field_exits_before_contact_install(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess &territory,
    const FieldBattleCatalog &catalog, const SourceRef &source, Integer side,
    ExactArithmeticBudget budget) {
  return read_field_exits_before_contact_install(
      x, relations, &territory, catalog, source, side, budget);
}
Outcome<FieldQualifiedExitRead> read_field_exits_at_original_contact(
    const FieldExactContactReadContext &x,
    const PoliticalRelationsReadContext &relations,
    const PoliticalTakeoverTerritoryAccess &territory,
    const FieldBattleCatalog &catalog, const SourceRef &source, Integer side,
    ExactArithmeticBudget budget) {
  return read_field_exits_at_original_contact(
      x, relations, &territory, catalog, source, side, budget);
}
} // namespace hanmo::resident
