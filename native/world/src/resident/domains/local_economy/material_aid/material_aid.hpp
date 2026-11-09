#pragma once
#include "material_aid_model.hpp"
#include "../obligations/obligation_sources_read.hpp"
#include "resident/domains/local_economy/local_economy_model.hpp"
#include "resident/domains/politics/treaty/treaty_aid_occurrence.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
// Borrowed original S3 acceptance. The provider must validate the actual two
// sides' resource/custody permissions at its real Common cut. Allowed containers
// in a budget are not a selection. No current-first-container fallback exists.
struct MaterialAidAcceptedCompensation {
 AidMoneyContainerRoute money_route;BudgetExecutionBinding budget_adoption;
};
struct MaterialAidAcceptedResources {
 MomentOwnerSource actual_acceptance;StableId treaty_id;Integer terms_revision=0;StableId term_id;WorldMoment accepted_at;
 StableId source_party;SubjectRef beneficiary;SourceRef delivery_site;
 std::vector<AidItemContainerRoute> item_routes;std::optional<AidMoneyContainerRoute> money_route;
 std::vector<MomentOwnerSource> authority_refs;
 // One real adopted budget scope per distinct measure. IDs/responsibility and
// accepted_at are filled only by the S5 accepting writer, never by the caller.
 std::vector<BudgetExecutionBinding> budget_adoptions;
 // Explicit immutable /2 selection; never default to the ordinary money route.
 std::optional<MaterialAidAcceptedCompensation> compensation;
};
struct MaterialAidActualIssue {
 AidSourceLegRef actual_source;AidOccurrenceKey occurrence;BudgetUnit unit;Integer quantity=0;
 StableId source_container_id,beneficiary_container_id;WorldMoment occurred_at;
 // Shipment for goods; actual transferred payment leg for money. An escrow
// deposit may be an issue but cannot itself be a beneficiary receipt.
 SourceRef original_transport_or_payment;
};
struct MaterialAidActualReceipt {
 AidSourceLegRef actual_source,actual_issue;AidOccurrenceKey occurrence;
 BudgetUnit unit;Integer quantity=0;StableId beneficiary_container_id;WorldMoment occurred_at;
};
class MaterialAidOriginalResourcesRead {public:
 virtual ~MaterialAidOriginalResourcesRead()=default;
 virtual bool bound_to(const CommonStore&)const noexcept=0;
 virtual Outcome<MaterialAidAcceptedResources> read_accepted_aid_resources(
  const MomentOwnerSource&)const=0;
 virtual Outcome<MaterialAidActualIssue> read_actual_aid_issue(const AidSourceLegRef&)const=0;
 virtual Outcome<MaterialAidActualReceipt> read_actual_aid_receipt(const AidSourceLegRef&)const=0;
 virtual Outcome<std::vector<AidSourceLegRef>> enumerate_actual_aid_issues(const AidOccurrenceKey&)const=0;
 virtual Outcome<std::vector<AidSourceLegRef>> enumerate_actual_aid_receipts(const AidOccurrenceKey&)const=0;
 // The original receiving producer must borrow the sole LE cash receipt
 // already recorded by service_finance, with its exact original source/M.
 // This never performs another transfer or manufactures a receipt from a sum.
 // Cash identity is source-local, with no call back to full P11/Aid/budget.
 // Authority may consume the named original budget-evidence projection,
 // but must never call the complete P11/Aid validation back.
 virtual Outcome<OriginalEconomicCashPayment> read_actual_aid_money_payment(
  const AidSourceLegRef& r)const {return {false,{},common_failure("MaterialAidOriginalCashProducerRequired",r.source.source_ref.id,ErrorKind::unsupported_capability).error};}
 virtual Status validate_actual_aid_money_payment_authority(const AidSourceLegRef& r)const {
  return common_failure("MaterialAidOriginalCashAuthorityRequired",r.source.source_ref.id,ErrorKind::unsupported_capability);}
 virtual Status validate_original_aid_shipment(const AidOccurrenceKey&,const SourceRef&)const=0;
 // An actual bounded-compensation liability is never inferred from no money.
 virtual Status validate_aid_compensation(const MaterialAidObligation&,
  const TreatyBoundedCompensation&,const ObligationRef&,const WorldMoment&)const=0;
 virtual Status validate_aid_compensation_for_budget_evidence(const MaterialAidObligation&,
  const TreatyBoundedCompensation&,const ObligationRef&,const WorldMoment&)const=0;
};
struct MaterialAidReadContext {
 const CommonStore& common;const TreatyMaterialAidState& state;const LocalEconomyState& economy;
 const OrderedTable<TreatyAidSchedule>& schedules;const TreatyAidTermsOriginalRead& terms;
 const MaterialAidOriginalResourcesRead& resources;const BudgetExecutionState& budget;
 const CurrentMomentRead& clock;
 const EconomicObligationReadContext* obligations=nullptr;
 // Selected original S6 source group, fixed by the same-world owning factory.
 Integer selected_treaty_sources=1;
};
struct MaterialAidWriteContext {
 const MaterialAidReadContext& read;LocalEconomyState& economy;BudgetExecutionState& budget;
 const BudgetAuthorizationRead& budget_authorization;
 // Full same-world composite, including NativeMaterialAidBudgetRead. This
// is required so capacity counts all original responsibilities, not just aid.
 const BudgetResponsibilityRead& budget_responsibilities;
 EconomicObligationWriteContext* obligation_writes=nullptr;
};
struct MaterialAidRemaining {BudgetUnit unit;Integer promised=0,issued=0,received=0,remaining=0;};
SourceRef aid_schedule_commitment_source(const StableId&);
SourceRef material_aid_establishment_source(const StableId&);
SourceRef material_aid_issue_source(const StableId&);
SourceRef material_aid_receipt_source(const StableId&);
SourceRef material_aid_schedule_release_source(const StableId&);
SourceRef material_aid_deadline_source(const StableId&);
bool same_aid_occurrence(const AidOccurrenceKey&,const AidOccurrenceKey&)noexcept;
Status validate_treaty_material_aid(const MaterialAidReadContext&);
Outcome<const AidScheduleCommitment*> read_aid_schedule_commitment(
 const MaterialAidReadContext&,const SourceRef&);
