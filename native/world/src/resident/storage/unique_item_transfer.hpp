// Original Common unique-item custody transfer. No business authorization,
// settlement, title acquisition or persistent source is created here.
#pragma once
#include "common_store.hpp"
#include "resource_execution_draft.hpp"
#include "unique_item_transfer_model.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
namespace hanmo::resident {
struct AppliedUniqueItemTransfer {
 const ResourceExecutionDraft* recipe=nullptr;
 const ResourceOperationExecutionDraft* operation=nullptr;
 const ResourceLotFlowDraft* flow=nullptr;
 const LotView* original_item=nullptr;
 const LotView* delivered_item=nullptr;
};
// Call-scoped output ownership only. The existing durable effect remains the
// original source row's payload; this envelope is never a persistent source.
class OwnedUniqueItemTransferEffect {
 CheckpointCopyLease memory_; // first: survives all ordinary payload fields
 OriginalUniqueItemTransferEffect value_;
public:
 OwnedUniqueItemTransferEffect()=default;
 explicit OwnedUniqueItemTransferEffect(const OriginalUniqueItemTransferEffectBuildView&);
 OwnedUniqueItemTransferEffect(const OwnedUniqueItemTransferEffect&)=delete;
 OwnedUniqueItemTransferEffect&operator=(const OwnedUniqueItemTransferEffect&)=delete;
 OwnedUniqueItemTransferEffect(OwnedUniqueItemTransferEffect&&)noexcept;
 OwnedUniqueItemTransferEffect&operator=(OwnedUniqueItemTransferEffect&&)noexcept;
 void swap(OwnedUniqueItemTransferEffect&)noexcept;
 const OriginalUniqueItemTransferEffect& value()const& noexcept{return value_;}
 const OriginalUniqueItemTransferEffect& value()const&&=delete;
};
// Read the exact just-applied capture; this result is not historical title authority.
Outcome<OwnedUniqueItemTransferEffect> capture_original_unique_item_transfer_effect(
 const CommonStore&,const AppliedUniqueItemTransfer&);
// The owner must first verify the actual handover/return scope, original title,
// receiver acceptance, reachability and applicable exclusive-use reservation.
// This is the existing TransferSelectedLots implementation, not a new mutation
// engine. It preserves instance/lot identity and legal owner, moves exactly one
// item, and proves the actual captured operation and whole unchanged remainder.
// Returned pointers borrow this candidate capture until its next mutation.
Outcome<AppliedUniqueItemTransfer> apply_original_unique_item_transfer_in_candidate(
 CommonStore&,const CargoSelection& actual_selection,const StableId& actual_destination,
 const StableId& instance_id,const Owner& actual_legal_owner,const ReservationRef& actual_reservation);
}
