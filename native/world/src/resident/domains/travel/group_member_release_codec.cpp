#include "group_member_release_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
using namespace movement_detail;
Status visit_group_member_release_checkpoint(
    CodecVisitor &codec, GroupMovementExecutionState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<GroupContactMemberReleaseSource>
        pending;
    take(group_member_release_fields::root(codec, state.contact_member_releases,
                                           pending, selected, selection));
    take(codec.status());
    if (codec.reading())
      take(transport_physical_table::adopt(state.contact_member_releases,
                                           pending, "contact_member_releases"));
  });
}
Status emit_group_member_release_projection(
    ProjectionWriter &sink, std::string_view name,
    const GroupMovementExecutionState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    if (selected != movement_group_member_releases_codec_revision)
      fail("UnsupportedGroupMemberReleasePhysicalRevision");
    transport_physical_table::DecodedRows<GroupContactMemberReleaseSource>
        pending;
    // The original projection archive is write-only; its canonical table
    // traversal borrows existing rows, with no whole-root deep copy.
    auto &table = const_cast<OrderedTable<GroupContactMemberReleaseSource> &>(
        state.contact_member_releases);
    take(sink.object(name, [&](auto &writer) {
      ActorFoundationProjectionArchive codec(writer);
      return group_member_release_fields::root(codec, table, pending, selected,
                                               selection);
    }));
    take(sink.status());
  });
}
Outcome<BoundaryValue>
encode_group_member_releases(const GroupMovementExecutionState &state,
                             Integer selected,
                             const MovementRecordCodecSelection &selection) {
  return guard<BoundaryValue>([&] {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    take(emit_group_member_release_projection(writer, "", state, selected,
                                              selection));
    take(writer.finish());
    return value;
  });
}
Status decode_group_member_releases(
    const BoundaryValue &value, GroupMovementExecutionState &state,
    Integer selected, const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<GroupContactMemberReleaseSource>
        pending;
    ActorFoundationJsonVisitor codec(value);
    take(group_member_release_fields::root(codec, state.contact_member_releases,
                                           pending, selected, selection));
    take(codec.finish());
    take(transport_physical_table::adopt(state.contact_member_releases, pending,
                                         "contact_member_releases"));
  });
}
} // namespace hanmo::resident
