// S3 state-goal contracts extend the original Task owner, without a quantity procurement surrogate.
#pragma once
#include <string_view>
#include "resident/domains/affairs/state_need.hpp"
#include "resident/model/offer_ref.hpp"
#include "resident/model/commitment_ref.hpp"
namespace hanmo::resident {
struct TaskState;
enum class StateObjectiveCriterion {care_coverage_established,medical_condition_resolved,medical_target_reached};
struct StateNeedObjectiveBinding {StateNeedRef need_ref;StateGoalRef goal_ref;Integer goal_source_episode=0;StateObjectiveCriterion criterion=StateObjectiveCriterion::care_coverage_established;StableId required_actor_role,claim_id,disclosure_policy_ref;};
struct StateTaskMoneyTerms {SourceRef original_terms_ref;TermsRevisionRef terms_revision;StableId funding_wallet_id,reward_wallet_id,payee_wallet_id;Owner issuer;Integer maximum_service_fee=0,success_reward=0;};
struct StateNeedOffer {OfferRef offer_ref;StateNeedRef need_ref;StableId goal_summary,terms_ref;Integer claim_window=0;StateNeedDeadline completion_gate;StateTaskMoneyTerms fee_reward_terms;StableId cancel_policy;SourceRef original_disclosure_ref;SubjectRef report_recipient;bool report_required=false;StateNeedObjectiveBinding objective;};
enum class StateTaskContractPhase {active,settling,closed};
struct StateNeedTaskContract {StableId id,task_id,offer_id,acceptance_id,escrow_wallet_id;SubjectRef assignee;StableId executor_actor_id;StateNeedObjectiveBinding objective;StateTaskMoneyTerms money;StateNeedDeadline completion_gate;StableId cancel_policy;SubjectRef report_recipient;bool report_required=false;WorldMoment accepted_at;Integer claim_until_tick=0,accepted_goal_revision=0;StateTaskContractPhase phase=StateTaskContractPhase::active;std::optional<StateClaimSettlementCause> closing_cause;std::optional<StableId> contribution_id;std::optional<SourceRef> report_receipt,settlement_source,closing_source;};
struct PredicateContributionUse {StableId id,contract_id,claim_id,need_id;Integer episode=0;StateObjectiveCriterion criterion=StateObjectiveCriterion::care_coverage_established;StateSatisfactionRef satisfaction_ref;SourceRef actual_contribution_ref,applied_result_ref;};
struct StateTaskClosure {StableId id,contract_id;StateClaimSettlementCause cause=StateClaimSettlementCause::claim_released;std::optional<SourceRef> actual_service_stop_ref;WorldMoment closed_at;};
struct StateTaskSettlement {StableId id,contract_id;StateClaimSettlementCause cause=StateClaimSettlementCause::claim_released;std::vector<SourceRef> service_charge_sources;Integer paid_service_fee=0,paid_success_reward=0,refunded_service_fee=0,refunded_reward=0;WorldMoment settled_at;};
struct StateNeedTaskState {OrderedTable<StateNeedOffer> offers;OrderedTable<StateNeedTaskContract> contracts;OrderedTable<PredicateContributionUse> predicate_contributions;OrderedTable<StateTaskSettlement> settlements;OrderedTable<StateTaskClosure> closures;};
struct StateTaskContributionProof {StateNeedRef need_ref;StateGoalRef goal_ref;Integer source_episode=0;StateObjectiveCriterion criterion=StateObjectiveCriterion::care_coverage_established;StableId executor_actor_id,actor_role;SourceRef actual_contribution_ref,applied_result_ref;WorldMoment completed_at;};
struct StateTaskServiceCharges {std::vector<SourceRef> original_sources;Integer actual_fee=0;};
class StateTaskSourceRead {public:virtual ~StateTaskSourceRead()=default;
 // The selected original S3 template, disclosure, fee/reward and cancellation rules
 // must all resolve under this exact catalogue and candidate before a policy is usable.
 virtual Status validate_state_need_policy(const CommonStore&,const CatalogIdentity&,const StateNeedPolicy&)const=0;
 virtual Status validate_offer_disclosure(const StateNeedOffer&)const=0;
 virtual Status validate_frozen_terms(const StateTaskMoneyTerms&,const SubjectRef&,const StableId&executor)const=0;
 virtual Outcome<bool> executor_eligible(const StateNeedTaskContract&,const StateGoalSnapshot&)const=0;
 virtual Outcome<std::optional<StateTaskContributionProof>> care_contribution(const StateNeedTaskContract&,const CareGoalSatisfaction&)const=0;
 virtual Outcome<std::optional<StateTaskContributionProof>> medical_contribution(const StateNeedTaskContract&,const MedicalGoalSatisfaction&)const=0;
 // The actual owner stops future service before returning the applied cancellation/closure source.
 virtual Outcome<std::optional<SourceRef>> stop_future_service(const StateNeedTaskContract&,StateClaimSettlementCause)=0;
 virtual Status validate_service_stop(const StateNeedTaskContract&,StateClaimSettlementCause,const std::optional<SourceRef>&)const=0;
 virtual Outcome<StateTaskServiceCharges> supplied_service_charges(const StateNeedTaskContract&)const=0;
 virtual Status validate_report_receipt(const StateNeedTaskContract&,const SourceRef&)const=0;
};
class StateNeedTaskRead final:public StateTaskRead {
 const CommonStore&common_;const TaskState&tasks_;const StateTaskSourceRead&sources_;
public:
 StateNeedTaskRead(const CommonStore&c,const TaskState&t,const StateTaskSourceRead&s):common_(c),tasks_(t),sources_(s){}
 Outcome<StateTaskAcceptance> read_state_acceptance(const SourceRef&)const override;
 Outcome<bool> state_claim_eligible(const StateNeedClaim&,const StateGoalSnapshot&)const override;
 Outcome<bool> state_claim_contributed(const StateNeedClaim&,const StateSatisfactionRef&)const override;
 Outcome<bool> state_claim_settled(const StateNeedClaim&)const override;
};
class StateNeedTaskConsumer final:public StateTaskConsumer {CommonStore&common_;TaskState&tasks_;StateTaskSourceRead&sources_;WorldMoment now_;StateNeedTaskRead read_;public:
 StateNeedTaskConsumer(CommonStore&c,TaskState&t,StateTaskSourceRead&s,WorldMoment now):common_(c),tasks_(t),sources_(s),now_(std::move(now)),read_(c,t,s){}
 Outcome<StateTaskAcceptance> read_state_acceptance(const SourceRef&r)const override{return read_.read_state_acceptance(r);}
 Outcome<bool> state_claim_eligible(const StateNeedClaim&c,const StateGoalSnapshot&g)const override{return read_.state_claim_eligible(c,g);}
 Outcome<bool> state_claim_contributed(const StateNeedClaim&c,const StateSatisfactionRef&r)const override{return read_.state_claim_contributed(c,r);}
 Outcome<bool> state_claim_settled(const StateNeedClaim&c)const override{return read_.state_claim_settled(c);}
 Outcome<SourceRef> settle_state_claim(const StateNeedClaim&,StateClaimSettlementCause,const std::optional<StateSatisfactionRef>&)override;
};
Status publish_state_need_offer(CommonStore&,TaskState&,const StateNeedOffer&,const StateTaskSourceRead&);
Outcome<StableId> accept_state_need_offer_in_candidate(CommonStore&,TaskState&,AffairsState&,const OfferRef&,const StateClaimQuote&,const SubjectRef&,const StateGoalRead&,StateTaskSourceRead&,const WorldMoment&,const ExactPeriodContext* original_open_period);
Outcome<SourceRef> finalize_state_need_task_in_candidate(CommonStore&,TaskState&,const StableId&contract_id,const std::optional<SourceRef>&actual_report_receipt,StateTaskSourceRead&,const WorldMoment&);
Status validate_state_need_tasks(const CommonStore&,const TaskState&,const AffairsState&,const StateTaskSourceRead&);
bool is_state_need_task(const TaskState&,const StableId&task_id);
bool state_need_task_reservation_exists(const TaskState&,const StableId&contract_id);
Outcome<WorldMoment> read_state_need_task_source_moment(const CommonStore&,const TaskState&,const SourceRef&,Integer schema);
Outcome<StateNeedTaskState> decode_state_need_tasks(const BoundaryValue&);
Outcome<BoundaryValue> encode_state_need_tasks(const StateNeedTaskState&);
Status visit_state_need_tasks_checkpoint(CodecVisitor&,StateNeedTaskState&);
class ProjectionWriter;
Status emit_state_need_tasks_projection(ProjectionWriter&,std::string_view,const StateNeedTaskState&);
}