Outcome<const MaterialAidScheduleRelease*> read_material_aid_schedule_release(
 const MaterialAidReadContext&,const SourceRef&);
Outcome<const MaterialAidDeadlineResult*> read_material_aid_deadline_result(
 const MaterialAidReadContext&,const SourceRef&);
Outcome<const MaterialAidObligation*> read_material_aid_obligation(
 const MaterialAidReadContext&,const ObligationRef&);
Outcome<std::vector<MaterialAidRemaining>> read_material_aid_remaining(
 const MaterialAidReadContext&,const ObligationRef&);
// Identity/cash projection from a genuine original receiving leg. The full
// obligation reader separately validates permission and publication coverage.
Outcome<OriginalEconomicCashPayment> read_material_aid_actual_cash_payment(
 const MaterialAidReadContext&,const ObligationRef&,const AidSourceLegRef&);
Outcome<std::optional<WorldMoment>> read_material_aid_original_money_fact_moment(
 const MaterialAidReadContext&,const SourceRef&);
Outcome<WorldMoment> read_material_aid_source_moment(
 const MaterialAidReadContext&,const SourceRef&,Integer schema);
// All writes are internal to an outer World candidate. Any failing return
// requires discarding that candidate, including IDs, holds and budget writes.
Outcome<SourceRef> accept_aid_schedule_in_candidate(CommonStore&,TreatyMaterialAidState&,
 const MaterialAidWriteContext&,const StableId& actual_schedule_id,
 const MomentOwnerSource& actual_resource_acceptance);
// Required once all real schedules of the same activation have been accepted.
// Publication also runs the identical full two-way financial/budget validators.
Status finalize_material_aid_acceptances_in_candidate(const MaterialAidWriteContext&);
Outcome<SourceRef> establish_material_aid_occurrence_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidWriteContext&,const StableId& schedule_id,Integer occurrence_ordinal);
// Consume every original issue/receipt leg of one real source atomically,
// including a same-fact payment's two sides and explicit cross-occurrence legs.
Outcome<std::vector<SourceRef>> consume_material_aid_actual_event_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidWriteContext&,const SourceRef& actual_event);
Outcome<SourceRef> receive_material_aid_issue_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidWriteContext&,const ObligationRef&,
 const AidSourceLegRef& actual_issue);
Outcome<SourceRef> receive_material_aid_delivery_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidWriteContext&,const ObligationRef&,
 const AidSourceLegRef& actual_receipt);
// Apply every schedule of the same real termination before full publication
// validation. This operation preserves all established debts and in-flight issues.
Outcome<SourceRef> terminate_aid_schedule_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidReadContext&,const StableId& schedule_id);
// Invoke only at the original politics_due_close, after true deliveries at M.
// Fully received occurrences produce no result; a late result is retained once.
Outcome<std::optional<SourceRef>> close_material_aid_deadline_in_candidate(CommonStore&,
 TreatyMaterialAidState&,const MaterialAidReadContext&,const ObligationRef&,
 const std::optional<ObligationRef>& actual_compensation={});
class NativeMaterialAidBudgetRead final:public BudgetResponsibilityRead {public:
 explicit NativeMaterialAidBudgetRead(const MaterialAidReadContext&x):x_(x){}
 Outcome<ActualBudgetResponsibility> read_budget_responsibility(const BudgetExecutionBinding&)const override;
 private:const MaterialAidReadContext&x_;
};
class NativeTreatyAidOccurrenceRead final:public TreatyAidOccurrenceOriginalRead {public:
 explicit NativeTreatyAidOccurrenceRead(const MaterialAidReadContext&x):x_(x){}
 Outcome<TreatyAidEstablishmentRead> read_material_aid_establishment(const SourceRef&)const override;
 Outcome<std::vector<SourceRef>> enumerate_material_aid_establishments(const SourceRef&)const override;
 Status validate_material_aid_shipment(const TreatyAidEstablishmentRead&,const SourceRef&)const override;
 Status validate_material_aid_receipt(const TreatyAidEstablishmentRead&,const std::optional<SourceRef>&,const SourceRef&)const override;
 Status validate_material_aid_deadline_result(const TreatyAidEstablishmentRead&,const SourceRef&)const override;
 Status validate_material_aid_termination_disposition(const TreatyAidEstablishmentRead&,const SourceRef&)const override;
 private:const MaterialAidReadContext&x_;
};
}
