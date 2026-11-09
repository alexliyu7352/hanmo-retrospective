// 原成员真实departed后逐职辞任；不从欠薪/伤病自动清任期。
#pragma once
#include "office_lifecycle.hpp"
#include "named_membership_model.hpp"
namespace hanmo::resident {
struct MembershipOfficeTransitionWriteContext {
 OrderedTable<VerifiedOfficeTransition>& sources; // 唯一OrganizationRuntime.office_transition_sources
 const OrganizationAuthorityClock& clock;const ReadStamp& stamp;
};
class MembershipOfficeTransitionRead final:public OfficeTransitionSourceRead {
 const CommonStore& common_;const OrganizationState& state_;const OrganizationCatalog& catalog_;
 const OrderedTable<VerifiedOfficeTransition>& sources_;const OrganizationAuthorityClock& clock_;
public:
 MembershipOfficeTransitionRead(const CommonStore& c,const OrganizationState& s,const OrganizationCatalog& catalog,const OrderedTable<VerifiedOfficeTransition>& sources,const OrganizationAuthorityClock& clock):common_(c),state_(s),catalog_(catalog),sources_(sources),clock_(clock){}
 Outcome<VerifiedOfficeTransition> read_office_transition(const SourceRef&,const ReadStamp&)const override;
};
// 只在原respond已实际完成责任释放/工资尾结和departed源后调用；复核当前真实责任。
Outcome<OrganizationEffects> consume_departed_membership_offices_in_candidate(CommonStore&,OrganizationState&,const OrganizationCatalog&,const ActivityRead&,const MembershipReleaseRead&,const StableId& actual_departure_fact,const MembershipOfficeTransitionWriteContext&);
Status validate_membership_office_transitions(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const OrderedTable<VerifiedOfficeTransition>&,const OrganizationAuthorityClock&,const ReadStamp&);
}
