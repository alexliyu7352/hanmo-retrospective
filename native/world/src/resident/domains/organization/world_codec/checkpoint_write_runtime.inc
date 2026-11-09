// 组织世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("organization", [&](auto &c) {
  return w.organization_runtime
             ? visit_organization_runtime_selected_body(c, w.organization,
                                                        w.catalog->identity)
             : visit_organization_checkpoint(c, w.organization,
                                             w.named_membership_enabled());
});
if (!r.ok)
  return {false, {}, r.error};
if (w.organization_runtime) {
  r = section("organization_runtime", [&](auto &c) {
    return visit_organization_runtime(c, w.organization,
                                      *w.organization_runtime,
                                      w.named_membership_enabled());
  });
  if (!r.ok)
    return {false, {}, r.error};
}
{
  auto effects =
      selected_native_organization_service_effects(w.catalog->identity);
  if (!effects.ok)
    return {false, {}, std::move(effects.error)};
  if (effects.value) {
    r = section("organization_service_effects", [&](auto &c) {
      return visit_organization_service_effects_checkpoint(c, w.organization,
                                                           effects.value);
    });
    if (!r.ok)
      return {false, {}, std::move(r.error)};
  }
}
