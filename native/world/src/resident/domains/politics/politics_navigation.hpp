// 任职行旅借S1唯一导航，不复制通道、报价、位置或行动状态。
#pragma once
#include "politics_model.hpp"
#include "resident/domains/social/social_model.hpp"
#include "resident/domains/travel/travel_navigation.hpp"
namespace hanmo::resident {
struct PoliticalNavigationRead {
  const TravelState &state;
  const TravelCatalog &catalog;
  const TravelServices &services;
};
// 玩家与NPC先通过同一任职资格/来源预览；仅NotAtSeat可以转成实际行旅下一段。
Outcome<TravelNextStep> plan_political_arrival(
    const CommonStore &, const PoliticsState &, const PoliticsCatalog &,
    const OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const SocialState &, const ActivityRead &,
    const PoliticalNavigationRead &, const StableId &actor,
    const StableId &appointment, Integer expected_revision,
    const StableId &source_fact,
    const PoliticalOrganizationRuntimeReadContext *runtime = nullptr);
} // namespace hanmo::resident
