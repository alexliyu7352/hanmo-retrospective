// 本 TU 显式声明边界值，避免依赖共享头的偶然包含顺序。
namespace hanmo::resident { struct BoundaryValue; }
// 只投影真实认可来源，不通过扫描Common头自行补业务载荷；禁用profile也拒残留认可头。
#include "collateral_control_payloads.hpp"
#include "family_source_projection.hpp"
namespace hanmo::resident {
Status emit_collateral_recognition_fact_payloads_fields(
    ProjectionWriter &out, const CommonStore &c,
    const CollateralControlState *state) {
  if (state) {
    auto shape = validate_collateral_control_shape(*state, 1);
    if (!shape.ok)
      return shape;
    for (const auto &row : state->recognitions.rows()) {
      const auto *h = c.state().facts.find(row.first);
      if (!h || h->id != row.first ||
          h->kind != "family_collateral_recognized" ||
          h->domain != "family_collateral_legacy")
        return common_failure("CollateralRecognitionPayloadHeaderMismatch",
                              row.first);
      out.object(row.first, [&](auto &w) {
        w.field("kind",
                std::get<CollateralRecognitionFact>(row.second.payload).kind);
        w.object("source", [&](auto &source) {
          return emit_collateral_recognition_source_fields(
              source,
              std::get<CollateralRecognitionFact>(row.second.payload).source);
        });
        w.text("domain", "family_collateral_legacy");
        return w.order_fields(h->field_order);
      });
      if (!out.status().ok)
        return out.status();
    }
  }
  std::size_t wanted = 0;
  bool order_mismatch = false;
  for (const auto &row : c.state().facts.rows())
    if (row.second.domain == "family_collateral_legacy" ||
        row.second.kind == "family_collateral_recognized" ||
        row.first.rfind("family_collateral:", 0) == 0) {
      if (!state || !state->recognitions.find(row.first))
        return common_failure("CollateralRecognitionPayloadSourceRequired",
                              row.first);
      if (wanted >= state->recognitions.order().size() ||
          state->recognitions.order()[wanted] != row.first)
        order_mismatch = true;
      ++wanted;
    }
  if (state && (order_mismatch || wanted != state->recognitions.size()))
    return common_failure("CollateralRecognitionPayloadOrderMismatch");
  return out.status();
}
Outcome<BoundaryValue>
collateral_recognition_fact_payloads(const CommonStore &c,
                                     const CollateralControlState *state) {
  BoundaryValue value;
  BoundaryProjectionWriter writer(value);
  auto status = writer.object("", [&](auto &out) {
    return emit_collateral_recognition_fact_payloads_fields(out, c, state);
  });
  if (status.ok)
    status = writer.finish();
  return {status.ok, std::move(value), status.error};
}
Status validate_collateral_recognition_json_payloads(
    const CommonStore &c, const CollateralControlState *state,
    const BoundaryValue &full) {
  try {
    auto projected = collateral_recognition_fact_payloads(c, state);
    if (!projected.ok)
      return {false, {}, projected.error};
    const auto &facts = boundary::field(full, "facts", "state");
    for (const auto &row :
         boundary::object(projected.value, "family_collateral_legacy")) {
      const auto *actual = facts.find(row.first);
      if (!actual || !compare_boundary(*actual, row.second).exact())
        return common_failure("CollateralRecognitionPayloadMismatch",
                              row.first);
    }
    return common_success();
  } catch (const boundary::DecodeFailure &e) {
    return {false, {}, e.error};
  }
}
} // namespace hanmo::resident
