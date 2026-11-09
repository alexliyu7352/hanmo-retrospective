#include "military_return_destination_policy.hpp"
#include "military_model.hpp"
#include "resident/model/common_model.hpp"
#include <algorithm>
namespace hanmo::resident {
// 现有策略的一处实现供旧当前与Field历史同用；rule无路继续原候选，internal错误不吞。
Outcome<MilitaryReturnDestination> read_military_return_destination_policy(
    const MilitaryCatalog &catalog, const CommonCatalog *common,
    const StableId &army, const StableId &home, const StableId &preferred_input,
    const StableId &selected,
    const std::function<Outcome<bool>(const StableId &)> &friendly,
    const std::function<Outcome<Integer>(const StableId &, const StableId &)> &route_cost, bool legacy_rule_failures_are_unreachable) {
  auto fail = [&](const char *code, const StableId &path = StableId{}) -> Outcome<MilitaryReturnDestination> {
    return {false, {}, common_failure(code, path).error};
  };
  if (!friendly || !route_cost) return fail("MilitaryReturnOriginalPolicyReadersRequired", army);
  if (!selected.empty()) {
    auto allowed = friendly(selected);
    if (!allowed.ok) return {false, {}, allowed.error};
    if (!allowed.value) return fail("CampaignFriendlyGarrisonRequired");
  }
  StableId preferred = selected.empty() ? preferred_input : selected;
  auto preferred_friendly = preferred.empty() ? Outcome<bool>{true, false, {}} : friendly(preferred);
  if (!preferred_friendly.ok) return {false, {}, preferred_friendly.error};
  if (preferred.empty() || !preferred_friendly.value) {
    auto original = friendly(home);
    if (!original.ok) return {false, {}, original.error};
    preferred = original.value ? home : StableId{};
  }
  std::vector<StableId> candidates;
  if (!preferred.empty()) candidates = {preferred};
  else {
    for (const auto &g : catalog.garrisons) candidates.push_back(g.id);
    std::sort(candidates.begin(), candidates.end());
  }
  std::optional<MilitaryReturnDestination> best;
  bool any = false;
  for (const auto &id : candidates) {
    auto allowed = friendly(id);
    if (!allowed.ok) return {false, {}, allowed.error};
    if (!allowed.value) continue;
    any = true;
    const GarrisonDefinition *garrison = nullptr;
    for (const auto &g : catalog.garrisons) if (g.id == id) {
      if (garrison) return fail("DuplicateMilitaryReturnGarrison", id);
      garrison = &g;
    }
    if (!garrison) return fail("CampaignFriendlyGarrisonRequired", id);
    const auto *place = common ? common->places.find(garrison->place_id) : nullptr;
    if (!place) return fail("UnknownPlace", garrison->place_id);
    auto cost = route_cost(id, place->settlement_id);
    if (!cost.ok) {
      const auto &code = cost.error.original.code;
      const bool unreachable = cost.error.kind == ErrorKind::rule &&
          (legacy_rule_failures_are_unreachable || code == "NoRoute" ||
           code == "InvalidOrigin" || code == "InvalidDestination" || code == "SegmentBlocked");
      if (!unreachable) return {false, {}, cost.error};
      continue;
    }
    if (!best || cost.value < best->cost)
      best = MilitaryReturnDestination{army, place->settlement_id, id, cost.value};
  }
  if (!best) {
    auto result = fail(any ? "CampaignNoFriendlyRoute" : "CampaignNoFriendlyGarrison");
    result.value.return_garrison_id = any ? preferred : StableId{};
    return result;
  }
  return {true, *best, {}};
}
} // namespace hanmo::resident
