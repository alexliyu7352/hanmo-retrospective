#pragma once
#include "material_aid.hpp"
namespace hanmo::resident {
// /2 owns references to original choices and actual dispositions, not copied
// maximum amounts, paid totals, wallet balances or another treaty schedule.
struct AidCompensationScope2 {
 StableId id;SourceRef original_schedule_commitment;MomentOwnerSource original_resource_acceptance;
 WorldMoment accepted_at;std::vector<StableId> occurrence_hold_refs;
};
struct AidCompensationHold2 {
 StableId id,scope_id;Integer occurrence_ordinal=0;std::optional<StableId> disposition_ref;
};
enum class AidCompensationDispositionKind2 { received_in_time, cancelled_future, late_due };
struct AidCompensationDisposition2 {
 StableId id,hold_id;AidCompensationDispositionKind2 kind=AidCompensationDispositionKind2::received_in_time;
 MomentOwnerSource original_cause;WorldMoment occurred_at;
 // Exactly a positive original maximum creates a payable. Zero maximum is a
 // real retained late terminal and has neither invented debt nor arrear.
 std::optional<StableId> original_obligation_id,original_arrear_id;
};
struct AidCompensationPayment2 {StableId id,disposition_id;WorldMoment paid_at;};
struct MaterialAidCompensationState2 {
 OrderedTable<AidCompensationScope2> scopes;OrderedTable<AidCompensationHold2> holds;
 OrderedTable<AidCompensationDisposition2> dispositions;OrderedTable<AidCompensationPayment2> payments;
};
// One explicit selected whole root. The original /1 subrecord has unchanged
// fields/bytes; it is owned here once, not installed as a second world root.
struct TreatyMaterialAidState2 {
 Integer schema_version=2;TreatyMaterialAidState principal;
 MaterialAidCompensationState2 compensation;
};
struct MaterialAidCompensationReadContext2 {
 const MaterialAidReadContext& principal;const MaterialAidCompensationState2& state;
};
struct MaterialAidCompensationOriginal2 {
 const AidCompensationScope2*scope=nullptr;const AidCompensationHold2*hold=nullptr;
 const AidCompensationDisposition2*disposition=nullptr;
 MaterialAidAcceptedResources accepted;const TreatyBoundedCompensation*policy=nullptr;
};
SourceRef material_aid_compensation_scope_source2(const StableId&);
SourceRef material_aid_compensation_disposition_source2(const StableId&);
Status validate_material_aid_compensation_values2(const MaterialAidCompensationState2&);
// Source-local, intentionally never calls P11 or complete budget totals back.
Outcome<MaterialAidCompensationOriginal2> read_material_aid_compensation_original2(
 const MaterialAidCompensationReadContext2&,const MomentOwnerSource&);
Status validate_material_aid_compensation_original_coverage2(const MaterialAidCompensationReadContext2&);
class NativeMaterialAidCompensationBudgetRead2 final:public BudgetResponsibilityRead {
 public:explicit NativeMaterialAidCompensationBudgetRead2(const MaterialAidCompensationReadContext2&x):x_(x){}
 Outcome<ActualBudgetResponsibility> read_budget_responsibility(const BudgetExecutionBinding&)const override;
 private:const MaterialAidCompensationReadContext2&x_;
};
// Each operation belongs to the outer atomic candidate. A failure discards
// Common IDs/facts, principal, compensation, P11 and budget together.
Outcome<SourceRef> accept_material_aid_compensation_in_candidate2(CommonStore&,
 MaterialAidCompensationState2&,const MaterialAidCompensationReadContext2&,
 const MaterialAidWriteContext&,const SourceRef&actual_schedule_commitment);

Outcome<std::optional<SourceRef>> release_material_aid_compensation_on_receipt_in_candidate2(CommonStore&,
 MaterialAidCompensationState2&,const MaterialAidCompensationReadContext2&,const SourceRef&actual_receipt);
Outcome<std::vector<SourceRef>> release_material_aid_compensation_on_termination_in_candidate2(CommonStore&,
 MaterialAidCompensationState2&,const MaterialAidCompensationReadContext2&,const SourceRef&actual_schedule_release);
Outcome<std::optional<SourceRef>> close_material_aid_compensation_deadline_in_candidate2(CommonStore&,
 TreatyMaterialAidState&,MaterialAidCompensationState2&,const MaterialAidCompensationReadContext2&,
 const MaterialAidWriteContext&,const ObligationRef&);
Status validate_material_aid_compensation_payable2(const MaterialAidCompensationReadContext2&,
 const MaterialAidObligation&,const TreatyBoundedCompensation&,const ObligationRef&,const WorldMoment&);

Status validate_material_aid_compensation_payable_for_budget_evidence2(const MaterialAidCompensationReadContext2&,
 const MaterialAidObligation&,const TreatyBoundedCompensation&,const ObligationRef&,const WorldMoment&);

Outcome<SourceRef> pay_material_aid_compensation_in_candidate2(CommonStore&,MaterialAidCompensationState2&,
 const MaterialAidCompensationReadContext2&,const MaterialAidWriteContext&,const MomentOwnerSource&actual_due,Integer amount);

Outcome<WorldMoment> read_material_aid_compensation_source_moment2(const MaterialAidCompensationReadContext2&,const SourceRef&,Integer schema);

}
