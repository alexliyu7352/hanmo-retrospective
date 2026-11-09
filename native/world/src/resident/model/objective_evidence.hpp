// OE1 references preserve each original owner's schema, rule and criterion separately.
#pragma once
#include "professional_outcome_read.hpp"
#include "resident/domains/tasks/work_execution/work_result_source.hpp"
namespace hanmo::resident {
enum class LegacyTaskEvidenceKind {delivery,transfer,escort_arrival,protected_battle,message_source};
struct LegacyTaskEvidenceRef {LegacyTaskEvidenceKind kind=LegacyTaskEvidenceKind::delivery;StableId id;};
struct ObjectiveReceiptRef {StableId source_domain,receipt_kind,fact_id;std::optional<StableId> activity_id,contribution_use_id;StableId rule_version;};
struct ProfessionalOutcomeSource {OutcomeFactRef ref;};
// I §18 的 Work 来源保留原 typed 联合；不将劳动成果伪装成专业交货回执。
struct WorkEvidenceSource {WorkResultSource source;};
using ObjectiveBaseEvidenceSource=std::variant<LegacyTaskEvidenceRef,ObjectiveReceiptRef,ProfessionalOutcomeSource,WorkEvidenceSource>;
struct LegacyGoalCriterion {StableId goal_kind,rule_version;};
struct CraftCriterion {StableId acceptance_kind;Integer acceptance_version=0;};
struct ProfessionalCriterion {StableId criterion_kind,criterion_version;};
struct WorkCriterion {StableId criterion_version;WorkAuthorizationRef authorization;WorkKind work_kind=WorkKind::patrol;};
using ObjectiveBaseCriterion=std::variant<LegacyGoalCriterion,CraftCriterion,ProfessionalCriterion,WorkCriterion>;
struct ObjectiveMeasured {Integer amount=0;StableId unit_id;};
struct ObjectivePredicate {SourceRef predicate_source;};
using ObjectiveAcceptedMeasure=std::variant<ObjectiveMeasured,ObjectivePredicate>;
bool same_objective_base_source(const ObjectiveBaseEvidenceSource&,const ObjectiveBaseEvidenceSource&);
bool same_objective_base_criterion(const ObjectiveBaseCriterion&,const ObjectiveBaseCriterion&);
Status validate_objective_base_source(const ObjectiveBaseEvidenceSource&,bool permit_contribution_use=false);
}
