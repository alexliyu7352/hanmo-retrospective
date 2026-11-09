// PROF/OUTCOME and OE1 share these bounded read values; domain results remain at their unique owner.
#pragma once
#include "commitment_ref.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
enum class ProfessionalOutcomeKind {
 observation_completed,treatment_period_supplied,treatment_closed,medicine_batch_completed,medicine_goods_delivered,
 research_completed,work_completed,assessment_issued,assessment_corrected,copy_completed,work_delivered,lecture_session_supplied,mediation_contribution_recognized,
 elixir_batch_completed,talisman_completed,infusion_completed,recharge_completed,effect_maintenance_completed,arcane_appraisal_completed,effect_applied,ritual_completed,expedition_stage_resolved,unique_outcome_awarded
};
struct OutcomeFactRef {StableId source_domain;ProfessionalOutcomeKind source_kind=ProfessionalOutcomeKind::observation_completed;StableId fact_id;Integer version=0;};
struct ProfessionalActorTarget {StableId actor_id;};struct ProfessionalItemTarget {StableId instance_id;};struct ProfessionalSiteTarget {StableId site_id;};struct ProfessionalWorkTarget {StableId work_id;};struct ProfessionalCaseTarget {StableId case_id;};struct ProfessionalBatchTarget {StableId batch_id;};struct ProfessionalRitualTarget {StableId ritual_id;};struct ProfessionalExpeditionTarget {StableId expedition_id;};
using ProfessionalTargetRef=std::variant<ProfessionalActorTarget,ProfessionalItemTarget,ProfessionalSiteTarget,ProfessionalWorkTarget,ProfessionalCaseTarget,ProfessionalBatchTarget,ProfessionalRitualTarget,ProfessionalExpeditionTarget>;
struct ProfessionalObserverScope {SubjectRef observer;StableId registered_scope_ref;};
struct ProfessionalOutcomeRead {OutcomeFactRef outcome_ref;StableId requester_domain="tasks",objective_ref,expected_criterion_kind,expected_criterion_version;std::optional<StableId> claimed_participant;std::optional<SubjectRef> claimed_recipient;ProfessionalObserverScope observer;std::shared_ptr<const ReadStamp> source_stamp;};
struct ProfessionalMeasured {Integer value=0;StableId unit_id;};struct ProfessionalNonQuantified {SourceRef predicate_fact_ref;};
using ProfessionalMeasure=std::variant<ProfessionalMeasured,ProfessionalNonQuantified>;
enum class ProfessionalDisposition {accepted,partial,rejected};
// 原完成 Tick 保原精度；只有该 owner 确有精确结果源时才给 completed_at。
struct ProfessionalOutcomeView {OutcomeFactRef outcome_ref;SourceRef source_activity_ref;StableId source_rule_version;std::optional<TermsRevisionRef> source_terms_ref;StableId criterion_kind,criterion_version;std::optional<SubjectRef> recipient;ProfessionalTargetRef result_target;std::optional<SourceRef> participant_contribution_ref;std::optional<StableId> actual_participant;Integer completed_tick=0;ProfessionalDisposition provider_disposition=ProfessionalDisposition::rejected;ProfessionalMeasure measure;StableId settlement_key;std::vector<SourceRef> relevant_result_refs,disclosable_result_refs;std::optional<OutcomeFactRef> supersedes_ref;std::optional<WorldMoment> completed_at;};
class ProfessionalOutcomeSourceRead {public:virtual ~ProfessionalOutcomeSourceRead()=default;virtual Outcome<ProfessionalOutcomeView> read_outcome(const ProfessionalOutcomeRead&)const=0;};
Status validate_outcome_fact_ref(const OutcomeFactRef&);
Status validate_professional_outcome_view(const ProfessionalOutcomeRead&,const ProfessionalOutcomeView&);
bool same_outcome_fact_ref(const OutcomeFactRef&,const OutcomeFactRef&);
bool same_professional_target(const ProfessionalTargetRef&,const ProfessionalTargetRef&);
const char* professional_outcome_kind_name(ProfessionalOutcomeKind)noexcept;
const char* professional_outcome_owner(ProfessionalOutcomeKind)noexcept;
}
