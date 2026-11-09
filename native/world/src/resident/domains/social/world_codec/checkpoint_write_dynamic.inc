// 社交世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (!w.social.dynamic)
  return {false, {}, common_failure("MissingDynamicSocial").error};
r = section("dynamic_social", [&](auto &c) {
  return visit_native_dynamic_social_checkpoint(c, *w.social.dynamic,
                                                *w.catalog);
});
if (!r.ok)
  return {false, {}, r.error};
