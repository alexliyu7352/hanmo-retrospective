// P21.1 original entry entitlement, adopted only after the true positive S1
// activity made the original BoundaryCrossingReceipt physically verifiable.
#pragma once
#include "treaty_termination_sources.hpp"
#include "resident/domains/travel/transport_manifest_history.hpp"
#include "resident/domains/travel/group_movement.hpp"
namespace hanmo::resident {
struct SafeExitNamedSubjects {std::vector<StableId> actor_or_member_refs;};
struct SafeExitCivilianSelectionScope {SourceRef population_selection_or_entry_ref;std::vector<SourceRef> current_successor_refs;};
using SafeExitSubjectScope=std::variant<SafeExitNamedSubjects,SafeExitCivilianSelectionScope>;
struct SafeExitCargoScopeRef {SourceRef entry_manifest_ref;StableId inventory_id;};
enum class SafeExitLineState {inside,exiting,outside,expired,superseded};
struct SafeExitLine {
 StableId line_id,binding_id;SourceRef entitlement_source_ref;SafeExitSubjectScope subject_scope;
 std::vector<SafeExitCargoScopeRef> cargo_scope_refs;std::optional<StableId> chosen_path_id;
 Integer last_exit_boundary_ordinal=0;SafeExitLineState state=SafeExitLineState::inside;
 std::vector<SourceRef> actual_crossing_refs;
};
enum class SafeExitBindingState {inside,exiting,outside,expired};
struct SafeExitBinding {
 StableId id,treaty_id;Integer terms_revision=0;StableId term_id;
 SourceRef entry_receipt_ref;SpatialCarrierRef entry_carrier;SourceRef entered_manifest_basis;
 std::vector<SubjectRef> entered_subject_refs;SourceRef exit_policy_ref;
 SafeExitBindingState state=SafeExitBindingState::inside;
 // Exact derived projection. Originals are the actual termination source and
 // the first real exit crossing; no independently writable clock is allowed.
 std::optional<SourceRef> termination_source;
 std::optional<WorldMoment> termination_at,exit_from,exit_until;
 std::vector<StableId> exit_line_refs;std::vector<SourceRef> manifest_successor_refs;
 std::optional<SourceRef> last_result_ref;
};
// Subobject of the sole original M-PASSAGE body. It is not another World root.
struct SafeExitState {OrderedTable<SafeExitBinding> bindings;OrderedTable<SafeExitLine> lines;};
struct SafeExitEntryTerm {
 SourceRef actual_activation;StableId term_id;
 // Actual original adopted Decision identifies exactly this original term.
 // The current term alone is insufficient to prove that entry was authorized.
 SourceRef actual_decision;
};
struct SafeExitInitialLine {
 SourceRef entitlement_source_ref;SafeExitSubjectScope subject_scope;
 std::vector<SafeExitCargoScopeRef> cargo_scope_refs;
};
struct SafeExitEntryPartition {std::vector<SubjectRef> entered_subjects;std::vector<SafeExitInitialLine> lines;};
class SafeExitEntryOriginalRead {public:virtual ~SafeExitEntryOriginalRead()=default;
 // null means the actual adopted Decision is explicitly not TreatyPassage.
 // Missing/corrupt decision or required term data is an error, never null.
 virtual Outcome<std::optional<SafeExitEntryTerm>> read_actual_safe_exit_entry_term(
  const BoundaryCrossingReceipt&)const=0;
 // Whole actual immutable entry manifest, including military original member
 // identities and original cargo rights, is partitioned by the original owners.
 // No count-only invented allocation or arbitrary first-line cargo assignment.
 virtual Outcome<SafeExitEntryPartition> read_original_safe_exit_entry_partition(
  const BoundaryCrossingReceipt&,const TransportEntryManifestSource&,
  const TreatyPassageTerm&)const=0;
 virtual Status validate_original_safe_exit_entry_partition(
  const BoundaryCrossingReceipt&,const TransportEntryManifestSource&,
  const TreatyPassageTerm&,const SafeExitEntryPartition&)const=0;
 // Full retained successor/crossing history belongs to the original S1/S2/S5
 // producers. The initial-entry reader never treats today's manifest as it.
 // This source leg verifies original lineage/decisions/path and true final-edge
 // completion, but must not call the S6 derived-window/full-entry reader back.
 virtual Status validate_safe_exit_retained_progress(const SafeExitBinding&,
  const OrderedTable<SafeExitLine>&)const=0;
 // Actual adoption leg, before S1 writes the crossing back-reference. It must
 // prove this immutable manifest is precisely the selected lawful live scopes,
 // original cargo/successors, and the actual authorized path/start or switch.
 // It must not rely on the not-yet-written S6 progress or future S1 back-reference.
 virtual Status validate_actual_safe_exit_exit_adoption(const SafeExitBinding&,
  const SafeExitPassageContext&,const BoundaryCrossingReceipt&,
  const TransportEntryManifestSource&)const=0;
};
struct SafeExitEntryContext {
 const CommonStore& common;const SpatialCarrierState& carriers;
 const MovementExactState& movement;const GroupMovementExecutionState& group_execution;
 const TravelCatalog& travel_catalog;const TransportManifestHistorySourceRead& manifest_history;
 const TreatyTerminationContext& treaties;const SafeExitState& state;
 const SafeExitEntryOriginalRead& originals;const OrganizationAuthorityClock& clock;
 ExactArithmeticBudget arithmetic;
};
// Original-source leg only: entry/manifest/activation/actual binding identity.
// Consumers use it to replay derived progress without recursing through the
// full SafeExit reader. It is not a certificate for retained line progress.
struct SafeExitOriginalEntryView {
 const SafeExitBinding* binding=nullptr;const BoundaryCrossingReceipt* crossing=nullptr;
 const TransportEntryManifestSource* manifest=nullptr;TreatyActivationView activation;
 const TreatyPassageTerm* term=nullptr;
};
Outcome<SafeExitOriginalEntryView> read_safe_exit_original_entry(
 const SafeExitEntryContext&,const SourceRef& binding_source,Integer schema);
SourceRef safe_exit_binding_source(const SafeExitBinding&);
Status validate_safe_exit_binding_shape(const SafeExitBinding&);
Status validate_safe_exit_line_shape(const SafeExitLine&);
Outcome<const SafeExitBinding*> read_safe_exit_entry_binding(const SafeExitEntryContext&,const SourceRef&,Integer schema);
Status validate_safe_exit_entries(const SafeExitEntryContext&);
// now is the trusted post-positive-prefix M_to; crossing.entry_at is original
// M_from. S1 remains sole writer of crossing.safe_exit_binding_ref, after this
// result, before the same candidate publishes. Error discards that candidate.
Outcome<std::optional<SourceRef>> adopt_safe_exit_after_positive_crossing_in_candidate(
 CommonStore&,SafeExitState&,const SafeExitEntryContext&,const SourceRef& actual_crossing);
Status visit_safe_exit_binding(CodecVisitor&,SafeExitBinding&);
Status visit_safe_exit_line(CodecVisitor&,SafeExitLine&);
Status emit_safe_exit_binding_projection(ProjectionWriter&,std::string_view,const SafeExitBinding&);
Status emit_safe_exit_line_projection(ProjectionWriter&,std::string_view,const SafeExitLine&);
Outcome<BoundaryValue> encode_safe_exit_binding(const SafeExitBinding&);
Outcome<SafeExitBinding> decode_safe_exit_binding(const BoundaryValue&);
Outcome<BoundaryValue> encode_safe_exit_line(const SafeExitLine&);
Outcome<SafeExitLine> decode_safe_exit_line(const BoundaryValue&);

}
