// D§5/N§4.3
// 的唯一Social期间源；两表仅冻结原消息引用，不拥有第二状态或知识时间。
#pragma once
#include "social_time2_model.hpp"
namespace hanmo::resident {
inline constexpr const char *social_due_provider_id = "social_due_messages_v1";
struct SocialPeriodOpening {
  StableId period_id;
  WorldMoment opened_at;
  Integer through_tick = 0;
  std::vector<StableId> due_message_ids;
  CatalogIdentity catalog_identity;
  StableId provider_rule_id = social_due_provider_id;
  Integer provider_rule_version = 1;
};
struct SocialPeriodClosure {
  StableId period_id;
  WorldMoment closed_at;
  std::vector<StableId> delivered_message_ids;
};
struct SocialPeriodSources {
  OrderedTable<SocialPeriodOpening> openings;
  OrderedTable<SocialPeriodClosure> closures;
};
struct SocialState;
class CommonStore;
// Opening在Ready→Open冻结；due_message_receipt运行原deliver，period
// close只核已应用来源。 Source-local original opening identity; complete
// message consumption remains validate_social_due_periods, independently of
// consumers of the Calendar cut.
Outcome<const SocialPeriodOpening *>
read_social_period_opening_source(const SocialPeriodSources &,
                                  const MomentOwnerSource &,
                                  const CatalogIdentity &);
Status open_social_due_period(const CommonStore &, const SocialState &,
                              SocialPeriodSources &, const StableId &period,
                              const WorldMoment &, const CatalogIdentity &);
Status deliver_social_due_period(CommonStore &, SocialState &,
                                 SocialPeriodSources &, const StableId &period,
                                 const WorldMoment &, const CatalogIdentity &);
// 已实际完成递送的原期间借读；可在 Complete 尾屏障使用，不复制消息或知识。
Status validate_social_due_period_completion(
    const CommonStore &, const SocialState &, const SocialPeriodSources &,
    const StableId &period, const WorldMoment &, const CatalogIdentity &);
Status close_social_due_period(CommonStore &, SocialState &,
                               SocialPeriodSources &, const StableId &period,
                               const WorldMoment &, const CatalogIdentity &);
// 仅当前封闭bootstrap
// profile的宏观queued消息适用；旧当面/实际阅读不套全域closure谓词。
Status validate_social_due_periods(const CommonStore &, const SocialState &,
                                   const SocialPeriodSources &,
                                   const WorldTimelineState &,
                                   const CatalogIdentity &,
                                   Integer initial_tick);
} // namespace hanmo::resident

namespace hanmo::resident {
// The original frozen calendar period supplies this actual integer end before
// S0 publishes Calendar.
Status deliver_dynamic_reports_at_period(CommonStore &, SocialState &,
                                         const DynamicStepInput &,
                                         const WorldMoment &);
} // namespace hanmo::resident
