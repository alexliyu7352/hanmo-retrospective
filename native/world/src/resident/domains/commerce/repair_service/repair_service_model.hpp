// E16.7 / I25: S7 original Repair contract, physically in item_instances/3.
#pragma once
#include "resident/domains/craft/repair_model.hpp"
#include "resident/domains/local_economy/obligation_ref.hpp"
#include "resident/domains/growth/growth_fixed_service_model.hpp"
#include "resident/storage/unique_item_transfer_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
// Only the original fixed RepairPolicy fee is supported by this native rule.
// There is no time-price, partial-service proration or implicit material fee.
enum class RepairServiceFeeTrigger {actual_start,repair_completed};
// Immutable author inputs in the selected item_instances/3 catalog. Every
// field is explicit; the sole fee amount remains original RepairPolicy.fee_qian.
struct RepairServiceFeePolicy {StableId policy_id;Integer version=0;RepairServiceFeeTrigger trigger=RepairServiceFeeTrigger::repair_completed;};
struct RepairServiceCustodyPolicy {StableId policy_id;Integer version=0;bool delivery_before_acceptance=false;Integer uncollected_fee_qian=0,uncollected_period_half_days=0;};
struct RepairServiceRuleCatalog {OrderedTable<RepairServiceFeePolicy> fee_policies;OrderedTable<RepairServiceCustodyPolicy> custody_policies;};
struct RepairServiceFeeTerms {StableId line_id;Integer amount_qian=0;RepairServiceFeeTrigger trigger=RepairServiceFeeTrigger::repair_completed;MomentOwnerSource original_rule_ref;};
struct RepairServiceReturnValues {Owner receiver;MomentOwnerSource receiver_authority;SourceRef site_ref;WorldMoment agreed_return_at;};
struct RepairServiceReturnTerms:RepairServiceReturnValues {MomentOwnerSource original_rule_ref;};
struct RepairServiceCustodyTerms {StableId inventory_id;MomentOwnerSource authority_ref;bool delivery_before_acceptance=false;WorldMoment collection_due_at;Integer uncollected_fee_qian=0,uncollected_period_half_days=0;MomentOwnerSource original_rule_ref;};
template<class Return>struct RepairServiceAgreementValues {
 StableId instance_id;RepairPolicyRef repair_policy_ref;StableId worker_id;Owner customer,provider;
 StableId payer_wallet_id,payee_wallet_id;MomentOwnerSource fee_policy_source;
 RepairServiceFeeTerms fee_terms;
 Return return_terms;RepairServiceCustodyTerms custody_terms;
 MomentOwnerSource owner_authority_source,payer_authority_source;
 Integer terms_revision=0;
};
using RepairServiceQuotedTerms=RepairServiceAgreementValues<RepairServiceReturnTerms>;
using RepairServiceOfferValues=RepairServiceAgreementValues<RepairServiceReturnValues>;
struct RepairServiceContractTerms:RepairServiceQuotedTerms {MomentOwnerSource actual_contact_source,customer_acceptance_source,provider_acceptance_source;};
struct RepairServiceAcceptanceInspection {CargoSelection original_item;ItemConditionHead condition_head;FacilitySlotRef workstation;std::optional<MomentOwnerSource> delivered_custody_source_ref;};
struct RepairServiceOfferSource {StableId id,provider_actor_id;WorldMoment offered_at;RepairServiceQuotedTerms quoted_terms;RepairServiceAcceptanceInspection inspection;MomentOwnerSource actual_contact_source,actual_command_source;LifeActorRoute actor_route;};
struct RepairServiceAssentSource {StableId id,customer_actor_id;WorldMoment accepted_at;MomentOwnerSource offer_ref,actual_contact_source,actual_command_source;LifeActorRoute actor_route;};
struct RepairServiceAcceptedResult {MomentOwnerSource commercial_contract,repair_acceptance;};
struct RepairServiceContractSource {StableId id;WorldMoment accepted_at;RepairServiceContractTerms terms;};
struct RepairCustodyReceivedBuildView;struct RepairItemReturnedBuildView;
struct RepairCustodyReceivedSource {
 CheckpointCopyLease copy_memory; // non-wire ordinary-field owner, first/destructed last
 RepairCustodyReceivedSource()=default;explicit RepairCustodyReceivedSource(const RepairCustodyReceivedBuildView&);RepairCustodyReceivedSource(const RepairCustodyReceivedSource&);RepairCustodyReceivedSource(RepairCustodyReceivedSource&&);
 RepairCustodyReceivedSource&operator=(const RepairCustodyReceivedSource&);RepairCustodyReceivedSource&operator=(RepairCustodyReceivedSource&&);void swap(RepairCustodyReceivedSource&);
 StableId id;MomentOwnerSource contract_ref;WorldMoment occurred_at;MomentOwnerSource actual_handover_source;CargoSelection actual_item_selection;ItemConditionHead actual_condition_head;std::vector<MomentOwnerSource> original_resource_effect_refs;MomentOwnerSource actual_contact_source;StableId actual_actor_id;LifeActorRoute actor_route;OriginalUniqueItemTransferEffect actual_transfer;};
