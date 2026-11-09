#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "military_fortification_node_codec.hpp"
#include "military_fortification_node_codec_fields.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
#include "resident/domains/travel/movement_exact_detail.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"

namespace hanmo::resident {
// 职责：所有新行仍由同一个实际 JSON job 收费；顺序错误不回退旧 parser。
Outcome<MilitaryFortificationNodes>
decode_military_fortification_nodes(const BoundaryValue &input) {
  auto *budget = ScopedCheckpointJsonDecode::current();
  if (!budget)
    return {false,
            {},
            common_failure("CheckpointJsonContextRequired",
                           "dynamic_military/fortification_nodes",
                           ErrorKind::internal)
                .error};
  return movement_detail::guard<MilitaryFortificationNodes>([&] {
    budget->default_values<MilitaryFortificationNodes>();
    MilitaryFortificationNodes nodes;
    ActorFoundationJsonVisitor original(input);
    movement_detail::take(original.root_sequence(
        nodes.rows, "fortification_nodes", [](auto &c, auto &row) {
          c.ordered_fields();
          return military_fortification_node_fields::value(c, row);
        }));
    movement_detail::take(original.finish());
    movement_detail::take(validate_military_fortification_node_order(nodes));
    return nodes;
  });
}
// 职责：验证结构后发原数组/六槽，不修改输入排序或内容。
Status
emit_military_fortification_nodes(ProjectionWriter &sink, std::string_view name,
                                  const MilitaryFortificationNodes &nodes) {
  auto valid = validate_military_fortification_node_order(nodes);
  if (!valid.ok)
    return valid;
  return sink.sequence(name, nodes.rows, [](auto &array, const auto &row) {
    return array.object("", [&](auto &fields) {
      ActorFoundationProjectionArchive original(fields);
      return military_fortification_node_fields::value(
          original, const_cast<FortificationNodeState &>(row));
    });
  });
}
// 职责：原 Boundary sink 仅作导出，持久根没有第二份 JSON 权威。
Outcome<BoundaryValue>
encode_military_fortification_nodes(const MilitaryFortificationNodes &nodes) {
  return movement_detail::guard<BoundaryValue>([&] {
    BoundaryValue out;
    BoundaryProjectionWriter sink(out);
    movement_detail::take(emit_military_fortification_nodes(sink, "", nodes));
    movement_detail::take(sink.finish());
    return out;
  });
}
// 职责：使用原有界 typed 序列，不从 payload 猜 schema 或补空未知行。
Status visit_military_fortification_nodes(CodecVisitor &codec,
                                          MilitaryFortificationNodes &nodes) {
  if (!codec.reading()) {
    auto valid = validate_military_fortification_node_order(nodes);
    if (!valid.ok)
      return valid;
  }
  auto result = codec.sequence(
      "fortification_nodes", nodes.rows, [](auto &child, auto &row) {
        return child.object("", [&](auto &fields) {
          return military_fortification_node_fields::value(fields, row);
        });
      });
  if (!result.ok)
    return result;
  return validate_military_fortification_node_order(nodes);
}
}
