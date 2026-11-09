// 组织目录和冷状态逐字段转换；缺字段、未知枚举或重复键都明确失败。
#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "organization_codec_detail.hpp"
#include "organization_projection.hpp"
#include "organization_service_effects_codec.hpp"
namespace hanmo::resident {
Outcome<OrganizationCatalog>
decode_organization_catalog(const BoundaryValue &root, bool named_membership) {
  try {
    // 原v2/v3组织目录借用同型目录已登记的上下文；不新建额度或旁路既有预算。
    std::optional<ScopedCheckpointJsonDecode> original;
    if (ScopedCheckpointBoundaryDecode::current() &&
        !ScopedCheckpointJsonDecode::current())
      original.emplace(CheckpointCatalogJsonKind::organization_authority);
    OrganizationCatalog out;
    const auto *section = root.find("organization");
    dynamic_economy_codec::decode(section ? *section : root, out,
                                  "/organization", false, named_membership);
    return {true, std::move(out), {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Outcome<OrganizationState> decode_organization_state(const BoundaryValue &root,
                                                     bool named_membership) {
  try {
    OrganizationState out;
    dynamic_economy_codec::decode(root, out, "/organization", false,
                                  named_membership);
    auto valid = validate_organization_service_effects(out, 0);
    return {valid.ok, std::move(out), valid.error};
  } catch (const boundary::DecodeFailure &error) {
    return {false, {}, error.error};
  }
}
Status emit_organization_state_fields(ProjectionWriter &out,
                                      const OrganizationState &state,
                                      bool named_membership) {
  auto selected = validate_organization_service_effects(state, 0);
  if (!selected.ok)
    return selected;
  for (const auto &row : state.facts.rows())
    if (row.second.occurred_at)
      return out.reject("OrganizationExactFactCodecRequired", row.first);
  return dynamic_economy_codec::emit_fields(out, state, false,
                                            named_membership);
}
Status emit_organization_state_projection(ProjectionWriter &out,
                                          std::string_view name,
                                          const OrganizationState &state,
                                          bool named_membership) {
  return out.object(name, [&](auto &fields) {
    return emit_organization_state_fields(fields, state, named_membership);
  });
}
BoundaryValue encode_organization_state(const OrganizationState &state,
                                        bool named_membership) {
  BoundaryValue value;
  BoundaryProjectionWriter out(value);
  auto status =
      emit_organization_state_projection(out, "", state, named_membership);
  if (status.ok)
    status = out.finish();
  if (!status.ok) {
    boundary::DecodeFailure failure(status.error.original.code,
                                    status.error.original.path);
    failure.error = status.error;
    throw failure;
  }
  return value;
}

} // namespace hanmo::resident
