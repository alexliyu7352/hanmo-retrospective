// Original Task proposal and acceptance for the selected Work profile.
#pragma once
#include "work_execution_contract.hpp"
#include "work_follow_through.hpp"
#include "resident/domains/interaction/face_to_face_contact_source.hpp"
#include "resident/domains/politics/governance_targets/governance_program.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
#include "resident/domains/social/social_period_sources.hpp"
namespace hanmo::resident {
struct WorkTaskSourceContext {
 const CommonStore& common;const TaskState& tasks;const GovernanceProgramReadContext& governance;
 const InteractionState& interaction;const InteractionCatalog& interaction_catalog;const InteractionAccess& interaction_access;
 const CurrentMomentRead& clock;const SocialPeriodSources& social_periods;const ExactPeriodContext* current_open_period=nullptr;
};
// 初始接受与接替接受共用真实接触和原社会期开工边界，不复制收费/Task 接受。
Status validate_work_acceptance_contact(const WorkTaskSourceContext&,const MomentOwnerSource&,const StableId& issuer,const StableId& executor,const WorldMoment&);
Outcome<WorldMoment> read_work_acceptance_earliest_service(const WorkTaskSourceContext&,const WorldMoment&);
Status validate_work_acceptance_service_window(const WorkTaskSourceContext&,const WorldMoment& accepted_at,const WorldMoment& earliest_service_at,const std::optional<StableId>& accepted_period_id);
struct WorkTaskDefinitionRead {
 const WorkTaskOfferSource* offered=nullptr;const WorkTaskAcceptanceSource* accepted=nullptr;
 const GovernanceProgramApproval* approval=nullptr;const GovernanceProgramSpec* specification=nullptr;
};
// Narrow S3 history checks for S6 callbacks. They never call S6 back or accept a
// mere ID whitelist. Complete WorkTaskRead additionally validates the real S6 sources.
Status validate_work_task_identity_source(const CommonStore&,const TaskState&,const SourceRef&program,const SourceRef&offered_task,const StableId&task_id,bool for_new_acceptance);
Status validate_work_task_execution_binding_source(const CommonStore&,const TaskState&,const SourceRef&program,const SourceRef&offered_task,const std::optional<SourceRef>&task_acceptance,const std::optional<SourceRef>&work_acceptance);
Status validate_work_task_follow_through_source(const WorkTaskSourceContext&,const WorkExecutionRead&,const GovernanceProgramApproval&,const WorkFollowThroughQuery&,const ReadStamp&,const ReadStampAuthority&,const DomainReadVersionSource&);
class WorkTaskRead {
 const WorkTaskSourceContext& context_;
public:explicit WorkTaskRead(const WorkTaskSourceContext&x):context_(x){}
 Outcome<WorkTaskDefinitionRead> read_definition(const StableId&task_id,bool accepting=false)const;
 Outcome<const WorkTaskOfferSource*> read_offer(const SourceRef&)const;
 Outcome<WorkAcceptedRead> read_accepted_work(const WorkAuthorizationRef&,bool accepting=false)const;
 bool owns_task(const StableId&)const noexcept;
 bool bound_to(const CommonStore&,const TaskState&)const noexcept;
 Outcome<WorldMoment> read_source_moment(const SourceRef&,Integer schema)const;
 Status validate_all()const;
};
// Presentation produces knowledge of this exact offered approval only for the
// named participant in the real contact. Accept is the actor's separate command.
Outcome<SourceRef> present_work_task_offer_in_candidate(CommonStore&,TaskState&,GovernanceProgramState&,const WorkTaskSourceContext&,const SourceRef&program,const StableId&executor);
Outcome<StableId> accept_work_task_in_candidate(CommonStore&,TaskState&,GovernanceProgramState&,const WorkTaskSourceContext&,const SourceRef&offer,const StableId&authenticated_actor,const WorkOriginalRead&,WorkExpenseAcceptanceWrite&);
}
