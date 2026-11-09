// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (w.organization_runtime) {
  WORLD_WHOLE(P::section, "organization",
              emit_organization_runtime_selected_body_projection(
                  out, "", w.organization, w.catalog->identity));
  WORLD_WHOLE(P::section, "organization_runtime",
              emit_organization_runtime_projection(
                  out, "", w.organization, *w.organization_runtime,
                  w.named_membership_enabled()));
  auto effects =
      selected_native_organization_service_effects(w.catalog->identity);
  if (!effects.ok)
    return {false, {}, std::move(effects.error)};
  if (effects.value)
    WORLD_WHOLE(P::section, "organization_service_effects",
                emit_organization_service_effects_projection(
                    out, "", w.organization, effects.value));
} else {
  WORLD_FIELDS(P::section, "organization",
               emit_organization_state_fields(out, w.organization,
                                              w.named_membership_enabled()));
}