struct RepairServiceChargeSource {StableId id;MomentOwnerSource contract_ref;StableId fee_line_id;MomentOwnerSource actual_trigger_source;WorldMoment earned_at;Integer actual_quantity=0,earned_qian=0;std::optional<ObligationRef> original_obligation_ref;std::optional<StableId> original_arrear_id;std::vector<SourceRef> payment_refs;};
struct RepairServicePaymentSource {StableId id;MomentOwnerSource charge_ref;SourceRef actual_payment_ref;WorldMoment paid_at;};
struct RepairServiceTerminationSettlementSource {StableId id;MomentOwnerSource contract_ref,actual_terminal_ref;WorldMoment settled_at;std::vector<MomentOwnerSource> preserved_charge_refs,custody_or_continuity_refs;};
struct RepairItemReturnedSource {
 CheckpointCopyLease copy_memory; // non-wire ordinary-field owner, first/destructed last
 RepairItemReturnedSource()=default;explicit RepairItemReturnedSource(const RepairItemReturnedBuildView&);RepairItemReturnedSource(const RepairItemReturnedSource&);RepairItemReturnedSource(RepairItemReturnedSource&&);
 RepairItemReturnedSource&operator=(const RepairItemReturnedSource&);RepairItemReturnedSource&operator=(RepairItemReturnedSource&&);void swap(RepairItemReturnedSource&);
 StableId id;MomentOwnerSource contract_ref;WorldMoment occurred_at;MomentOwnerSource actual_receiver_acceptance,actual_receiver_authority,actual_destination_source;CargoSelection actual_item_selection;ItemConditionHead actual_condition_head;std::vector<MomentOwnerSource> original_resource_effect_refs;MomentOwnerSource actual_contact_source;StableId actual_actor_id;LifeActorRoute actor_route;OriginalUniqueItemTransferEffect actual_transfer;};
// Borrow only the original fields while planning a fresh owned row. No new wire.
struct RepairCustodyReceivedBuildView {
 const StableId&id;const MomentOwnerSource&contract_ref;const WorldMoment&occurred_at;
 const MomentOwnerSource&actual_handover_source;const CargoSelection&actual_item_selection;
 const ItemConditionHead&actual_condition_head;const std::vector<MomentOwnerSource>&original_resource_effect_refs;
 const MomentOwnerSource&actual_contact_source;const StableId&actual_actor_id;const LifeActorRoute&actor_route;
 const OriginalUniqueItemTransferEffect&actual_transfer;
};
struct RepairItemReturnedBuildView {
 const StableId&id;const MomentOwnerSource&contract_ref;const WorldMoment&occurred_at;
 const MomentOwnerSource&actual_receiver_acceptance;const MomentOwnerSource&actual_receiver_authority;
 const MomentOwnerSource&actual_destination_source;const CargoSelection&actual_item_selection;
 const ItemConditionHead&actual_condition_head;const std::vector<MomentOwnerSource>&original_resource_effect_refs;
 const MomentOwnerSource&actual_contact_source;const StableId&actual_actor_id;const LifeActorRoute&actor_route;
 const OriginalUniqueItemTransferEffect&actual_transfer;
};
struct RepairServiceContractsState {
 OrderedTable<RepairServiceOfferSource> offers;
 OrderedTable<RepairServiceAssentSource> assents;
 OrderedTable<RepairServiceContractSource> contracts;
 OrderedTable<RepairCustodyReceivedSource> custody_receipts;
 OrderedTable<RepairServiceChargeSource> charges;
 OrderedTable<RepairServicePaymentSource> payments;
 OrderedTable<RepairServiceTerminationSettlementSource> termination_settlements;
 OrderedTable<RepairItemReturnedSource> returns;
};
bool repair_service_contracts_empty(const RepairServiceContractsState&)noexcept;
}
