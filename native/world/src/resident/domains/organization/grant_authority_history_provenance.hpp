// 同原Grant全历史算法的实际Office读依赖；不另造历史fold或猜祖先。
#pragma once
#include "grant_authority.hpp"
namespace hanmo::resident {
class GrantAuthorityHistoryOfficeSink {
public:
  virtual ~GrantAuthorityHistoryOfficeSink() = default;
  // 同步只读原根/原行；不得保存短context、变更候选，后续原验证失败丢整组。
  virtual Status office_use(const CommonStore &, const OrganizationState &,
                            const OrganizationCatalog &,
                            const FactRecord &actual_change_head,
                            const OrganizationFact &actual_change,
                            const WorldMoment &actual_at,
                            const OfficeState &initial,
                            const OfficeState &current) = 0;
};
Status visit_grant_authority_history_office_uses(
    const CommonStore &, const OrganizationState &, const OrganizationCatalog &,
    const OrganizationAuthorityClock &, GrantAuthorityHistoryOfficeSink &);
} // namespace hanmo::resident
