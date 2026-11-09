#include "group_member_loss_handoff_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Status visit_group_member_loss_handoff_checkpoint(
    CodecVisitor &codec, GroupMovementExecutionState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<GroupMemberLossHandoffSource> pending;
    take(group_member_loss_fields::root(codec, state.member_loss_handoffs,
                                        pending, selected, selection));
    take(codec.status());
    if (codec.reading())
      take(transport_physical_table::adopt(state.member_loss_handoffs, pending,
                                           "member_loss_handoffs"));
  });
}
Status emit_group_member_loss_handoff_projection(
    ProjectionWriter &sink, std::string_view name,
    const GroupMovementExecutionState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    if (selected != movement_group_member_losses_codec_revision)
      fail("UnsupportedGroupMemberLossPhysicalRevision");
    transport_physical_table::DecodedRows<GroupMemberLossHandoffSource> pending;
    // The original projection archive is write-only; its canonical table
    // traversal borrows existing rows, with no whole-root deep copy.
    auto &table = const_cast<OrderedTable<GroupMemberLossHandoffSource> &>(
        state.member_loss_handoffs);
    take(sink.object(name, [&](auto &writer) {
      ActorFoundationProjectionArchive codec(writer);
      return group_member_loss_fields::root(codec, table, pending, selected,
                                            selection);
    }));
    take(sink.status());
  });
}
Outcome<BoundaryValue> encode_group_member_loss_handoffs(
    const GroupMovementExecutionState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return guard<BoundaryValue>([&] {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    take(emit_group_member_loss_handoff_projection(writer, "", state, selected,
                                                   selection));
    take(writer.finish());
    return value;
  });
}
Status decode_group_member_loss_handoffs(
    const BoundaryValue &value, GroupMovementExecutionState &state,
    Integer selected, const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<GroupMemberLossHandoffSource> pending;
    ActorFoundationJsonVisitor codec(value);
    take(group_member_loss_fields::root(codec, state.member_loss_handoffs,
                                        pending, selected, selection));
    take(codec.finish());
    take(transport_physical_table::adopt(state.member_loss_handoffs, pending,
                                         "member_loss_handoffs"));
  });
}
} // namespace hanmo::resident
