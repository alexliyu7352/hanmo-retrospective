// P17 + C4.9: the single persistent control chain. Current Director routes,
// characters, property, offices and old Family consent payloads stay with their
// owners.
#pragma once
#include "family_continuation_people4.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/domains/interaction/interaction_response_sources.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/read_stamp.hpp"
namespace hanmo::resident {
struct CauseRef {
  StableId domain, kind, fact_id;
  std::optional<Integer> ordinal;
};
struct ConsentRef {
  StableId proposal_id;
  Integer terms_version = 1;
  StableId decision_actor_id, accepted_fact_id;
};
bool same_continuation_cause(const CauseRef &, const CauseRef &) noexcept;
bool same_continuation_consent(const ConsentRef &, const ConsentRef &) noexcept;
enum class ContinuationMode { voluntary_retirement, after_death };
enum class ContinuationPath {
  biological,
  adopted,
  collateral,
  adult_adopted,
  disciple
};
enum class ContinuationStatus { open, activated, cancelled, terminal_retained };
enum class ContinuationBoundaryOwner { military, interaction };
enum class ContinuationBoundaryStatus {
  ready,
  yielded,
  candidate_changed,
  fault
};
enum class ContinuationBoundaryOperation {
  resolve_frozen,
  close_terminal,
  resume_selected
};
enum class ContinuationMilitaryBoundary { planning, result, closed };
using ContinuationExpectedBoundary =
    std::variant<ContinuationMilitaryBoundary, InteractionResponseNext>;
struct ContinuationBoundarySessionRef {
  StableId session_id;
  Integer session_generation = 1;
};
// The original P17.5 Pending appendix; it owns no world clock, progress or
// second control binding. Historical slices remain in actual Common receipts.
struct ContinuationBoundaryWork {
  StableId candidate;
  ContinuationPath path = ContinuationPath::adopted;
  CauseRef basis_ref;
  ConsentRef acceptance_ref;
  ContinuationBoundaryOwner owner = ContinuationBoundaryOwner::interaction;
  ContinuationBoundarySessionRef session_ref;
  Integer frozen_ordinal = 0;
  ContinuationBoundaryOperation next_operation =
      ContinuationBoundaryOperation::resume_selected;
  std::vector<MomentOwnerSource> completed_source_refs;
  ContinuationBoundaryStatus status = ContinuationBoundaryStatus::yielded;
};
struct ControlBinding {
  StableId actor_id;
  Integer control_epoch = 0;
};
struct PendingContinuation {
  StableId pending_id;
  ContinuationMode mode = ContinuationMode::after_death;
  StableId predecessor_actor_id;
  CauseRef source_ref;
  Integer created_tick = 0, generation = 1;
  ContinuationStatus status = ContinuationStatus::open;
  Integer control_epoch_at_open = 0;
  std::optional<ContinuationBoundaryWork>
      boundary_work; // Explicit Control3 only.
};
struct ControlRequestOrigin {
  WorldId world_id{};
  Integer execution_epoch_at_commit = 0;
  StableId predecessor_actor_id;
  Integer request_seq = 0;
};
struct ControlActivationRecord {
  StableId activation_id, pending_id, predecessor_actor_id, successor_actor_id;
  ContinuationPath path = ContinuationPath::adopted;
  CauseRef basis_ref;
  ConsentRef acceptance_ref;
  ContinuationMode mode = ContinuationMode::after_death;
  CauseRef source_ref;
  Integer effective_tick = 0, previous_control_epoch = 0, new_control_epoch = 0;
  StableId command_result_ref;
  ControlRequestOrigin request_origin;
  StableId command_kind;
  Integer committed_tick = 0, committed_revision = 0;
};
// An external-source row is a typed reference, never a copy of the Legacy or
// voluntary source. Actual payload/withdrawal and knowledge remain original.
struct ContinuationConsentSource {
  enum class Decision { accept, revoke };
  Decision decision = Decision::accept;
  ConsentRef consent_ref;
  CauseRef original_source;
};
struct ControlContinuationState {
  ControlBinding control_binding;
  OrderedTable<PendingContinuation> pending_continuations;
  OrderedTable<ControlActivationRecord> activation_records;
  OrderedTable<ContinuationConsentSource> continuation_consent_sources;
};
struct ControlContinuationCatalog {
  bool enabled = false;
  Integer selected_version = 2;
};
Outcome<ControlContinuationCatalog>
decode_control_continuation_catalog(const BoundaryValue &);
Status validate_control_continuation_catalog(const ControlContinuationCatalog &,
                                             const CatalogIdentity &);
Status visit_control_continuation_v2(CodecVisitor &,
                                     ControlContinuationState &);
Outcome<BoundaryValue>
encode_control_continuation_v2(const ControlContinuationState &);
Outcome<ControlContinuationState>
decode_control_continuation_v2(const BoundaryValue &);
Status
emit_control_continuation_v2_projection(ProjectionWriter &, std::string_view,
                                        const ControlContinuationState &);
Status visit_control_continuation_v3(CodecVisitor &,
                                     ControlContinuationState &);
Outcome<BoundaryValue>
encode_control_continuation_v3(const ControlContinuationState &);
Outcome<ControlContinuationState>
decode_control_continuation_v3(const BoundaryValue &);
Status
emit_control_continuation_v3_projection(ProjectionWriter &, std::string_view,
                                        const ControlContinuationState &);

// Pure page/selection references; none is persisted or accepted as authority.
enum class ContinuationObserverKind { active_actor, continuation_controller };
struct ContinuationObserver {
  ContinuationObserverKind kind = ContinuationObserverKind::active_actor;
  StableId actor_id;
  Integer control_epoch = 0;
};
struct ContinuationCandidateRef {
  StableId pending_id;
  Integer pending_generation = 1;
  StableId candidate_actor_id;
  ContinuationPath path = ContinuationPath::adopted;
  CauseRef basis_ref;
  ConsentRef acceptance_ref;
  WorldIdentity source_identity;
  Integer control_epoch = 0;
};
// Pure source reference. candidate supplies the original first eight fields;
// codecs flatten them in the P17.5 order rather than mirror candidate state.
struct ContinuationBoundaryRef {
  ContinuationCandidateRef candidate;
  ContinuationBoundaryOwner owner = ContinuationBoundaryOwner::interaction;
  StableId session_ref;
  Integer session_generation = 1, frozen_segment_ordinal = 0;
  ContinuationExpectedBoundary expected_next_boundary =
      InteractionResponseNext{InteractionResponseTerminal{}};
};
struct StabilizeContinuationBoundary {
  StableId pending_id;
  Integer expected_pending_generation = 1;
  StableId candidate_actor_id;
  ContinuationPath path = ContinuationPath::adopted;
  CauseRef basis_ref;
  ConsentRef acceptance_ref;
  ContinuationBoundaryRef boundary_ref;
};
struct ContinuationBoundaryResult {
  SourceRef
      boundary_work_ref; // Actual Common command_result/1, filled by its seal.
  ContinuationBoundaryStatus status = ContinuationBoundaryStatus::fault;
  std::vector<MomentOwnerSource> settled_source_refs;
  Integer world_half_days = 0;
  std::optional<ContinuationBoundaryRef> next_boundary_ref;
};
// Sole immutable history of a committed slice, embedded in the Common receipt.
struct ContinuationBoundaryReceipt {
  StabilizeContinuationBoundary request;
  ContinuationBoundaryResult result;
  WorldMoment actual_at;
  // Captured from actual Common immediately after the S4 prepare check, before
  // the one original owner mutates. This is part of this same immutable
  // receipt.
  CommonActualFactPrefix candidate_prefix;
};
Outcome<StabilizeContinuationBoundary>
decode_stabilize_continuation_boundary(const BoundaryValue &);
Status
validate_continuation_boundary_ref_shape(const ContinuationBoundaryRef &);
Status validate_continuation_boundary_request_shape(
    const StabilizeContinuationBoundary &);
Status validate_continuation_boundary_receipt_shape(
    const ContinuationBoundaryReceipt &);
Status
validate_continuation_boundary_work_shape(const ContinuationBoundaryWork &);
bool same_continuation_boundary_work(const ContinuationBoundaryWork &,
                                     const ContinuationBoundaryWork &) noexcept;
Status visit_continuation_boundary_receipt(CodecVisitor &,
                                           ContinuationBoundaryReceipt &);
Status
emit_continuation_boundary_receipt_fields(ProjectionWriter &,
                                          const ContinuationBoundaryReceipt &);
Outcome<BoundaryValue>
encode_continuation_boundary_receipt(const ContinuationBoundaryReceipt &);
Outcome<ContinuationBoundaryReceipt>
decode_continuation_boundary_receipt(const BoundaryValue &);
// P17.5 explicit bridge cancellation invalidates only unexecuted continuation
// work. It never cancels the underlying original session, Action or contract.
struct CancelContinuationBoundary {
  StableId pending_id;
  Integer expected_control_epoch = 0, expected_pending_generation = 1;
};
struct ContinuationBoundaryCancelled {
  CancelContinuationBoundary request;
  Integer generation = 0;
  std::optional<SourceRef> stopped_slice_ref; // Original preceding Common
                                              // slice, if one was scheduled.
};
Status validate_continuation_boundary_cancelled_shape(
    const ContinuationBoundaryCancelled &);
Outcome<CancelContinuationBoundary>
decode_cancel_continuation_boundary(const BoundaryValue &);
Status visit_continuation_boundary_cancelled(CodecVisitor &,
                                             ContinuationBoundaryCancelled &);
Status emit_continuation_boundary_cancelled_fields(
    ProjectionWriter &, const ContinuationBoundaryCancelled &);
Outcome<BoundaryValue>
encode_continuation_boundary_cancelled(const ContinuationBoundaryCancelled &);
Outcome<ContinuationBoundaryCancelled>
decode_continuation_boundary_cancelled(const BoundaryValue &);
struct SelectContinuation {
  StableId pending_id;
  ContinuationCandidateRef candidate_ref;
  Integer expected_control_epoch = 0;
  bool voluntary_confirmation = false;
};

// Actual source reads are separate from the S0 routing/commit writer. In
// particular no current "available" boolean can reject an injured, captured
// or travelling actor whose original owner has a playable entry.
// 只借原控制与死亡地基；此接口不表示接续路径、本人同意或局部收束来源已齐。
class ContinuationFoundationSourceRead {
public:
  virtual ~ContinuationFoundationSourceRead() = default;
  virtual bool bound_to(const CommonStore &) const noexcept = 0;
  virtual Outcome<WorldMoment> actual_now() const = 0;
  virtual Status validate_initial_control_actor(const StableId &) const = 0;
  virtual Outcome<VerifiedDeathView> read_death(const DeathRef &) const = 0;
  virtual Outcome<std::optional<VerifiedDeathView>>
  read_actor_death(const StableId &) const = 0;
  virtual Status
  validate_pending_control_origin(const PendingContinuation &) const = 0;
};
class ContinuationOriginalSourceRead : public ContinuationFoundationSourceRead {
public:
  ~ContinuationOriginalSourceRead() override = default;
  virtual Status
  validate_voluntary_preparation(const PendingContinuation &) const = 0;
  virtual Status
  validate_consent_source(const ContinuationConsentSource &) const = 0;
  virtual Status validate_original_consent_registry(
      const ControlContinuationState &) const = 0;
  virtual Status
  validate_current_candidate(const PendingContinuation &,
                             const ContinuationCandidateRef &) const = 0;
  virtual Status
  validate_historical_activation(const PendingContinuation &,
                                 const ControlActivationRecord &) const = 0;
  virtual Status validate_historical_boundary_candidate(
      const PendingContinuation &,
      const ContinuationBoundaryReceipt &) const = 0;
  virtual Status validate_historical_retirement_withdrawal(
      const PendingContinuation &) const = 0;
};
// These operate only in the caller's private candidate. Any failure must abort
// that candidate, including an ID already allocated before a later failure.
Status initialize_control_continuation_in_candidate(
    const CommonStore &, ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &);
// Original S4 sources only. Control3 additionally requires central S2/S3 owner
// closure; catalog parsing is not admission of an incomplete formal profile.
Status validate_control_continuation(const CommonStore &,
                                     const ControlContinuationState &,
                                     const ControlContinuationCatalog &,
                                     const CatalogIdentity &,
                                     const ContinuationOriginalSourceRead &);
Status validate_control_continuation_v2(const CommonStore &,
                                        const ControlContinuationState &,
                                        const ControlContinuationCatalog &,
                                        const CatalogIdentity &,
                                        const ContinuationOriginalSourceRead &);
Status validate_control_continuation_v3(const CommonStore &,
                                        const ControlContinuationState &,
                                        const ControlContinuationCatalog &,
                                        const CatalogIdentity &,
                                        const ContinuationOriginalSourceRead &);
Outcome<std::optional<StableId>> open_after_death_continuation_in_candidate(
    CommonStore &, ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &,
    const ContinuationFoundationSourceRead &, const DeathRef &);
Outcome<StableId> open_voluntary_continuation_in_candidate(
    CommonStore &, ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &,
    const ContinuationOriginalSourceRead &, const CauseRef &actual_preparation);
// Shared original candidate gate for pure boundary reads and actual selection.
// The returned row is borrowed from this same state. It does not confirm
// voluntary retirement, change control, allocate a plan, or advance an owner.
Outcome<const PendingContinuation *> read_current_continuation_candidate(
    const CommonStore &, const ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &,
    const ContinuationOriginalSourceRead &, const ContinuationCandidateRef &);
// Pure S4 eligibility/source gate. It consumes the same original candidate as
// SelectContinuation; S0 must additionally read the actual S2/S3 safe boundary.
// The caller owns exact catalog admission. It does not run a local segment,
// select a Control3 profile, or treat a wire reference as authority.
Status validate_stabilize_continuation_candidate(
    const CommonStore &, const ControlContinuationState &,
    const ContinuationOriginalSourceRead &,
    const StabilizeContinuationBoundary &);
Status validate_select_continuation(const CommonStore &,
                                    const ControlContinuationState &,
                                    const ControlContinuationCatalog &,
                                    const CatalogIdentity &,
                                    const ContinuationOriginalSourceRead &,
                                    const SelectContinuation &);
// Original S0 records this after its actual route/control change, then binds
// the real result/history seal. It rechecks the immutable before-state/result;
// it does not itself change player_actor_id, routes, action, property or
// Office. Borrowed actual prefix after the same current-candidate gate. S0
// copies its count/header into receipt.candidate_prefix under the original K9
// copy lease before any owner mutation; no ID is fabricated to stand for this
// boundary.
Outcome<CurrentCommonFactPrefix> read_stabilize_continuation_candidate_prefix(
    const CommonStore &, const ControlContinuationState &,
    const ContinuationOriginalSourceRead &,
    const StabilizeContinuationBoundary &);
struct CommitResult;
// Pure borrowed preparation also lets central/K9 reserve the complete outgoing
// receipt before the writer copies request or previous actual result identity.
struct PreparedContinuationBoundaryCancellation {
  const PendingContinuation *pending = nullptr;
  const CommitResult *previous_slice = nullptr;
};
struct ContinuationBoundaryCancelledBuildView {
  const CancelContinuationBoundary &request;
  Integer generation;
  const StableId *stopped_slice_result_id = nullptr;
};
// This token covers only the original outgoing DTO, never a second World
// target charge. Keep it alive while Common makes its separately owned copy.
class ProducedContinuationBoundaryCancelled {
  friend Outcome<ProducedContinuationBoundaryCancelled>
  cancel_continuation_boundary_in_candidate(
      const CommonStore &, ControlContinuationState &,
      const ControlContinuationCatalog &, const CatalogIdentity &,
      const ContinuationOriginalSourceRead &, const StableId &,
      const CancelContinuationBoundary &);
  CheckpointMemoryCharge copied_fields_;
  ContinuationBoundaryCancelled receipt_;
  explicit ProducedContinuationBoundaryCancelled(
      const ContinuationBoundaryCancelledBuildView &);

public:
  ProducedContinuationBoundaryCancelled() = default;
  ProducedContinuationBoundaryCancelled(
      const ProducedContinuationBoundaryCancelled &) = default;
  ProducedContinuationBoundaryCancelled(
      ProducedContinuationBoundaryCancelled &&) noexcept = default;
  ProducedContinuationBoundaryCancelled &
  operator=(const ProducedContinuationBoundaryCancelled &) = delete;
  ProducedContinuationBoundaryCancelled &
  operator=(ProducedContinuationBoundaryCancelled &&) = delete;
  const ContinuationBoundaryCancelled &receipt() const noexcept {
    return receipt_;
  }
};
Outcome<PreparedContinuationBoundaryCancellation>
read_cancel_continuation_boundary(const CommonStore &,
                                  const ControlContinuationState &,
                                  const ControlContinuationCatalog &,
                                  const CatalogIdentity &,
                                  const ContinuationOriginalSourceRead &,
                                  const StableId &actual_actor,
                                  const CancelContinuationBoundary &);
Outcome<ProducedContinuationBoundaryCancelled>
cancel_continuation_boundary_in_candidate(
    const CommonStore &, ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &,
    const ContinuationOriginalSourceRead &, const StableId &actual_actor,
    const CancelContinuationBoundary &);
Outcome<const ContinuationBoundaryCancelled *>
read_actual_continuation_boundary_cancellation(const CommonStore &,
                                               const ControlContinuationState &,
                                               const SourceRef &);
// Replays only actual typed Common cancellation receipts up to a real command
// revision. No generation ledger is persisted alongside PendingContinuation.
Outcome<Integer> read_continuation_generation_at_revision(
    const CommonStore &, const ControlContinuationState &,
    const StableId &pending_id, Integer original_revision);

// Original S0 seal only: after allocating its real result ID/revision, before
// constructing immutable ReceiptDetails and inserting the original result and
// HistoryRecord. No empty/pending self-reference is a legal cold receipt.
Status bind_continuation_boundary_receipt_seal_in_candidate(
    const CommonStore &, ContinuationBoundaryReceipt &, const CommitResult &);
// Returns the exact typed payload borrowed from Common.results, never the
// caller's copy. This verifies the original seal/address and pending lineage,
// not historical candidate eligibility or the actual S2/S3 consequences.
// Common-only original seal reader for S3's own response source closure. It
// proves no pending/candidate authority; the overload below adds lineage.
Outcome<const ContinuationBoundaryReceipt *>
read_actual_continuation_boundary_receipt(const CommonStore &,
                                          const SourceRef &);
Outcome<const ContinuationBoundaryReceipt *>
read_actual_continuation_boundary_receipt(const CommonStore &,
                                          const ControlContinuationState &,
                                          const SourceRef &);
// Rechecks the original candidate at the captured prefix, even after later
// revocation/death. Actual S2/S3 consequences remain their owners' obligation.
Status validate_original_continuation_boundary_receipt(
    const CommonStore &, const ControlContinuationState &,
    const ContinuationOriginalSourceRead &, const SourceRef &);
// Enumerate references from that exact immutable payload for the original PIN
// integration. The sink must still dispatch each original owner/schema. In
// particular enumeration is not permission to omit historical source closure.
class ContinuationBoundarySourceSink {
public:
  virtual ~ContinuationBoundarySourceSink() = default;
  virtual Status
  sealed_command(const SourceRef &) = 0; // common/command_result/1
  virtual Status original_consequence(const MomentOwnerSource &) = 0;
  // Retain every actual Common fact in this prefix, not only its last anchor.
  virtual Status captured_candidate_prefix(const CommonActualFactPrefix &) = 0;
};
// Every persisted pending generation retains its actual cancellation chain,
// including cancellation before any local slice was ever scheduled.
Status visit_continuation_pending_generation_source_refs(
    const CommonStore &, const ControlContinuationState &,
    const StableId &pending_id, ContinuationBoundarySourceSink &);
Status visit_continuation_boundary_receipt_source_refs(
    const CommonStore &, const ControlContinuationState &, const SourceRef &,
    ContinuationBoundarySourceSink &);
Outcome<StableId> record_control_activation_in_candidate(
    CommonStore &, ControlContinuationState &,
    const ControlContinuationState &before, const SelectContinuation &,
    const ContinuationOriginalSourceRead &);
Status register_original_continuation_consent_in_candidate(
    const CommonStore &, ControlContinuationState &,
    const ContinuationOriginalSourceRead &, const ContinuationConsentSource &);
Outcome<StableId> bind_control_activation_result_in_candidate(
    CommonStore &, ControlContinuationState &,
    const ControlContinuationState &before, const SelectContinuation &,
    const CommitResult &, const ContinuationOriginalSourceRead &);
Outcome<const ControlActivationRecord *> read_control_activation_for_replay(
    const CommonStore &, const ControlContinuationState &,
    const ControlContinuationCatalog &, const CatalogIdentity &,
    const ContinuationOriginalSourceRead &, const StableId &predecessor,
    Integer request_seq);
} // namespace hanmo::resident
