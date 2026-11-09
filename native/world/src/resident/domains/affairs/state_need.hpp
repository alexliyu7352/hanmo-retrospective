// STATE-NEED-1 is coordination of two owner-defined state goals, never an amount of health or care.
#pragma once
#include <string_view>
#include "resident/model/dynamic_contract.hpp"
#include "resident/kernel/world_timeline.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct CodecVisitor;struct AffairsState;struct ExactPeriodContext;
struct QuantityNeedRef { StableId need_id; };
struct StateNeedRef { StableId need_id;Integer episode=0; };
using CoordinatedNeedRef=std::variant<QuantityNeedRef,StateNeedRef>;
struct CareCoverageGoal {StableId care_requirement_id,subject_actor_id;};
struct MedicalResolutionGoal {StableId medical_need_id,patient_actor_id;};
using StateGoalRef=std::variant<CareCoverageGoal,MedicalResolutionGoal>;
struct CareGoalSatisfaction {SourceRef source;Integer source_schema=1;};
struct MedicalGoalSatisfaction {SourceRef source;Integer source_schema=1;};
using StateSatisfactionRef=std::variant<CareGoalSatisfaction,MedicalGoalSatisfaction>;
enum class StateGoalState {unsatisfied,satisfied,withdrawn,invalid};
struct StateGoalSnapshot {
 StateGoalRef goal_ref;Integer source_revision=0,source_episode=0;
 StateGoalState state=StateGoalState::unsatisfied;Integer checked_tick=0;
 SourceRef source_ref;ScopeRef claim_scope_ref;
 std::optional<StateSatisfactionRef> satisfaction_ref;std::optional<WorldMoment> satisfaction_at;std::optional<StableId> invalid_reason;
};
class StateGoalRead {public:virtual ~StateGoalRead()=default;
 virtual Outcome<StateGoalSnapshot> read_care_goal(const CareCoverageGoal&)const=0;
 virtual Outcome<StateGoalSnapshot> read_medical_goal(const MedicalResolutionGoal&)const=0;
};
struct StateNeedDeadline {Integer tick=0;bool inclusive=true;};
enum class StateNeedPhase {open,claimed,satisfied,withdrawn,invalid,expired};
enum class StateClaimPhase {accepted,performing,settling,closed};
struct StateNeed {
 StableId need_id,affair_id;StateGoalRef goal_ref;Integer episode=1,source_episode=0,source_revision=0,opened_tick=0;
 std::optional<StateNeedDeadline> deadline;StateNeedPhase phase=StateNeedPhase::open;
 std::optional<StableId> active_claim_id;std::optional<SourceRef> closing_source_ref;
 SourceRef last_reconcile_source_ref;std::optional<StateNeedRef> previous_episode_ref;
 // Derived from deadline_closures; not a second serialized deadline source.
 std::optional<Integer> deadline_closed_at_tick;
};
struct StateNeedClaim {
 StableId claim_id;StateNeedRef need_ref;SubjectRef assignee;StableId executor_actor_id,task_contract_id;
 Integer accepted_source_revision=0,accepted_tick=0,claim_until_tick=0;
 StateClaimPhase phase=StateClaimPhase::accepted;std::optional<StableId> release_reason;
 SourceRef acceptance_ref;std::optional<Integer> deadline_closed_at_tick;
};
struct StateNeedDeadlineGateRef {StateNeedRef need_ref;};
struct StateClaimDeadlineGateRef {StableId claim_id;};
using StateDeadlineGateRef=std::variant<StateNeedDeadlineGateRef,StateClaimDeadlineGateRef>;
struct StateNeedDeadlineClosure {
 StableId need_id;Integer episode=0;std::optional<StableId> claim_id;
 StateDeadlineGateRef deadline_gate_ref;Integer deadline_closed_at_tick=0;
 SourceRef closing_source_ref;FixedLockedRuleInput rule_ref;
};
struct StateNeedOpeningClaim {StableId claim_id,task_contract_id;SourceRef acceptance_ref;};
// The outer real period opening owns this frozen value. No late current-table scan
// may substitute an accepted claim or bound Task absent from this opening.
struct StateNeedDeadlineOpening {StateNeedRef need_ref;std::optional<StateNeedOpeningClaim> claim;FixedLockedRuleInput rule_ref;};
struct StateNeedPeriodOpening {StableId period_id;WorldMoment opened_at;Integer through_tick=0;std::vector<StateNeedDeadlineOpening> opening_needs;};
struct StateNeedState {
 OrderedTable<StateNeed> needs;OrderedTable<StateNeedClaim> claims;
 OrderedTable<StateNeedDeadlineClosure> deadline_closures;
 bool exact_period_openings=false;OrderedTable<StateNeedPeriodOpening> period_openings;
 // Rebuilt hot indexes, omitted by state_needs/1.
 OrderedTable<StateNeedRef> current_episodes;
};
struct KnownStateGoal {StableId summary;SourceRef knowledge_source;Integer source_revision=0;};
class StateNeedKnowledgeRead {public:virtual ~StateNeedKnowledgeRead()=default;virtual Outcome<std::optional<KnownStateGoal>> known_state_goal(const SubjectRef&,const StateNeedRef&)const=0;};
struct StateClaimQuote {
 StateNeedRef need_ref;Integer goal_revision=0;StableId executor,terms_ref;Integer claim_until_tick=0;
 StableId known_goal_summary;std::vector<ReadVersion> resource_quotes;
};
struct StateNeedPolicy {
 StableId id,owner_rule,task_template,disclosure,fee_reward_terms,cancellation_rule;
 Integer goal_kind=0,claim_window=0;std::optional<StateNeedDeadline> deadline;
};
class StateNeedPolicyRead {public:virtual ~StateNeedPolicyRead()=default;
 virtual const CatalogIdentity& state_need_catalog_identity()const=0;
 virtual Outcome<const StateNeedPolicy*> state_need_policy(const StateGoalRef&)const=0;
};
struct StateTaskAcceptance {
 SourceRef acceptance_ref;StateNeedRef need_ref;StableId contract_id,executor_actor_id,terms_ref;
 SubjectRef assignee;Integer goal_revision=0,accepted_tick=0,claim_until_tick=0;
};
enum class StateClaimSettlementCause {fulfilled,resolved_elsewhere,withdrawn,invalid,claim_released,claim_expired,need_expired};
// S3 is still the only commitment, evidence, fee/reward and cancellation writer.
// The coordinator requires its applied source; it never manufactures a receipt or pays a reward.
class StateTaskRead {public:virtual ~StateTaskRead()=default;
 virtual Outcome<StateTaskAcceptance> read_state_acceptance(const SourceRef&)const=0;
 virtual Outcome<bool> state_claim_eligible(const StateNeedClaim&,const StateGoalSnapshot&)const=0;
 virtual Outcome<bool> state_claim_contributed(const StateNeedClaim&,const StateSatisfactionRef&)const=0;
 virtual Outcome<bool> state_claim_settled(const StateNeedClaim&)const=0;
};
class StateTaskConsumer:public StateTaskRead {public:
 virtual Outcome<SourceRef> settle_state_claim(const StateNeedClaim&,StateClaimSettlementCause,const std::optional<StateSatisfactionRef>&)=0;
};
StableId state_goal_key(const StateGoalRef&);
Outcome<StateGoalSnapshot> read_state_goal(const StateGoalRead&,const StateGoalRef&);
Outcome<StateNeedRef> reconcile_state_need(CommonStore&,AffairsState&,const StateGoalRef&,const StateNeedPolicy&,const StateGoalRead&,StateTaskConsumer&,const WorldMoment& actual);
Outcome<StateClaimQuote> prepare_state_need_claim(const CommonStore&,const StateNeedState&,const StateNeedRef&,Integer expected_revision,const SubjectRef&,const StableId& executor,const StableId& terms_ref,const StateNeedPolicy&,const StateGoalRead&,const OrganizationRead*,const ActingAuthority*,const StateNeedKnowledgeRead&,const WorldMoment& actual,const ExactPeriodContext* original_open_period);
Outcome<StableId> establish_state_need_claim(CommonStore&,StateNeedState&,const StateClaimQuote&,const SourceRef& actual_acceptance,const StateGoalRead&,StateTaskConsumer&,const WorldMoment& actual,const ExactPeriodContext* original_open_period);
Status release_state_need_claim(StateNeedState&,const StableId&,const StateGoalSnapshot&,StateClaimSettlementCause,StateTaskConsumer&);
Status close_state_need_deadlines(CommonStore&,AffairsState&,const std::vector<StateNeedDeadlineOpening>& opening_needs,const StateNeedPolicyRead&,const StateGoalRead&,StateTaskConsumer&,const WorldMoment& actual);
StableId state_need_deadline_closure_key(const StateDeadlineGateRef&);
Status rebuild_state_need_indexes(StateNeedState&);
Outcome<WorldMoment> read_state_need_deadline_source(const StateNeedState&,const SourceRef&,Integer schema);
Status validate_state_needs(const CommonStore&,const AffairsState&,const StateGoalRead&,const StateTaskRead&,const WorldMoment& actual,const StateNeedPolicyRead&);
// Historical replay returns the unique retained episode, never today's index.
// The original Care/Medical source owner supplies the exact source_episode.
Outcome<const StateNeed*> read_state_need_for_source_episode(const CommonStore&,const AffairsState&,const StateGoalRef&,Integer source_episode,const StateGoalRead&,const StateTaskRead&,const WorldMoment& actual,const StateNeedPolicyRead&);
Status reconcile_state_affair_closure(CommonStore&,AffairsState&,const StateTaskRead&,const WorldMoment& actual);
std::vector<SourceRef> state_need_source_pins(const StateNeedState&);
Outcome<StateNeedState> decode_state_needs(const BoundaryValue&,Integer physical_version=1);
Outcome<BoundaryValue> encode_state_needs(const StateNeedState&);
Status visit_state_needs_checkpoint(CodecVisitor&,StateNeedState&,Integer physical_version=1);
class ProjectionWriter;
Status emit_state_needs_projection(ProjectionWriter&,std::string_view,const StateNeedState&);
}
