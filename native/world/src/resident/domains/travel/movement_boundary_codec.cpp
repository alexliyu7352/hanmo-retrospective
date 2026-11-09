#include "movement_boundary_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Status visit_movement_boundary_checkpoint(
    CodecVisitor &codec, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<MovementBoundaryContact> pending;
    take(movement_boundary_fields::root(codec, state.boundary_contacts, pending,
                                        selected, selection));
    take(codec.status());
    if (codec.reading())
      take(transport_physical_table::adopt(state.boundary_contacts, pending,
                                           "boundary_contacts"));
  });
}
Status emit_movement_boundary_projection(
    ProjectionWriter &sink, std::string_view name,
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    if (selected != movement_boundary_contacts_codec_revision)
      fail("UnsupportedMovementBoundaryPhysicalRevision");
    transport_physical_table::DecodedRows<MovementBoundaryContact> pending;
    // The original projection archive is write-only; its canonical table
    // traversal borrows existing rows, with no whole-root deep copy.
    auto &table = const_cast<OrderedTable<MovementBoundaryContact> &>(
        state.boundary_contacts);
    take(sink.object(name, [&](auto &writer) {
      ActorFoundationProjectionArchive codec(writer);
      return movement_boundary_fields::root(codec, table, pending, selected,
                                            selection);
    }));
    take(sink.status());
  });
}
Outcome<BoundaryValue> encode_movement_boundary_contacts(
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return guard<BoundaryValue>([&] {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    take(emit_movement_boundary_projection(writer, "", state, selected,
                                           selection));
    take(writer.finish());
    return value;
  });
}
Status decode_movement_boundary_contacts(
    const BoundaryValue &value, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<MovementBoundaryContact> pending;
    ActorFoundationJsonVisitor codec(value);
    take(movement_boundary_fields::root(codec, state.boundary_contacts, pending,
                                        selected, selection));
    take(codec.finish());
    take(transport_physical_table::adopt(state.boundary_contacts, pending,
                                         "boundary_contacts"));
  });
}
} // namespace hanmo::resident
