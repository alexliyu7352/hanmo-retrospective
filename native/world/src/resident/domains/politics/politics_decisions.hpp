// 政治NPC只提出玩家同一具名命令；准入、复评、公平预算及命令采用仍归Director。
#pragma once
#include "politics_decision_model.hpp"
#include "politics_navigation.hpp"
#include "politics_views.hpp"
#include <map>
namespace hanmo::resident {
// 可重建索引只保存未决任命ID，绑定同候选身份；不入checkpoint，不成为第二份任命状态。
struct PoliticsDecisionIndex {
  WorldIdentity identity;
  std::size_t appointment_count = 0, fact_count = 0;
  std::map<StableId, std::vector<StableId>> pending_by_actor;
};
// 到任旅行是明确未完成的下一目标；没有S1正式旅行命令时不能瞬移或提前生效。
// 路由实现由S1唯一拥有；本借口仅在同一候选调用真实下一步报价。
struct PoliticalCareerProposal {
  std::optional<PoliticsCommand> command;
  std::optional<TravelNextStep> travel_step;
  std::optional<Integer> quoted_travel_cost;
  StableId reason, source_fact_id, appointment_id, destination_place_id;
  Integer inspected_candidates = 0;
  bool travel_required = false;
};
PoliticsDecisionIndex make_politics_decision_index(const CommonStore &,
                                                   const PoliticsState &);
Status validate_political_career_policy(const PoliticsState &,
                                        const PoliticsCatalog &,
                                        const OrganizationState &,
                                        const PoliticalCareerPolicy &);
Outcome<PoliticalCareerProposal> propose_political_career(
    const CommonStore &, const PoliticsState &, const PoliticsCatalog &,
    const OrganizationState &, const OrganizationCatalog &,
    const LocalEconomyState &, const SocialState &, const ActivityRead &,
    const PoliticsDecisionIndex &, const StableId &actor,
    const PoliticalCareerPolicy &, Integer candidate_budget = 16,
    const PoliticalNavigationRead *navigation = nullptr,
    const PoliticalOrganizationRuntimeReadContext *runtime = nullptr);
Outcome<PoliticalCareerPolicy>
decode_political_career_policy(const BoundaryValue &);
BoundaryValue encode_political_career_policy(const PoliticalCareerPolicy &);
} // namespace hanmo::resident
