#pragma once
#include "material_aid.hpp"
#include <algorithm>
#include <limits>
namespace hanmo::resident::aid_detail {
struct Failure {RuleError error;};
[[noreturn]]inline void fail(const char*code,const StableId&id={},ErrorKind kind=ErrorKind::internal){throw Failure{common_failure(code,id,kind).error};}
template<class T>T take(Outcome<T>r){if(!r.ok)throw Failure{std::move(r.error)};return std::move(r.value);}
inline void take(Status r){if(!r.ok)throw Failure{std::move(r.error)};}
inline int compare(const WorldMoment&a,const WorldMoment&b){return take(compare_world_moments(a,b));}
inline bool same(const MomentOwnerSource&a,const MomentOwnerSource&b){return a.source_schema==b.source_schema&&same_source_ref(a.source_ref,b.source_ref);}
inline bool same(const AidSourceLegRef&a,const AidSourceLegRef&b){return a.leg_ordinal==b.leg_ordinal&&same(a.source,b.source);}
inline bool same(const SubjectRef&a,const SubjectRef&b){return a.kind==b.kind&&a.id==b.id;}
inline bool same(const BudgetUnit&a,const BudgetUnit&b){if(a.index()!=b.index())return false;const auto*i=std::get_if<BudgetItemQuantity>(&a);return !i||i->item_id==std::get<BudgetItemQuantity>(b).item_id;}
inline bool same(const BudgetPeriod&a,const BudgetPeriod&b){return a.period_from_tick==b.period_from_tick&&a.period_until_tick==b.period_until_tick;}
inline bool same(const BudgetExecutionRef&a,const BudgetExecutionRef&b){return a.budget_id==b.budget_id&&a.budget_revision==b.budget_revision&&a.policy_revision==b.policy_revision&&a.approval_fact_id==b.approval_fact_id;}
inline Integer add(Integer a,Integer b){if(a<0||b<0||a>std::numeric_limits<Integer>::max()-b)fail("MaterialAidQuantityOverflow");return a+b;}
inline Integer multiply(Integer a,Integer b){if(a<0||b<0||(b&&a>std::numeric_limits<Integer>::max()/b))fail("MaterialAidQuantityOverflow");return a*b;}
WorldMoment now(const MaterialAidReadContext&);
std::size_t header(const CommonStore&,const SourceRef&);
void before(const CommonStore&,const SourceRef&,const SourceRef&);
void context(CommonStore&,TreatyMaterialAidState&,const MaterialAidReadContext&);
const TreatyAidSchedule& schedule(const MaterialAidReadContext&,const StableId&);
ActivatedTreatyAidTermsRead terms(const MaterialAidReadContext&,const TreatyAidSchedule&);
std::vector<TreatyAidItemAmount> normalized_items(const TreatyMaterialAidTerm&);
std::vector<MaterialAidRemaining> original_measures(const TreatyMaterialAidTerm&);
AidOccurrenceKey key(const TreatyAidSchedule&,Integer);
const AidScheduleCommitment& commitment(const MaterialAidReadContext&,const StableId&);
const AidScheduleCommitment* commitment_for_schedule(const MaterialAidReadContext&,const StableId&);
MaterialAidAcceptedResources accepted_resources(const MaterialAidReadContext&,const AidScheduleCommitment&);
void validate_commitment_identity(const MaterialAidReadContext&,const AidScheduleCommitment&);
void validate_commitment_holds(const MaterialAidReadContext&,const AidScheduleCommitment&);
const MaterialAidEstablishment& establishment(const MaterialAidReadContext&,const SourceRef&);
const MaterialAidObligation& obligation(const MaterialAidReadContext&,const ObligationRef&);
MaterialAidActualIssue original_issue(const MaterialAidReadContext&,const MaterialAidObligation&,const AidSourceLegRef&);
MaterialAidActualReceipt original_receipt(const MaterialAidReadContext&,const MaterialAidObligation&,const AidSourceLegRef&);
MaterialAidActualIssue issue(const MaterialAidReadContext&,const MaterialAidObligation&,const MaterialAidResourceIssue&);
MaterialAidActualReceipt receipt(const MaterialAidReadContext&,const MaterialAidObligation&,const MaterialAidReceipt&);
std::vector<MaterialAidRemaining> remaining(const MaterialAidReadContext&,const MaterialAidObligation&,
 const WorldMoment* received_through=nullptr,const SourceRef* actual_close_cut=nullptr);
MaterialAidObligationState projected_state(const MaterialAidObligation&,const std::vector<MaterialAidRemaining>&);
// Original establishment only, before the common SourceOnly writer. It never
// reads P11, cash authority, or future Aid receiving adoptions.
void validate_established_terms(const MaterialAidReadContext&,const MaterialAidObligation&);
void validate_obligation(const MaterialAidReadContext&,const MaterialAidObligation&);
void validate_obligation_for_budget_evidence(const MaterialAidReadContext&,const MaterialAidObligation&);
const BudgetExecutionBinding& adoption_for(const MaterialAidAcceptedResources&,const BudgetUnit&);
void validate_binding(const MaterialAidReadContext&,const AidScheduleCommitment&,const BudgetExecutionBinding&);
}
