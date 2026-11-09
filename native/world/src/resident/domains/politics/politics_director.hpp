// 政治只填既有Director提案；公平预算、复评和最终采用由原Director唯一拥有。
#pragma once
#include "politics_decisions.hpp"
#include "resident/domains/director/director_model.hpp"
namespace hanmo::resident {
struct DirectorPoliticalContext {
  const PoliticsState &politics;
  const PoliticsCatalog &catalog;
  const LocalEconomyState &economy;
  const SocialState &social;
  const ActivityRead &activity;
  const PoliticalNavigationRead &navigation;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      military_appointment;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      field_appointment;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      supply_renewal;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      shop_registry_acceptance;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      governance_commission;
  // 原任务及明确安民签发优先，新代理仅借当次同栈报价。
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      governance_agency;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      office_resignation_response;
  std::function<Outcome<std::optional<DecisionProposal>>(
      const KnownSubjectView &, const DirectorPolicy &,
      const DecisionProposal &)>
      subordinate_appointment;
  // 完整模式由同 World 拥有体保活；只给任职提案借实际读钟、读票及资格。
  const PoliticalOrganizationRuntimeReadContext *runtime = nullptr;
};
// caller已处理旧Task与成员响应；空结果让原生活路径继续，非空仍只是一份待采用的提案。
Outcome<std::optional<DecisionProposal>> propose_director_political_career(
    const CommonStore &, const DirectorCatalog &, const KnownSubjectView &,
    const OrganizationState &, const OrganizationCatalog &,
    const DirectorPoliticalContext &, const PoliticalCareerPolicy &,
    const DecisionProposal &base);
Status validate_political_director(const CommonStore &, const PoliticsState &,
                                   const PoliticsCatalog &,
                                   const OrganizationState &,
                                   const DirectorState &,
                                   const DirectorCatalog &);
} // namespace hanmo::resident
