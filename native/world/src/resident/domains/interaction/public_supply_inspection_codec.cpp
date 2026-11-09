// 此原生来源两种冷编码共用字段序；不在恢复时执行 Inspect 或补 Knowledge。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "public_supply_inspection.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
namespace hanmo::resident {
namespace {
// 每条 source 保精确读取 M 与原观察引用，字段缺失或多余一律拒绝。
template <class A> Status record(A &a, PublicSupplyInspection &r) {
  a.field("id", r.id);
  a.field("actor_id", r.actor_id);
  a.field("prop_id", r.prop_id);
  a.field("report_id", r.report_id);
  a.object("original_observation", [&](auto &x) {
    x.object("source_ref", [&](auto &s) {
      s.field("domain", r.original_observation.source_ref.domain);
      s.field("kind", r.original_observation.source_ref.kind);
      return s.field("id", r.original_observation.source_ref.id);
    });
    return x.field("source_schema", r.original_observation.source_schema);
  });
  a.field("source_key", r.source_key);
  a.field("settlement_id", r.settlement_id);
  a.field("place_id", r.place_id);
  a.object("inspected_at", [&](auto &x) {
    return dynamic_codec::exact_moment(x, r.inspected_at);
  });
  return a.status();
}
// 同一 visitor 负责原表保存、恢复及保留量普查；没有另一个 source 索引账。
template <class A> Status state_fields(A &a, PublicSupplyInspectionState &s) {
  a.field("version", s.version);
  if (s.version != 1)
    return a.reject("UnsupportedPublicSupplyInspectionVersion");
  return dynamic_codec::table(a, "results", s.results,
                              [](auto &x, auto &r) { return record(x, r); });
}
} // namespace
Outcome<PublicSupplyInspectionCatalog>
decode_public_supply_inspection_catalog(const BoundaryValue &value) {
  PublicSupplyInspectionCatalog catalog;
  dynamic_codec::JsonArchive a(value);
  a.field("version", catalog.version);
  if (catalog.version != 1)
    return {false,
            {},
            common_failure("UnsupportedPublicSupplyInspectionVersion", {},
                           ErrorKind::unsupported_capability)
                .error};
  auto valid = dynamic_codec::table(
      a, "notice_sources", catalog.notice_sources, [](auto &x, auto &source) {
        return x.field("supply_source_key", source);
      });
  if (valid.ok)
    valid = a.finish();
  return valid.ok
             ? Outcome<PublicSupplyInspectionCatalog>{true,
                                                      std::move(catalog),
                                                      {}}
             : Outcome<PublicSupplyInspectionCatalog>{false, {}, valid.error};
}
Outcome<PublicSupplyInspectionState>
decode_public_supply_inspection_state(const BoundaryValue &value) {
  PublicSupplyInspectionState state;
  dynamic_codec::JsonArchive a(value);
  auto valid = state_fields(a, state);
  if (valid.ok)
    valid = a.finish();
  return valid.ok
             ? Outcome<PublicSupplyInspectionState>{true, std::move(state), {}}
             : Outcome<PublicSupplyInspectionState>{false, {}, valid.error};
}
// 借外层同一冷读任务及当前 section，原字段访问器直接读这一个输入节点。
Outcome<PublicSupplyInspectionState>
decode_public_supply_inspection_state(const BoundaryValue &value,
                                      ScopedCheckpointJsonDecode &scope) {
  if (ScopedCheckpointJsonDecode::current() != &scope)
    return {false,
            {},
            common_failure("PublicSupplyInspectionSameJsonCandidateRequired",
                           {}, ErrorKind::internal)
                .error};
  scope.default_values<PublicSupplyInspectionState>();
  return decode_public_supply_inspection_state(value);
}
// 精确导出只借原字段写出，不构造第二份 BoundaryValue 树。
Status
emit_public_supply_inspection_state(ProjectionWriter &writer,
                                    const PublicSupplyInspectionState &state) {
  DynamicProjectionArchive archive(writer);
  return state_fields(archive,
                      const_cast<PublicSupplyInspectionState &>(state));
}
// Common 只导出原事实头；记录正文唯一保存在本领域 section，不复制第二份。
Status emit_public_supply_inspection_fact_payloads_fields(
    ProjectionWriter &writer, const PublicSupplyInspectionState &state) {
  if (state.version != 1)
    return writer.reject("UnsupportedPublicSupplyInspectionVersion");
  for (const auto &row : state.results.rows()) {
    if (row.first != row.second.id || row.first.empty())
      return writer.reject("PublicSupplyInspectionSourceKeyMismatch");
    auto valid = writer.object(row.first, [&](auto &child) {
      child.text("kind", public_supply_inspection_source_kind);
      return child.text("domain", "interaction");
    });
    if (!valid.ok)
      return valid;
  }
  return writer.status();
}
Outcome<BoundaryValue> encode_public_supply_inspection_state(
    const PublicSupplyInspectionState &state) {
  BoundaryValue value;
  BoundaryProjectionWriter writer(value);
  auto valid = writer.object("", [&](auto &child) {
    return emit_public_supply_inspection_state(child, state);
  });
  if (valid.ok)
    valid = writer.finish();
  return valid.ok ? Outcome<BoundaryValue>{true, std::move(value), {}}
                  : Outcome<BoundaryValue>{false, {}, valid.error};
}
Status
visit_public_supply_inspection_checkpoint(CodecVisitor &a,
                                          PublicSupplyInspectionState &s) {
  return state_fields(a, s);
}
} // namespace hanmo::resident
