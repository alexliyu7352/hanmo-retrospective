// P24.1 goal authority and v1c exact transition sources. A Need never supplies
// consent, physical handover, provision, or a satisfied care state.
#pragma once
#include "born_character_source.hpp"
#include "family_life_exact.hpp"
#include "resident/domains/affairs/state_need.hpp"
namespace hanmo::resident {
struct DependentCareRuleRef {ActorFoundationRuleRef rule_ref;};
using CareRequirementBasis=std::variant<BirthFactRef,DependentCareRuleRef>;
struct CareRequirement {
 StableId id,subject_actor_id;Integer revision=1,source_episode=1,created_tick=0;
 SourceRef current_transition_ref;ActorFoundationRuleRef rule_version;
 CareRequirementBasis care_basis;
 std::optional<StableId> accepted_care_terms_ref,current_care_binding_ref;
 StateGoalState state=StateGoalState::unsatisfied;
};
struct CareRequirementTransition {
 StableId transition_id,requirement_id;Integer revision_before=0,revision_after=1;
 Integer source_episode_before=0,source_episode_after=1;
 enum class Kind{requirement_created,handover_completed,coverage_lost,requirement_closed};
 Kind kind=Kind::requirement_created;
 std::optional<SourceRef> previous_transition_ref;SourceRef cause_ref;
};
struct BirthDueCareOrigin {BirthFactRef birth_ref;SourceRef scheduled_source_ref;};
struct DependentCareApplicabilityResultRef {MomentOwnerSource original_result;};
struct CareRequirementCreatedEvent {StableId requirement_id;Integer source_episode=1;std::variant<BirthDueCareOrigin,DependentCareApplicabilityResultRef> origin;};
struct CareHandoverCompletedEvent {
 StableId handover_id,requirement_id;Integer source_episode=1;StableId binding_id,satisfied_fact_id;
 MomentOwnerSource completion_work_period_ref;std::vector<MomentOwnerSource> actual_participation_refs;
 MomentOwnerSource placement_result_ref;std::vector<MomentOwnerSource> fee_result_refs;
};
struct CareHolderDeath {DeathRef death_ref;};
struct CareRequiredCapacityLost {SourceRef capacity_transition_ref;ConditionReceiptRefV2 condition_receipt_ref;ActorFoundationRuleRef purpose_ref;};
struct CareActualSeparation {MomentOwnerSource actual_location_or_binding_transition;};
struct CareAcceptedWindowEnded {StableId accepted_care_terms_ref;Integer until_tick=0;};
struct CareParticipationConflict {MomentOwnerSource actual_occupation_change;ActorFoundationRuleRef compatibility_rule_ref;};
struct CareAuthorityOrArrangementEnded {MomentOwnerSource actual_ended_result;};
using CareLossCauseV2=std::variant<CareHolderDeath,CareRequiredCapacityLost,CareActualSeparation,CareAcceptedWindowEnded,CareParticipationConflict,CareAuthorityOrArrangementEnded>;
struct CareCoverageLostEvent {StableId requirement_id;Integer prior_source_episode=1,next_source_episode=2;StableId old_binding_ref,lost_fact_id;CareLossCauseV2 cause;};
struct CareSubjectDeath {DeathRef death_ref;};
struct CareNoLongerDependent {MomentOwnerSource actual_applicability_result;};
struct CareLawfulGoalWithdrawal {MomentOwnerSource actual_withdrawal;};
using CareGoalClosureCauseV2=std::variant<CareSubjectDeath,CareNoLongerDependent,CareLawfulGoalWithdrawal>;
struct CareRequirementClosedEvent {StableId requirement_id;Integer source_episode=1;StateGoalState final_state=StateGoalState::invalid;CareGoalClosureCauseV2 cause;};
using CareTemporalEventBody=std::variant<CareRequirementCreatedEvent,CareHandoverCompletedEvent,CareCoverageLostEvent,CareRequirementClosedEvent>;
struct CareTemporalEvent {StableId event_id;SourceRef original_transition_ref;WorldMoment occurred_at;ActorFoundationRuleRef rule_ref;CareTemporalEventBody body;};
// No second owning model: these views borrow the three corresponding roots
// from the complete Care owner. Physical root ordering is selected separately.
struct CareGoalReadTables {const OrderedTable<CareRequirement>& requirements;const OrderedTable<CareRequirementTransition>& transitions;const OrderedTable<CareTemporalEvent>& temporal_events;};
struct CareGoalWriteTables {OrderedTable<CareRequirement>& requirements;OrderedTable<CareRequirementTransition>& transitions;OrderedTable<CareTemporalEvent>& temporal_events;CareGoalReadTables read()const{return {requirements,transitions,temporal_events};}};
struct CareRequirementRule {
 ActorFoundationRuleRef rule_ref;StableId service_kind;
 ActorFoundationRuleRef dependency_rule_ref,cancellation_rule_ref,disclosure_rule_ref;
 StableId state_need_policy_ref;
};
class CareRequirementRuleRead {public:virtual ~CareRequirementRuleRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Integer selected_care_version()const noexcept=0;
 virtual Outcome<const CareRequirementRule*> read_birth_care_rule(const BornCharacterSourceView&)const=0;
 virtual Outcome<const CareRequirementRule*> read_care_requirement_rule(const ActorFoundationRuleRef&)const=0;
};
struct CareCoverageSources;
struct CareGoalSourceContext {
 const CommonStore& common;CareGoalReadTables goals;const BornCharacterSourceRead& births;
 const CareRequirementRuleRead& rules;const FamilyDeathState& deaths;
 const FamilyLifeCauseSources4& life;const CurrentMomentRead& clock;
 const CareCoverageSources* coverage=nullptr;
};
// Full original goal chain. Handover/coverage use the same owner's borrowed
// coverage sources; a missing actual placement/participation provider rejects.
Status validate_care_requirement_goal_sources(const CareGoalSourceContext&);
Outcome<Integer> care_goal_transition_source_schema(const SourceRef&);
Outcome<const CareTemporalEvent*> read_care_goal_transition_source(const CareGoalSourceContext&,const SourceRef&);
Outcome<const CareRequirement*> read_birth_care_requirement(const CareGoalSourceContext&,const BirthFactRef&);
struct CareRequirementApplication {StableId requirement_id;SourceRef transition_ref;bool already_applied=false;};
Outcome<CareRequirementApplication> create_birth_care_requirement_in_candidate(CommonStore&,CareGoalWriteTables,const CareGoalSourceContext&,const BirthFactRef&);
Outcome<CareRequirementApplication> close_care_requirement_after_subject_death_in_candidate(CommonStore&,CareGoalWriteTables,const CareGoalSourceContext&,const StableId& requirement_id,const DeathRef&);
// Pure selected goal projection. The complete binder combines this with the
// separately owned MedicalGoal reader; it never invents an empty medical value.
Outcome<StateGoalSnapshot> read_care_requirement_goal(const CareGoalSourceContext&,const CareCoverageGoal&);
// 原消费者 Common 头前的目标链。只借最后已发生事件，不用今日状态，也不
// 递归验证该消费者之后才形成的交接/同意；发布仍须完整双向覆盖。
struct CareGoalBeforeFact {Integer revision=0,source_episode=0;StateGoalState state=StateGoalState::unsatisfied;const CareTemporalEvent* last_transition=nullptr;};
Outcome<CareGoalBeforeFact> read_care_requirement_before_original_fact(
 const CareGoalSourceContext&,const StableId& requirement_id,const SourceRef& actual_consumer);
// Original locked rule and its StateNeed policy reference. This source reader
// never calls Affairs, Need reconciliation, claims, or Task validation.
Outcome<const CareRequirementRule*> read_care_requirement_rule_for_goal(
 const CareGoalSourceContext&,const CareCoverageGoal&);
struct BirthCareNeedApplication {CareRequirementApplication requirement;StateNeedRef need;};
// Shared post-source coordinator. A new actual transition must be current; a
// verified replay returns the original retained Need for its source_episode.
// No replay writes today's goal or manufactures a missing historical Need.
Outcome<BirthCareNeedApplication> reconcile_applied_care_goal_in_candidate(
 CommonStore&,CareGoalWriteTables,const CareGoalSourceContext&,CareRequirementApplication,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
// Called by the actual BirthDue stage after original Birth4 creation, using the
// same selected S0/S3 lease. A missing claimant is a normal open Need, whereas
// a missing compiled source service is a pre-install capability failure.
Outcome<BirthCareNeedApplication> create_birth_care_requirement_and_need_in_candidate(
 CommonStore&,CareGoalWriteTables,const CareGoalSourceContext&,const BirthFactRef&,
 AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
// The real subject Death4 writer uses the same coordination path immediately;
// it cannot leave an old open Need/claim for a later periodic repair.
Outcome<BirthCareNeedApplication> close_care_requirement_and_need_after_subject_death_in_candidate(
 CommonStore&,CareGoalWriteTables,const CareGoalSourceContext&,const StableId& requirement_id,
 const DeathRef&,AffairsState&,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&);
class SelectedCareGoalVisitor {public:virtual ~SelectedCareGoalVisitor()=default;
 virtual Status selected_care_goal(const StableId& requirement_id,const StableId& subject_actor_id)=0;
};
// Borrow original IDs in canonical requirement order. All actual source chains
// and reverse coverage are verified before the first callback.
Status visit_selected_care_goals(const CareGoalSourceContext&,SelectedCareGoalVisitor&);
}
