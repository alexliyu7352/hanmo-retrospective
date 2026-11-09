// Final V physical roots. Internal MovementExact visitors are not these sections.
#pragma once
#include "movement_physical_codec_fields.hpp"
#include "transport_payment_physical.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// The restore job reserves the complete original deep-copy plan before each
// runtime basis copy. The reservation belongs to the decoded world owner.
class MovementBasisDecodeAllocation {
public:
 virtual ~MovementBasisDecodeAllocation()=default;
 virtual Status reserve_original_basis_copy(const MovementBasis&)=0;
};
struct TransportPhysicalCodecSelection {
 Integer travel_version=4;
 Integer group_transport_version=0; // absent, or exactly 4/5/6
 Integer transport_manifest_bindings_version=0; // absent, or exactly 2
 CommonResourceCodecContext common_resources;
 bool carrier_location_enabled=false,typed_reservations=false,domain_activity_reservations=false;
};
enum class TransportPhysicalSection {travel,group_transport,transport_manifest_bindings};
// All pointers borrow this same world candidate. Selection comes from its locked
// catalog, never from optional presence or the contents of a decoded table.
struct TransportPhysicalCodecRoots {
 TravelState& travel;MovementExactState& movement;
 SpatialCarrierState* carriers=nullptr;
 GroupMovementExecutionState* executions=nullptr;
 TransportRegistrationState* registrations=nullptr;
 EconomicTransportExpenseState* payments=nullptr;
};
class TransportPhysicalCodecContext:public CheckpointAllocatedObject {
public:
 struct State;
 explicit TransportPhysicalCodecContext(std::unique_ptr<State>);
 ~TransportPhysicalCodecContext();
 TransportPhysicalCodecContext(const TransportPhysicalCodecContext&)=delete;
 TransportPhysicalCodecContext& operator=(const TransportPhysicalCodecContext&)=delete;
 State& state()noexcept;
private:std::unique_ptr<State> state_;
};
// A context spans exactly one complete encode/decode traversal of the selected
// three physical sections. It owns only codec indices and decode temporaries.
// Decode does not hydrate totals, replay an action or call a business writer.
Outcome<std::unique_ptr<TransportPhysicalCodecContext>> make_transport_physical_codec_context(
 const CommonStore&,TransportPhysicalCodecRoots,const TransportPhysicalCodecSelection&,
 bool reading,ExactArithmeticBudget={},MovementBasisDecodeAllocation* original_basis_allocation=nullptr);
// A fresh write-only traversal for one actual selected physical section. Used
// independently by stream measurement and stream output. It validates/indexes
// all selected original roots, but emits no other section and keeps no progress
// across calls. The matching visit/emit below and finish are still mandatory.
Outcome<std::unique_ptr<TransportPhysicalCodecContext>> make_transport_physical_section_write_context(
 const CommonStore&,TransportPhysicalCodecRoots,const TransportPhysicalCodecSelection&,
 TransportPhysicalSection,ExactArithmeticBudget={});
Status visit_travel_physical_checkpoint(CodecVisitor&,TransportPhysicalCodecContext&);
Status visit_group_transport_physical_checkpoint(CodecVisitor&,TransportPhysicalCodecContext&);
Status visit_transport_manifest_bindings_physical_checkpoint(CodecVisitor&,TransportPhysicalCodecContext&);
// The travel projection emits only the new exact-section fields. The outer
// world writer merges the unchanged original Travel action_params/facts/itineraries;
// every new table is an explicit ordered [{key,fields}] array. It shares the
// same typed record callback with binary, not a binary-to-JSON conversion.
Status emit_travel_physical_projection(ProjectionWriter&,std::string_view,TransportPhysicalCodecContext&);
Status emit_group_transport_physical_projection(ProjectionWriter&,std::string_view,TransportPhysicalCodecContext&);
Status emit_transport_manifest_bindings_physical_projection(ProjectionWriter&,std::string_view,TransportPhysicalCodecContext&);
Outcome<BoundaryValue> encode_travel_physical(TransportPhysicalCodecContext&);
Outcome<BoundaryValue> encode_group_transport_physical(TransportPhysicalCodecContext&);
Outcome<BoundaryValue> encode_transport_manifest_bindings_physical(TransportPhysicalCodecContext&);
Status decode_travel_physical(const BoundaryValue& original_world_seed,const BoundaryValue& exact_section,TransportPhysicalCodecContext&);
Status decode_group_transport_physical(const BoundaryValue&,TransportPhysicalCodecContext&);
Status decode_transport_manifest_bindings_physical(const BoundaryValue&,TransportPhysicalCodecContext&);
// Called after all selected sections, or the one explicit write-only section.
// It proves physical one-owner coverage
// and receipt exhaustiveness. The outer restore then calls the original pure
// manifest hydrate and all complete cold validators before adopting the world.
Status finish_transport_physical_codec(TransportPhysicalCodecContext&);
}
