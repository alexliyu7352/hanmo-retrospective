// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto transport_versions =
    selected_native_transport_versions(w.catalog->identity);
if (!transport_versions.ok)
  return {false, {}, std::move(transport_versions.error)};
auto transport_section = [&](CodecVisitor &codec,
                             TransportPhysicalSection part) -> Status {
  auto roots = native_transport_physical_roots(w);
  if (!roots.ok)
    return {false, {}, std::move(roots.error)};
  if (!roots.value)
    return common_failure("SelectedTransportRootsRequired", {},
                          ErrorKind::internal);
  auto selected = native_transport_physical_selection(*w.catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto context = make_transport_physical_section_write_context(
      w.common, *roots.value, selected.value, part);
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  Status written =
      part == TransportPhysicalSection::travel
          ? visit_travel_physical_checkpoint(codec, *context.value)
      : part == TransportPhysicalSection::group_transport
          ? visit_group_transport_physical_checkpoint(codec, *context.value)
          : visit_transport_manifest_bindings_physical_checkpoint(
                codec, *context.value);
  if (!written.ok)
    return written;
  return finish_transport_physical_codec(*context.value);
};
r = section("travel", [&](auto &c) {
  return transport_versions.value.travel
             ? transport_section(c, TransportPhysicalSection::travel)
             : visit_travel_checkpoint(c, w.travel);
});
if (!r.ok)
  return {false, {}, r.error};
if (transport_versions.value.group) {
  r = section("group_transport", [&](auto &c) {
    return transport_section(c, TransportPhysicalSection::group_transport);
  });
  if (!r.ok)
    return {false, {}, r.error};
}
if (transport_versions.value.manifest) {
  r = section("transport_manifest_bindings", [&](auto &c) {
    return transport_section(
        c, TransportPhysicalSection::transport_manifest_bindings);
  });
  if (!r.ok)
    return {false, {}, r.error};
}

// 独立 /1 扩展只保存原 movement 的单表，不改变上面三个物理段的字段。
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
  r = section("movement_boundary_contacts", [&](auto &c) {
    return visit_movement_boundary_checkpoint(
        c, *w.movement_exact, boundary_version.value, selected.value);
  });
  if (!r.ok)
    return {false, {}, std::move(r.error)};
}

// 同一前置锁必备独立continuation/1，空表也写；contacts/1字节完全不变。
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
  r = section(movement_boundary_continuation_module, [&](auto &c) {
    return visit_movement_boundary_continuation_checkpoint(
        c, *w.movement_exact, continuation_version.value, records.value);
  });
  if (!r.ok)
    return {false, {}, std::move(r.error)};
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
  r = section(local_move_module, [&](auto &c) {
    return visit_local_move_checkpoint(
        c, *w.movement_exact, local_move_version.value, records.value);
  });
  if (!r.ok)
    return {false, {}, std::move(r.error)};
}

// 原 group 表各用独立明确段；未选有行/孤头仍拒绝，不隐入旧 group 字节。
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
  if (handoff_versions.value.releases) {
    r = section("movement_group_member_releases", [&](auto &c) {
      return visit_group_member_release_checkpoint(
          c, *w.group_movement_executions, handoff_versions.value.releases,
          records.value);
    });
    if (!r.ok)
      return {false, {}, std::move(r.error)};
  }
  if (handoff_versions.value.losses) {
    r = section("movement_group_member_losses", [&](auto &c) {
      return visit_group_member_loss_handoff_checkpoint(
          c, *w.group_movement_executions, handoff_versions.value.losses,
          records.value);
    });
    if (!r.ok)
      return {false, {}, std::move(r.error)};
  }
}
