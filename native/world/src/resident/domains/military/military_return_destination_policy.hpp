// 返营目的地的唯一原优先规则；当前位置/归属历史由调用方真实来源读口提供。
#pragma once
#include "military_lifecycle.hpp"
#include <functional>
namespace hanmo::resident {
struct CommonCatalog;
// 不授新地点、不改费用；显选友营→原preferred→原营→全部友营稳定最低原成本。
Outcome<MilitaryReturnDestination> read_military_return_destination_policy(
    const MilitaryCatalog &, const CommonCatalog *, const StableId &army_id,
    const StableId &home_garrison, const StableId &preferred_garrison,
    const StableId &selected_garrison,
    const std::function<Outcome<bool>(const StableId &)> &friendly,
    const std::function<Outcome<Integer>(const StableId &, const StableId &)> &actual_route_cost,
    bool legacy_rule_failures_are_unreachable = false);
} // namespace hanmo::resident
