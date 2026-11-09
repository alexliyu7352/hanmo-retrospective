// 原已接受责任的历史继续服务；仅两种原Grant连续性，不判断付款或另存责任状态。
#pragma once
#include "governance_supervision_scope.hpp"
#include "resident/domains/actor_foundation/character_capability.hpp"
namespace hanmo::resident {
struct PoliticsGovernanceState;
struct GovernanceWorkLifecycleSourceContext;
struct SecurityConsequence;
struct GovernanceOrganizationContinuityContext {
 const GovernanceProgramReadContext& programs;
 const OrganizationLifecycleCatalog& lifecycle_catalog;
 const OrganizationLifecycleSourceRead& lifecycle_sources;
 const OfficeTransitionSourceRead& office_sources;const VerifiedDeathRead& deaths;
 const ReadStamp& original_sources_stamp;
 const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;
};
// 与监督服务共享同一个原组织/Office重放核；前者只认当前M，后者只认原
// security writer已经写下的本program效果cut。两者均不重新授Grant或改变责任。
Outcome<GovernanceServiceContinuityView> read_governance_program_continuity_now(
 const GovernanceOrganizationContinuityContext&,const GovernanceProgramApproval&,const GovernanceProgramSpec&);
Outcome<GovernanceServiceContinuityView> read_governance_patrol_continuity_before_application(
 const GovernanceOrganizationContinuityContext&,const PoliticsGovernanceState&,
 const GovernanceProgramApproval&,const GovernanceProgramSpec&,const SecurityConsequence&);
// 再委任与实际开工分层：仅原 stop_when_vacant 的真实 Office vacancy 可待执行。
Status validate_governance_reassignment_continuity(const GovernanceOrganizationContinuityContext&,const GovernanceProgramApproval&,const GovernanceProgramSpec&,const WorldMoment&,const SourceRef* actual_cut);
struct GovernanceServiceContinuityContext {
 const GovernanceWorkHistoryContext& history;
 const OrganizationLifecycleCatalog& lifecycle_catalog;
 const OrganizationLifecycleSourceRead& lifecycle_sources;
 const OfficeTransitionSourceRead& office_sources;
 const VerifiedDeathRead& deaths;
 // 同次owning lease内真实stamp，原生命周期owner使用；跨write必须重取。
 const ReadStamp& original_sources_stamp;
 const ReadStampAuthority& stamp_authority;const DomainReadVersionSource& versions;
 const CharacterConditionSources& body;
};
class NativeGovernanceSupervisionContinuityRead final:public GovernanceSupervisionContinuityRead {
 GovernanceServiceContinuityContext context_;
 // /6 的完整 owning graph 保活此同根数据上下文；旧版本不借新源。
 const GovernanceWorkLifecycleSourceContext* lifecycle_=nullptr;
public:
 explicit NativeGovernanceSupervisionContinuityRead(GovernanceServiceContinuityContext x,const GovernanceWorkLifecycleSourceContext* lifecycle=nullptr):context_(x),lifecycle_(lifecycle){}
 Outcome<GovernanceServiceContinuityView> read_accepted_supervision_continuity_at(
  const GovernanceAcceptedWorkHistory&,const WorkEpisode&,const WorldMoment&,const SourceRef*)const override;
 Outcome<GovernanceServiceContinuityView> read_accepted_supervision_continuity_span(
  const GovernanceAcceptedWorkHistory&,const WorkEpisode&,const SupervisionActionStartSource&,const MomentSpan&)const override;
 Status validate_actual_supervision_interruption(const GovernanceAcceptedWorkHistory&,const WorkEpisode&,
  const SupervisionActionStartSource&,const MomentOwnerSource&,const WorldMoment&)const override;
};
}
