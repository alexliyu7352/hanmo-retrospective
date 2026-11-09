#include "local_move_codec_fields.hpp"
#include "movement_exact_detail.hpp"
#include "resident/checkpoint/projection_writer.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident::transport_decoded_transfer {
// 本独立段显式承担含真实 WorldMoment 的可抛行转移；失败撤候选，不放宽公共静断或伪称 noexcept。
template<> struct OriginalRow<LocalMoveSource> : std::true_type {};
}
namespace hanmo::resident {
using namespace movement_detail;
Status visit_local_move_checkpoint(
    CodecVisitor &codec, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<LocalMoveSource>
        pending;
    take(local_move_fields::root(
        codec, state.local_moves, pending, selected, selection));
    take(codec.status());
    if (codec.reading())
      take(transport_physical_table::adopt(state.local_moves,
                                           pending, "local_moves"));
  });
}
Status emit_local_move_projection(
    ProjectionWriter &sink, std::string_view name,
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    if (selected != local_move_codec_revision)
      fail("UnsupportedLocalMovePhysicalRevision");
    transport_physical_table::DecodedRows<LocalMoveSource>
        pending;
    // 原投影仅写，按规范键顺序借读真实表，不深拷贝整个根。
    auto &table =
        const_cast<OrderedTable<LocalMoveSource> &>(
            state.local_moves);
    take(sink.object(name, [&](auto &writer) {
      ActorFoundationProjectionArchive codec(writer);
      return local_move_fields::root(codec, table, pending,
                                                         selected, selection);
    }));
    take(sink.status());
  });
}
Outcome<BoundaryValue> encode_local_moves(
    const MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return guard<BoundaryValue>([&] {
    BoundaryValue value;
    BoundaryProjectionWriter writer(value);
    take(emit_local_move_projection(writer, "", state,
                                                        selected, selection));
    take(writer.finish());
    return value;
  });
}
Status decode_local_moves(
    const BoundaryValue &value, MovementExactState &state, Integer selected,
    const MovementRecordCodecSelection &selection) {
  return status([&] {
    transport_physical_table::DecodedRows<LocalMoveSource>
        pending;
    ActorFoundationJsonVisitor codec(value);
    take(local_move_fields::root(
        codec, state.local_moves, pending, selected, selection));
    take(codec.finish());
    take(transport_physical_table::adopt(state.local_moves, pending,
                                         "local_moves"));
  });
}
} // namespace hanmo::resident
