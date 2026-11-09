// A10 / P27: the original finite financial commitment. No inventory, wallet
// balance, Shipment state or second treaty due cursor is owned here.
#pragma once
#include "resident/domains/local_economy/fiscal/budget_execution_contract.hpp"
#include "resident/domains/local_economy/obligation_ref.hpp"
#include "resident/domains/politics/treaty/treaty_aid_schedule.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
struct AidOccurrenceKey {
 StableId treaty_id;Integer terms_revision=0;StableId term_id;Integer ordinal=0;
};
struct AidItemContainerRoute {StableId item_id,source_inventory_id,beneficiary_inventory_id;};
struct AidMoneyContainerRoute {StableId source_wallet_id,beneficiary_wallet_id;};
struct AidScheduleCommitment {
 StableId id,schedule_id;SourceRef treaty_term_ref;MomentOwnerSource resource_acceptance_ref;
 WorldMoment accepted_at;std::vector<MomentOwnerSource> authority_refs;
 std::vector<StableId> occurrence_cap_reservation_refs,budget_binding_refs;
 std::vector<SourceRef> established_occurrence_refs;
 std::optional<SourceRef> termination_source_ref;
};
struct AidOccurrenceCapReservation {
 StableId id,schedule_commitment_id;Integer occurrence_ordinal=0;BudgetUnit unit;Integer quantity=0;
 // Both absent: future authorization hold. Exactly one present: its actual
// same-candidate conversion or release. These are references, not new amounts.
 std::optional<SourceRef> establishment_result_ref,release_result_ref;
};
enum class MaterialAidObligationState {owed,partly_received,received,late,settling,closed};
struct MaterialAidObligation {
 StableId id,schedule_commitment_id;AidOccurrenceKey occurrence;
 WorldMoment established_at;Integer due_tick=0,delivery_deadline_tick=0;
 SubjectRef beneficiary;SourceRef delivery_site;
 std::vector<TreatyAidItemAmount> frozen_items;std::optional<TreatyAidMoney> frozen_money;
 MomentOwnerSource resource_acceptance_ref;std::vector<AidItemContainerRoute> item_routes;
 std::optional<AidMoneyContainerRoute> money_route;
 std::optional<StableId> original_money_obligation_id,original_money_arrear_id;
 std::vector<StableId> issue_result_refs,receipt_result_refs;
 std::optional<StableId> deadline_result_ref;
 MaterialAidObligationState state=MaterialAidObligationState::owed;
};
struct MaterialAidEstablishment {
 StableId id,schedule_commitment_id;Integer occurrence_ordinal=0;
 ObligationRef original_obligation_ref;WorldMoment established_at;
 std::vector<StableId> converted_reservation_refs;
};
// An actual original receiving/issuing leg; ordinal selects the real owner
// payload line, never a fabricated Common header for each item.
struct AidSourceLegRef {MomentOwnerSource source;Integer leg_ordinal=0;};
struct MaterialAidResourceIssue {
 StableId id,obligation_id;AidSourceLegRef actual_issue_ref;WorldMoment occurred_at;
};
struct MaterialAidReceipt {
 StableId id,obligation_id,issue_result_id;AidSourceLegRef actual_receipt_ref;WorldMoment occurred_at;
};
struct MaterialAidScheduleRelease {
 StableId id,schedule_commitment_id;MomentOwnerSource actual_termination_ref;WorldMoment occurred_at;
 std::vector<StableId> released_reservation_refs;
};
struct MaterialAidDeadlineResult {
 StableId id,obligation_id;WorldMoment occurred_at;
 // The bounded-compensation producer must supply its own real obligation.
 std::optional<ObligationRef> compensation_obligation_ref;
};
struct TreatyMaterialAidState {
 Integer schema_version=1;
 OrderedTable<AidScheduleCommitment> schedule_commitments;
 OrderedTable<AidOccurrenceCapReservation> occurrence_cap_reservations;
 OrderedTable<MaterialAidEstablishment> establishments;
 OrderedTable<MaterialAidObligation> obligations;
 OrderedTable<MaterialAidResourceIssue> resource_issues;
 OrderedTable<MaterialAidReceipt> receipts;
 OrderedTable<MaterialAidScheduleRelease> schedule_releases;
 OrderedTable<MaterialAidDeadlineResult> deadline_results;
};
}
