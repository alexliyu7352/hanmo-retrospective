// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto versions = selected_native_transport_versions(w.catalog->identity);
if (!versions.ok)
  return {false, {}, std::move(versions.error)};
if (versions.value.travel)
  WORLD_WHOLE(P::section, "travel",
              emit_native_transport_json_section(
                  out, w, TransportPhysicalSection::travel));
if (versions.value.group)
  WORLD_WHOLE(P::section, "group_transport",
              emit_native_transport_json_section(
                  out, w, TransportPhysicalSection::group_transport));
if (versions.value.manifest)
  WORLD_WHOLE(P::section, "transport_manifest_bindings",
              emit_native_transport_json_section(
                  out, w,
                  TransportPhysicalSection::transport_manifest_bindings));

auto boundary_version =
    selected_native_movement_boundary_version(w.catalog->identity);
if (!boundary_version.ok)
  return {false, {}, std::move(boundary_version.error)};
if (boundary_version.value) {
  auto layout = validate_native_movement_boundary_layout(w);
  if (!layout.ok)
    return {false, {}, std::move(layout.error)};
  auto selected = native_movement_boundary_codec_selection(*w.catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  WORLD_WHOLE(P::section, "movement_boundary_contacts",
              emit_movement_boundary_projection(out, "", *w.movement_exact,
                                                boundary_version.value,
                                                selected.value));
}

auto continuation_version =
    selected_native_movement_boundary_continuation_version(w.catalog->identity);
if (!continuation_version.ok)
  return {false, {}, std::move(continuation_version.error)};
auto continuation_layout = validate_native_movement_boundary_continuation_layout(w);
if (!continuation_layout.ok)
  return {false, {}, std::move(continuation_layout.error)};
if (continuation_version.value) {
  auto records = native_movement_boundary_codec_selection(*w.catalog);
  if (!records.ok)
    return {false, {}, std::move(records.error)};
  WORLD_WHOLE(P::section, movement_boundary_continuation_module,
              emit_movement_boundary_continuation_projection(
                  out, "", *w.movement_exact, continuation_version.value, records.value));
}

auto local_move_version =
    selected_native_local_move_version(w.catalog->identity);
if (!local_move_version.ok)
  return {false, {}, std::move(local_move_version.error)};
auto local_move_layout = validate_native_local_move_layout(w);
if (!local_move_layout.ok)
  return {false, {}, std::move(local_move_layout.error)};
if (local_move_version.value) {
  auto records = native_local_move_codec_selection(*w.catalog);
  if (!records.ok)
    return {false, {}, std::move(records.error)};
  WORLD_WHOLE(P::section, local_move_module,
              emit_local_move_projection(
                  out, "", *w.movement_exact, local_move_version.value, records.value));
}

// 只写两个原表，不复制业务状态或借身体引用伪造另一个 Common 头。
auto handoff_layout = validate_native_group_member_handoff_layout(w);
if (!handoff_layout.ok)
  return {false, {}, std::move(handoff_layout.error)};
auto handoff_versions =
    selected_native_group_member_handoff_versions(w.catalog->identity);
if (!handoff_versions.ok)
  return {false, {}, std::move(handoff_versions.error)};
if (handoff_versions.value.releases || handoff_versions.value.losses) {
  auto records = native_group_member_handoff_codec_selection(*w.catalog);
  if (!records.ok)
    return {false, {}, std::move(records.error)};
  if (handoff_versions.value.releases)
    WORLD_WHOLE(P::section, "movement_group_member_releases",
                emit_group_member_release_projection(
                    out, "", *w.group_movement_executions,
                    handoff_versions.value.releases, records.value));
  if (handoff_versions.value.losses)
    WORLD_WHOLE(P::section, "movement_group_member_losses",
                emit_group_member_loss_handoff_projection(
                    out, "", *w.group_movement_executions,
                    handoff_versions.value.losses, records.value));
}
