// 原组织writer借同一候选权源和读钟；不捕获跨clone的世界指针。
#pragma once
#include "organization_runtime.hpp"
#include "membership_office_transition.hpp"
namespace hanmo::resident {
class OrganizationCalendarAdmissionRead;
struct PopulationTransportState;
struct ProductionPopulationChangeContext;
class OrganizationTrainingCoverageRead;
struct OrganizationOriginalRuntimeWriteContext {OrganizationRuntime& runtime;const OrganizationAuthorityClock& clock;const ReadStamp& stamp;const MembershipReleaseRead& releases;const OrganizationCalendarAdmissionRead* calendar=nullptr;PopulationTransportState* population=nullptr;ProductionPopulationChangeContext* production_population=nullptr;};
// Full Time2 original Calendar caller only: any failure discards the entire
// unpublished World candidate, including population and production mutations.
Outcome<OrganizationEffects> step_organization_activities(CommonStore&,OrganizationState&,const OrganizationCatalog&,LocalEconomyState&,const ActivityRead&,const DynamicResultRead&,const DynamicStepInput&,const OrganizationAuthorityClock*,const OrganizationCalendarAdmissionRead*,const OrganizationTrainingCoverageRead*,ProductionPopulationChangeContext*);
class NativeOrganizationOriginalAuthorityRead final:public AuthorityIssueSourceRead {
 const CommonStore& common_;const OrganizationState& state_;const OrganizationAuthorityClock& clock_;
public:NativeOrganizationOriginalAuthorityRead(const CommonStore& c,const OrganizationState& s,const OrganizationAuthorityClock& k):common_(c),state_(s),clock_(k){}
 Outcome<WorldMoment> read_organization_authority_now()const override;
 Outcome<AuthorityIssueSourceView> read_authority_issue_source(const SourceRef&)const override;
};
Status validate_organization_original_authorities(const CommonStore&,const OrganizationState&,const OrganizationCatalog&,const OrganizationRuntime&,const OrganizationAuthorityClock&);
}
