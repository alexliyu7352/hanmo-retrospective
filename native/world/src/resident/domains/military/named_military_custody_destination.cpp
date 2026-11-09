#include "resident/domains/military/military_model.hpp"
// 两级完整查找后才有原地结果；坏目录、缺控制或缺路网都不是“没有候选”。
#include "named_military_custody_destination.hpp"
#include "resident/domains/politics/takeover/politics_takeover_territory.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/domains/travel/travel.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
#include "resident/storage/common_store.hpp"
#include <algorithm>
#include <map>
#include <set>
namespace hanmo::resident {
namespace {
using namespace movement_detail;
bool same(const WorldIdentity &a, const WorldIdentity &b) {
  return a.world_id == b.world_id && a.execution_epoch == b.execution_epoch &&
      a.revision == b.revision && a.tick == b.tick;
}
const ArmyRecord &keeper(const NamedMilitaryCustodyContext &context, const StableId &id) {
  const ArmyRecord *out = nullptr;
  for (const auto &army : context.military.armies) if (army.id == id) {
    if (out) fail("NamedMilitaryCustodyDuplicateKeeper", id);
    out = &army;
  }
  if (!out) fail("NamedMilitaryCustodyCaptorRequired", id);
  return *out;
}
// 完整目录中每个真正据点均查原映射；不同地图无法沿当前 S1 图到达，不造跨图直线。
NamedMilitaryCustodyDestination quote(
    const NamedMilitaryCustodyContext &context, const TravelCatalog &travel,
    const NamedMilitaryCustodyTerritorySources &territory,
    const NamedMilitaryCustodyTerm &term, const ArmyRecord &captor,
    const MovementAnchor &origin, Integer tick,
    const NamedMilitaryCustodyRelease *release, ExactArithmeticBudget budget) {
  const auto tree = take(read_original_road_distance_tree(travel, origin, tick, budget));
  std::map<StableId, const OriginalRoadDistance *> distances;
  for (const auto &distance : tree.reachable)
    if (!distances.emplace(distance.settlement_id, &distance).second)
      fail("NamedMilitaryCustodyDistanceRepeated", distance.settlement_id);
  // S1 原据点全集含城、乡里、渡口和营寨，不能用军事 Stronghold 子集代替。
  // 先核节点与同图来源；同图每个节点均须真实 Jurisdiction 绑定，缺源不是无候选。
  std::vector<const TravelSettlement *> towns;
  for (const auto &[id, town] : travel.settlements.rows()) {
    if (id != town.id || id.empty() || town.world_map_id.empty() ||
        !travel.maps.find(town.world_map_id))
      fail("NamedMilitaryCustodySettlementRoadSourceRequired", id);
    if (town.world_map_id == tree.world_map_id) towns.push_back(&town);
  }
  std::sort(towns.begin(), towns.end(), [](const auto *a, const auto *b) { return a->id < b->id; });
  NamedMilitaryCustodyDestination out;
  for (const auto *town : towns) {
    const auto *jurisdiction = take(territory.military_settlement_jurisdiction(town->id));
    if (!jurisdiction || jurisdiction->empty())
      fail("NamedMilitaryCustodySettlementJurisdictionRequired", town->id);
    const auto control = release
        ? take(territory.military_jurisdiction_control_before_custody_release(*release, *jurisdiction))
        : take(territory.governance_jurisdiction_control_current(*jurisdiction));
    if (!same(control.source_identity, context.common.state().identity) ||
        control.jurisdiction_id != *jurisdiction)
      fail("CrossCandidateNamedMilitaryCustodyTerritory", town->id);
    Integer priority = 0;
    if (!term.captured_organization_id.empty() &&
        control.controlling_organization_id == term.captured_organization_id) priority = 1;
    else if (control.controlling_organization_id != captor.organization_id) priority = 2;
    const auto distance = distances.find(town->id);
    if (!priority || town->world_map_id != tree.world_map_id || distance == distances.end()) continue;
    if (out.priority && (out.priority < priority ||
        (out.priority == priority && compare(out.road_distance, distance->second->distance, budget) <= 0)))
      continue;
    const auto *place = travel.places.find(town->entry_place);
    if (!place || place->settlement_id != town->id)
      fail("NamedMilitaryCustodyReleasePlaceRequired", town->id);
    out.priority = priority;
    out.settlement_id = town->id;
    out.jurisdiction_id = *jurisdiction;
    out.road_distance = distance->second->distance;
    out.road_ids = distance->second->road_ids;
    out.initial_returning = distance->second->initial_returning;
    out.destination = SettlementLocation{town->id, town->entry_place};
  }
  return out;
}
} // namespace
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination(
    const NamedMilitaryCustodyContext &context, const TravelCatalog &travel,
    const NamedMilitaryCustodyTerritorySources &territory, const NamedMilitaryCustodyTerm &term,
    const MovementAnchor &origin, ExactArithmeticBudget budget) {
  return movement_detail::guard<NamedMilitaryCustodyDestination>([&] {
    const auto *actual = take(read_original_named_military_custody_term(
        context, {"military", "military_named_custody_term", term.id}));
    if (actual != &term || !context.clock) fail("NamedMilitaryCustodyActualTermRequired", term.id);
    const auto now = take(context.clock->read_current_moment());
    const auto custody = take(read_current_named_military_custody(context, term.actor_id));
    if (!custody) fail("NamedMilitaryCustodyCurrentSourceRequired", term.actor_id);
    if (take(compare_world_moments(now, term.due_at, budget)) < 0) fail("NamedMilitaryCustodyNotDue", term.id);
    return quote(context, travel, territory, term, keeper(context, custody->captor_army_id),
                 origin, now.half_day_tick, nullptr, budget);
  });
}
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination_before_release(
    const NamedMilitaryCustodyContext &context, const TravelCatalog &travel,
    const NamedMilitaryCustodyTerritorySources &territory, const NamedMilitaryCustodyRelease &release,
    ExactArithmeticBudget budget) {
  return movement_detail::guard<NamedMilitaryCustodyDestination>([&] {
    const NamedMilitaryCustodyRelease *actual = nullptr;
    for (const auto &row : context.military.named_custody.closures) if (row.id == release.id) {
      if (actual) fail("NamedMilitaryCustodyReleaseRepeated", release.id);
      actual = &row;
    }
    const NamedMilitaryCustodyTerm *term = nullptr;
    for (const auto &row : context.military.named_custody.terms) if (row.id == release.term_id) {
      if (term) fail("NamedMilitaryCustodyTermRepeated", release.term_id);
      term = &row;
    }
    if (actual != &release || !term || term->actor_id != release.actor_id || !context.clock ||
        release.recorded_tick != release.released_at.half_day_tick ||
        take(compare_world_moments(release.released_at, term->due_at, budget)) < 0)
      fail("NamedMilitaryCustodyReleaseSourceRequired", release.id);
    const auto now = take(context.clock->read_current_moment());
    if (take(compare_world_moments(release.released_at, now, budget)) > 0)
      fail("NamedMilitaryCustodyReleaseFutureSource", release.id);
    const auto release_position = take(read_actual_common_fact_position(context.common,
        SourceRef{"military", "military_named_custody_released", release.id}));
    const auto term_position = take(read_actual_common_fact_position(context.common,
        SourceRef{"military", "military_named_custody_term", term->id}));
    if (term_position >= release_position) fail("NamedMilitaryCustodyReleaseOrderMismatch", release.id);
    const auto &captor = keeper(context, release.keeper_army_id);
    if (captor.organization_id != release.keeper_organization_id)
      fail("NamedMilitaryCustodyReleaseKeeperMismatch", release.id);
    return quote(context, travel, territory, *term, captor, release.origin_anchor,
                 release.recorded_tick, &release, budget);
  });
}
// 旧调用仍使用原 concrete 权威，不令未选分支替换已选接管历史。
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination(
    const NamedMilitaryCustodyContext &context, const TravelCatalog &travel,
    const PoliticalTakeoverTerritoryAccess &territory, const NamedMilitaryCustodyTerm &term,
    const MovementAnchor &origin, ExactArithmeticBudget budget) {
  return quote_named_military_custody_destination(context, travel,
      NamedMilitaryCustodyTerritorySources(territory), term, origin, budget);
}
Outcome<NamedMilitaryCustodyDestination> quote_named_military_custody_destination_before_release(
    const NamedMilitaryCustodyContext &context, const TravelCatalog &travel,
    const PoliticalTakeoverTerritoryAccess &territory, const NamedMilitaryCustodyRelease &release,
    ExactArithmeticBudget budget) {
  return quote_named_military_custody_destination_before_release(context, travel,
      NamedMilitaryCustodyTerritorySources(territory), release, budget);
}
} // namespace hanmo::resident
