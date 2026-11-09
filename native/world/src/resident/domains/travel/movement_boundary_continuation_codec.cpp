#include "movement_boundary_continuation_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Status visit_movement_boundary_continuation_checkpoint(
    CodecVisitor &codec, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<MovementBoundaryContinuationSource>
        pending;
    take(movement_boundary_continuation_fields::root(
        codec, state.boundary_continuations, pending, selected, selection));
    take(codec.status());
    if (codec.reading())
      take(transport_physical_table::adopt(state.boundary_continuations,
                                           pending, "boundary_continuations"));
  });
}
Status emit_movement_boundary_continuation_projection(
    ProjectionWriter &sink, std::string_view name,
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    if (selected != movement_boundary_continuations_codec_revision)
      fail("UnsupportedMovementBoundaryContinuationPhysicalRevision");
    transport_physical_table::DecodedRows<MovementBoundaryContinuationSource>
        pending;
    // The original projection archive is write-only; its canonical table
    // traversal borrows existing rows, with no whole-root deep copy.
    auto &table =
        const_cast<OrderedTable<MovementBoundaryContinuationSource> &>(
            state.boundary_continuations);
    take(sink.object(name, [&](auto &writer) {
      ActorFoundationProjectionArchive codec(writer);
      return movement_boundary_continuation_fields::root(codec, table, pending,
                                                         selected, selection);
    }));
    take(sink.status());
  });
}
Outcome<BoundaryValue> encode_movement_boundary_continuations(
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return guard<BoundaryValue>([&] {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    take(emit_movement_boundary_continuation_projection(writer, "", state,
                                                        selected, selection));
    take(writer.finish());
    return value;
  });
}
Status decode_movement_boundary_continuations(
    const BoundaryValue &value, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<MovementBoundaryContinuationSource>
        pending;
    ActorFoundationJsonVisitor codec(value);
    take(movement_boundary_continuation_fields::root(
        codec, state.boundary_continuations, pending, selected, selection));
    take(codec.finish());
    take(transport_physical_table::adopt(state.boundary_continuations, pending,
                                         "boundary_continuations"));
  });
}
} // namespace hanmo::resident
