#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
#include "military_spatial_codec.hpp"
#include "military_spatial_codec_fields.hpp"
#include "military_state_codec_archive.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
namespace hanmo::resident {
namespace {
struct OriginalMilitarySpatialKeys {
  template <class T>
  static constexpr bool owns = std::is_same_v<T, MilitarySpatialChart2> ||
                               std::is_same_v<T, MilitarySpatialHandoff2>;
  template <class T> bool operator()(const StableId &key, const T &row) const {
    return !key.empty() && key == row.id;
  }
};
template <class T>
Status table(CodecVisitor &c, std::string_view name, OrderedTable<T> &rows) {
  military_state_codec::RootArchive<CodecVisitor, OriginalMilitarySpatialKeys>
      root{c, "MilitarySpatialPayloadKeyMismatch",
           "MilitarySpatialPayloadRootAlreadyPopulated"};
  return root.table(name, rows, [](auto &body, auto &row) {
    return military_spatial_fields::value(body, row);
  });
}
template <class T> Status visit(CodecVisitor &c, T &row) {
  return movement_detail::status([&] {
    try {
      movement_detail::take(military_spatial_fields::value(c, row));
    } catch (const exact_fields::CodecFailure &e) {
      throw movement_detail::Failure{e.error};
    }
  });
}
template <class T> Outcome<T> decode(const BoundaryValue &raw) {
  return movement_detail::guard<T>([&] {
    try {
      ActorFoundationJsonVisitor c(raw);
      movement_detail::take(prepare_checkpoint_default_values<T>(c));
      T row;
      movement_detail::take(military_spatial_fields::value(c, row));
      movement_detail::take(c.finish());
      return row;
    } catch (const exact_fields::CodecFailure &e) {
      throw movement_detail::Failure{e.error};
    }
  });
}
template <class T> Status fields(ProjectionWriter &sink, const T &row) {
  return movement_detail::status([&] {
    try {
      ActorFoundationProjectionArchive c(sink);
      movement_detail::take(
          military_spatial_fields::value(c, const_cast<T &>(row)));
    } catch (const exact_fields::CodecFailure &e) {
      throw movement_detail::Failure{e.error};
    }
  });
}
template <class T>
Status emit(ProjectionWriter &sink, std::string_view name, const T &row) {
  return sink.object(name,
                     [&](ProjectionWriter &body) { return fields(body, row); });
}
template <class T> Outcome<BoundaryValue> encode(const T &row) {
  return movement_detail::guard<BoundaryValue>([&] {
    BoundaryValue out;
    BoundaryProjectionWriter sink(out);
    movement_detail::take(emit(sink, "", row));
    movement_detail::take(sink.finish());
    return out;
  });
}
} // namespace
Status visit_military_spatial_chart2(CodecVisitor &c,
                                     MilitarySpatialChart2 &v) {
  return visit(c, v);
}
Status visit_military_spatial_handoff2(CodecVisitor &c,
                                       MilitarySpatialHandoff2 &v) {
  return visit(c, v);
}
// 此二进制定义 visit 的唯一外链实现保留在 movement_exact_codec.cpp。
Status
visit_military_entry_connector_definition(CodecVisitor &c,
                                          MilitaryEntryConnectorDefinition &v) {
  return visit(c, v);
}
Outcome<MilitarySpatialChart2>
decode_military_spatial_chart2(const BoundaryValue &v) {
  return decode<MilitarySpatialChart2>(v);
}
Outcome<MilitarySpatialHandoff2>
decode_military_spatial_handoff2(const BoundaryValue &v) {
  return decode<MilitarySpatialHandoff2>(v);
}
Outcome<MilitarySpatialGeometryDefinition2>
decode_military_spatial_definition2(const BoundaryValue &v) {
  return decode<MilitarySpatialGeometryDefinition2>(v);
}
Outcome<MilitaryEntryConnectorDefinition>
decode_military_entry_connector_definition(const BoundaryValue &v) {
  return decode<MilitaryEntryConnectorDefinition>(v);
}
Outcome<BoundaryValue>
encode_military_spatial_chart2(const MilitarySpatialChart2 &v) {
  return encode(v);
}
Outcome<BoundaryValue>
encode_military_spatial_handoff2(const MilitarySpatialHandoff2 &v) {
  return encode(v);
}
Outcome<BoundaryValue> encode_military_spatial_definition2(
    const MilitarySpatialGeometryDefinition2 &v) {
  return encode(v);
}
Outcome<BoundaryValue> encode_military_entry_connector_definition(
    const MilitaryEntryConnectorDefinition &v) {
  return encode(v);
}
Status emit_military_spatial_chart2(ProjectionWriter &w, std::string_view n,
                                    const MilitarySpatialChart2 &v) {
  return emit(w, n, v);
}
Status emit_military_spatial_handoff2(ProjectionWriter &w, std::string_view n,
                                      const MilitarySpatialHandoff2 &v) {
  return emit(w, n, v);
}
Status
emit_military_spatial_definition2(ProjectionWriter &w, std::string_view n,
                                  const MilitarySpatialGeometryDefinition2 &v) {
  return emit(w, n, v);
}
Status emit_military_entry_connector_definition(
    ProjectionWriter &w, std::string_view n,
    const MilitaryEntryConnectorDefinition &v) {
  return emit(w, n, v);
}
Status emit_military_spatial_chart2_fields(ProjectionWriter &w,
                                           const MilitarySpatialChart2 &v) {
  return fields(w, v);
}
Status emit_military_spatial_handoff2_fields(ProjectionWriter &w,
                                             const MilitarySpatialHandoff2 &v) {
  return fields(w, v);
}
Status emit_military_spatial_definition2_fields(
    ProjectionWriter &w, const MilitarySpatialGeometryDefinition2 &v) {
  return fields(w, v);
}
Status emit_military_entry_connector_definition_fields(
    ProjectionWriter &w, const MilitaryEntryConnectorDefinition &v) {
  return fields(w, v);
}
// 此根载荷 visit 同样保留原 movement 实现；本文件的严格 JSON 读写不变。
} // namespace hanmo::resident
