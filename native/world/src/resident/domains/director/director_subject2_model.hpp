// DIRECTOR-SUBJECT-2 extends the original finite scheduler; business state remains at its owner.
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
enum class DirectorLifecycle;
struct ActorDirectorAdmission {StableId actor_id,admission_id;};
struct CohortDirectorAdmission {SourceRef accepted_household_policy_ref;Integer source_revision=0;};
struct OrganizationDirectorAdmission {SourceRef approved_responsibility_ref;Integer source_revision=0;};
using DirectorAdmissionSource2=std::variant<ActorDirectorAdmission,CohortDirectorAdmission,OrganizationDirectorAdmission>;
struct DirectorRegistration2 {SubjectRef subject;StableId policy_id,admission_id,reason;Integer epoch=1,entered_tick=0,changed_tick=0;DirectorLifecycle lifecycle=static_cast<DirectorLifecycle>(0);bool obligations_only=false;DirectorAdmissionSource2 admission_source;};
struct ActorExecutionBinding {SourceRef original_business_ref;std::optional<SourceRef> execution_ref;};
struct CohortPurchaseBinding {SourceRef purchase_plan_ref;std::optional<SourceRef> execution_ref;};
struct CohortHouseholdRoutineBinding {SourceRef need_source_ref;std::optional<SourceRef> execution_ref;};
struct InstitutionalExecutionBinding {SourceRef approved_responsibility_ref;std::optional<SourceRef> execution_ref;};
struct AwaitOfficerBinding {SourceRef approved_goal_ref;};
using SubjectExecutionBinding=std::variant<ActorExecutionBinding,CohortPurchaseBinding,CohortHouseholdRoutineBinding,InstitutionalExecutionBinding,AwaitOfficerBinding>;
struct SubjectPlanBinding {StableId plan_id;SubjectRef subject;Integer registration_epoch=0;SubjectExecutionBinding binding;SourceRef policy_source_ref;Integer last_source_revision=0;std::optional<SourceRef> continuation_ref;};
struct SubjectContinuation {SubjectRef prior_subject,current_subject;SourceRef original_continuation_ref;Integer old_registration_epoch=0,new_registration_epoch=0;std::vector<StableId> plan_ids;};
enum class DirectorAdoptionStatus2 {adopted=0,rejected=1,waiting=2};
// Physical director/6 only. The identity is supplied by the actual Common
// command commit, never allocated or inferred by the decision scheduler.
struct DirectorAcceptedCommand {
 // 普通文字/目录向量先预留后fresh复制，实际记录或其副本销毁后才退还。
 CheckpointMemoryCharge copy_memory;
 DirectorAcceptedCommand()=default;
 DirectorAcceptedCommand(const StableId&,const StableId&,Integer,const StableId&,const StableId&,const CatalogIdentity&,const WorldMoment&,const MomentOwnerSource&);
 DirectorAcceptedCommand(const DirectorAcceptedCommand&);
 DirectorAcceptedCommand(DirectorAcceptedCommand&&);
 DirectorAcceptedCommand& operator=(const DirectorAcceptedCommand&);
 DirectorAcceptedCommand& operator=(DirectorAcceptedCommand&&)noexcept;
 void swap(DirectorAcceptedCommand&)noexcept;
 StableId actor_id,command_kind;Integer request_seq=0;StableId result_id,policy_id;
 CatalogIdentity policy_catalog;WorldMoment accepted_at;MomentOwnerSource actual_effect;
};
struct DirectorAdoptionResult2 {SubjectRef subject;Integer registration_epoch=0,decision_seq=0;std::optional<StableId> plan_id;Integer typed_proposal_kind=0;SourceRef source_policy_ref;DirectorAdoptionStatus2 status=DirectorAdoptionStatus2::waiting;std::vector<SourceRef> actual_outcome_refs;Integer reviewed_tick=0,next_review_tick=0;std::optional<DirectorAcceptedCommand> accepted_command;};
enum class SubjectWakeKind2 {known_need,known_message,commitment,authority,capability};
struct SubjectWake2 {SubjectRef subject;SubjectWakeKind2 kind=SubjectWakeKind2::known_need;SourceRef actual_source;Integer source_schema=1,source_revision=0;};
struct DirectorWakeRecord2 {SubjectWake2 wake;WorldMoment observed_at;std::optional<Integer> consumed_decision_seq;};
struct DirectorSubjects2State {
 // Selected physical layout, not an implicit migration or a second runtime root.
 Integer command_binding_version=0;
 // Derived cache only. Wire state is the original ReviewSchedule.wake_reasons.
 // Restore rebuilds from actual owner sources before any review can execute.
 OrderedTable<DirectorWakeRecord2> wake_sources;bool wake_cache_ready=false;
 OrderedTable<DirectorRegistration2> registrations;
 // Original plan IDs key both live and retained completed bindings.
 OrderedTable<SubjectPlanBinding> subject_plan_bindings;
 OrderedTable<SubjectContinuation> subject_continuations;
 // Original (subject, epoch, decision_seq) durable key, never a request cache.
 OrderedTable<DirectorAdoptionResult2> adoption_results;
};
}
