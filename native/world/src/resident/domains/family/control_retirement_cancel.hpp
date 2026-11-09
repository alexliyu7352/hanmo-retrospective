// P17.1/P17.4: an explicit living predecessor cancels the original voluntary
// preparation. This is not a system disposition for a later death.
#pragma once
#include "native_continuation_sources.hpp"
#include "resident/kernel/common_command_sources.hpp"
namespace hanmo::resident {
struct CancelPendingRetirement {
 StableId pending_id;Integer expected_control_epoch=0,expected_pending_generation=1;
};
struct PendingRetirementCancelled {
 StableId pending_id;Integer generation=0;CauseRef original_withdrawn_preparation;
};
// Borrow only the original live source. Preview and mutation share the same
// gate; no copied source or receipt is constructed by this preparation.
struct PreparedPendingRetirementCancellation {
 const PendingContinuation* pending=nullptr;
 const SuccessionConsentFact* voluntary_preparation=nullptr;
};
Outcome<PreparedPendingRetirementCancellation> read_cancel_pending_retirement(
 const CommonStore&,const FamilyState&,const ControlContinuationState&,const ControlContinuationCatalog&,
 const CatalogIdentity&,const NativeContinuationOriginalSourceRead&,const StableId& actual_command_actor,
 const CancelPendingRetirement&);
struct PendingRetirementCancelledBuildView {
 const StableId& pending_id;Integer generation;
 const CauseRef& original_withdrawn_preparation;
};
// Transient outgoing ownership only. The immutable Common receipt remains the
// sole persistent DTO; central reserves its own copy while this token is alive.
class ProducedPendingRetirementCancelled {
 friend Outcome<ProducedPendingRetirementCancelled> cancel_pending_retirement_in_candidate(
  CommonStore&,FamilyState&,ControlContinuationState&,const ControlContinuationCatalog&,
  const CatalogIdentity&,const NativeContinuationOriginalSourceRead&,const StableId&,const CancelPendingRetirement&);
 CheckpointMemoryCharge copied_fields_;PendingRetirementCancelled receipt_;
 explicit ProducedPendingRetirementCancelled(const PendingRetirementCancelledBuildView&);
public:
 ProducedPendingRetirementCancelled()=default;
 ProducedPendingRetirementCancelled(const ProducedPendingRetirementCancelled&)=default;
 ProducedPendingRetirementCancelled(ProducedPendingRetirementCancelled&&)noexcept=default;
 ProducedPendingRetirementCancelled& operator=(const ProducedPendingRetirementCancelled&)=delete;
 ProducedPendingRetirementCancelled& operator=(ProducedPendingRetirementCancelled&&)=delete;
 const PendingRetirementCancelled& receipt()const noexcept{return receipt_;}
};
Status validate_pending_retirement_cancelled_shape(const PendingRetirementCancelled&);
Outcome<CancelPendingRetirement> decode_cancel_pending_retirement(const BoundaryValue&);
Status visit_pending_retirement_cancelled(CodecVisitor&,PendingRetirementCancelled&);
Status emit_pending_retirement_cancelled_fields(ProjectionWriter&,const PendingRetirementCancelled&);
Outcome<BoundaryValue> encode_pending_retirement_cancelled(const PendingRetirementCancelled&);
Outcome<PendingRetirementCancelled> decode_pending_retirement_cancelled(const BoundaryValue&);
// The only withdrawal history is the actual Common typed result and its unique
// HistoryRecord. No synthesized fact/ID or second conclusion ledger is used.
Outcome<CommonCommandSourceView> read_original_pending_retirement_withdrawal(
 const CommonStore&,const FamilyState&,const ControlContinuationState&,const StableId& pending_id);
// Called only by the original S0 guarded command in its private world candidate.
// Family's actual conclusion and Control's generation/status publish together.
Outcome<ProducedPendingRetirementCancelled> cancel_pending_retirement_in_candidate(CommonStore&,FamilyState&,ControlContinuationState&,const ControlContinuationCatalog&,const CatalogIdentity&,const NativeContinuationOriginalSourceRead&,const StableId& actual_command_actor,const CancelPendingRetirement&);
}
