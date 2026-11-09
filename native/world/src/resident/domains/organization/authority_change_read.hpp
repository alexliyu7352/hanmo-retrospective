// K8/P11只读原变更，不重新解析今日持官者。payload只借用原OrganizationFact。
#pragma once
#include "office_lifecycle.hpp"
#include "grant_authority.hpp"
#include <functional>
namespace hanmo::resident {
struct OfficeAuthorityChangeView {const OrganizationFact* fact=nullptr;const OrganizationOfficeChangedPayload* change=nullptr;};
struct GrantAuthorityChangeView {const OrganizationFact* fact=nullptr;const GrantChangePayload* change=nullptr;OfficeTermRead target_term_at_change;};
Outcome<OfficeAuthorityChangeView> read_office_authority_change_source(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const SourceRef&,Integer schema,const ReadStamp&,const OfficeTransitionSourceRead&,const VerifiedDeathRead&,const OrganizationAuthorityClock&);
Outcome<GrantAuthorityChangeView> read_grant_authority_change_source(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const SourceRef&,Integer schema,const OrganizationAuthorityClock&);
}
namespace hanmo::resident {
// 包含变更本Grant目标，以及当次变更前实际仍有效、父链依赖该Grant的后代目标。
// 同一office只访问一次，空缺office仍以历史任期返回；不借今天current_grants推过去。
using HistoricalGrantTargetVisitor=std::function<Status(const OfficeTermRead&)>;
Status visit_descendant_grant_targets_at_change(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const SourceRef&,Integer schema,const OrganizationAuthorityClock&,const HistoricalGrantTargetVisitor&);
}
namespace hanmo::resident {
Status visit_office_dependent_grant_targets_at_change(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const SourceRef&,Integer schema,const ReadStamp&,const OfficeTransitionSourceRead&,const VerifiedDeathRead&,const OrganizationAuthorityClock&,const HistoricalGrantTargetVisitor&);
}
