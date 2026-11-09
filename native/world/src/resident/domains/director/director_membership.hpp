// 本人只根据获知的邀请、旧职责和欠薪提出正式命令；采用仍由中央一人一边界控制。
#pragma once
#include "director_membership_policy.hpp"
#include "director_model.hpp"
#include "resident/domains/organization/membership_offer.hpp"
namespace hanmo::resident {
// 同候选短寿命借用由中央重绑，Director不能取得可写世界或第二份知识。
struct DirectorMembershipContext {
  const SocialState &social;
  const LocalEconomyState &economy;
  const LocalEconomyCatalog &economy_catalog;
  const InteractionState &interaction;
  const ActivityRead &activity;
  const MembershipReleaseRead &release;
  // 真实完整世界租约借入原权威读钟；旧调用留空，不放宽精确成员校验。
  const OrganizationAuthorityClock *actual_clock = nullptr;
};
// 空结果把控制还给原任务/生活路径；有结果最多一条，不在此推进任务或改变成员。
Outcome<std::vector<DecisionProposal>> propose_director_membership(
    const CommonStore &, const DirectorState &, const DirectorCatalog &,
    const KnownSubjectView &, const SocialState &, const OrganizationState &,
    const OrganizationCatalog &, const LocalEconomyState &,
    const LocalEconomyCatalog &, const InteractionState &, const ActivityRead &,
    const MembershipReleaseRead &, Integer tick,
    const OrganizationAuthorityClock *actual_clock = nullptr);
// 精确比较工资率与作者底线；不使用浮点，不把公库实时余额当个人已知。
Outcome<bool> membership_wage_meets_policy(const MembershipTerms &,
                                           const ObligationRate &,
                                           const NamedMembershipPolicy &);
// 当前episode全部已知欠期无正欠额且存在本人真实付款报告时，返回该报告来源供正式复核。
std::optional<KnownArrear>
known_membership_paid_source(const OrganizationState &, const SocialState &,
                             const KnownSubjectView &, const OrgMemberState &);
} // namespace hanmo::resident
