// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r=validate_field_battle_layout(w.catalog->identity,w.catalog->field_battle.get(),w.field_battle?&*w.field_battle:nullptr);
if(!r.ok)return r;
if(w.field_battle){r=section("field_battle",[&](auto& c){return visit_field_battle_world(c,*w.field_battle);});if(!r.ok)return r;}
r = validate_native_military_state_layout(w);
if (!r.ok)
  return r;
auto military_versions =
    selected_native_military_state_versions(w.catalog->identity);
if (!military_versions.ok)
  return {false, {}, std::move(military_versions.error)};
if (w.military_exact) {
  r = section("dynamic_military", [&](auto &c) {
    return visit_military_exact_state(c, *w.military_exact,
                                      military_versions.value.dynamic_military);
  });
  if (!r.ok)
    return r;
}
if (w.military_action_time) {
  r = section("military_action_time", [&](auto &c) {
    return visit_military_action_time_state(
        c, *w.military_action_time,
        military_versions.value.military_action_time);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_authority_time) {
  r = section("military_authority_time", [&](auto &c) {
    return visit_military_authority_time(c, w.military, 1);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_action_resources) {
  r = section("military_action_resources", [&](auto &c) {
    return visit_military_action_resources(c, *w.military_action_time, 1);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_return_detachments) {
  r = section("military_return_detachments", [&](auto &c) {
    return visit_military_return_detachments(c, *w.military_action_time, 1);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_formation_reservations) {
  r = section("military_formation_reservations", [&](auto &c) {
    return visit_military_formation_reservations(
        c, *w.military_action_time,
        military_versions.value.military_formation_reservations);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_action_conditions) {
  r = section("military_action_conditions", [&](auto &c) {
    return visit_military_action_conditions(
        c, *w.military_action_time,
        military_versions.value.military_action_conditions);
  });
  if (!r.ok)
    return r;
}
if (military_versions.value.military_training_completion_time) {
  r = section("military_training_completion_time", [&](auto &c) {
    return visit_military_training_completion_time(c, *w.military_action_time,
                                                   1);
  });
  if (!r.ok)
    return r;
}
