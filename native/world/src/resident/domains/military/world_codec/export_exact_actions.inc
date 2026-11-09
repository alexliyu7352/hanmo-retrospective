// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (w.field_battle)
  WORLD_WHOLE(P::section, "field_battle",
              emit_field_battle_world(out, "", *w.field_battle));
auto military_layout = validate_native_military_state_layout(w);
if (!military_layout.ok)
  return military_layout;
auto military_versions =
    selected_native_military_state_versions(w.catalog->identity);
if (!military_versions.ok)
  return {false, {}, std::move(military_versions.error)};
if (w.military_exact)
  WORLD_WHOLE(
      P::section, "dynamic_military",
      emit_military_exact_state(out, "", *w.military_exact,
                                military_versions.value.dynamic_military));
if (w.military_action_time)
  WORLD_WHOLE(P::section, "military_action_time",
              emit_military_action_time_state(
                  out, "", *w.military_action_time,
                  military_versions.value.military_action_time));
if (military_versions.value.military_authority_time)
  WORLD_WHOLE(P::section, "military_authority_time",
              emit_military_authority_time(out, "", w.military, 1));
if (military_versions.value.military_action_resources)
  WORLD_WHOLE(P::section, "military_action_resources",
              emit_military_action_resources(out, "", *w.military_action_time,
                                             1, w.dynamic_enabled(),
                                             resource_context));
if (military_versions.value.military_return_detachments)
  WORLD_WHOLE(P::section, "military_return_detachments",
              emit_military_return_detachments(out, "", *w.military_action_time,
                                               1));
if (military_versions.value.military_formation_reservations)
  WORLD_WHOLE(P::section, "military_formation_reservations",
              emit_military_formation_reservations(
                  out, "", *w.military_action_time,
                  military_versions.value.military_formation_reservations,
                  w.dynamic_enabled(), resource_context));
if (military_versions.value.military_action_conditions)
  WORLD_WHOLE(P::section, "military_action_conditions",
              emit_military_action_conditions(
                  out, "", *w.military_action_time,
                  military_versions.value.military_action_conditions));
if (military_versions.value.military_training_completion_time)
  WORLD_WHOLE(P::section, "military_training_completion_time",
              emit_military_training_completion_time(out, "",
                                                     *w.military_action_time,
                                                     1));
