// 行旅检查点恢复分派：沿原 else-if 链读取 travel, travel, group_transport,
// transport_manifest_bindings；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "travel" && transport_versions.value.travel) {
  if (!common_read || transport_context || transport_finished)
    return {false, {}, common_failure("TransportRestoreOrderMismatch").error};
  transport_common.emplace(std::move(seed.common), catalog.common);
  auto selected = native_transport_physical_selection(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  TransportPhysicalCodecRoots roots{
      *t,
      *movement,
      transport_versions.value.manifest ? carriers.get() : nullptr,
      transport_versions.value.manifest ? executions.get() : nullptr,
      transport_versions.value.manifest ? registrations.get() : nullptr,
      payments.get()};
  auto context = make_transport_physical_codec_context(
      *transport_common, roots, selected.value, true, {}, &basis_allocation);
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  transport_context = std::move(context.value);
  r = visit_travel_physical_checkpoint(reader, *transport_context);
}
else if (section.domain == "travel") r = visit_travel_checkpoint(reader, *t);
else if (section.domain == "group_transport") {
  if (!transport_context || !transport_versions.value.group)
    return {false, {}, common_failure("TransportRestoreOrderMismatch").error};
  r = visit_group_transport_physical_checkpoint(reader, *transport_context);
}
else if (section.domain == "transport_manifest_bindings") {
  if (!transport_context || !transport_versions.value.manifest)
    return {false, {}, common_failure("TransportRestoreOrderMismatch").error};
  r = visit_transport_manifest_bindings_physical_checkpoint(reader,
                                                            *transport_context);
}
else if (section.domain == "movement_boundary_contacts") {
  if (!movement || !transport_finished || boundary_decoded ||
      boundary_version.value != 1 || section.codec_version != 1)
    return {false,
            {},
            common_failure("MovementBoundaryRestoreOrderMismatch").error};
  auto selected = native_movement_boundary_codec_selection(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  r = visit_movement_boundary_checkpoint(
      reader, *movement, boundary_version.value, selected.value);
  boundary_decoded = true;
}

else if (section.domain == movement_boundary_continuation_module) {
  if (!movement || !transport_finished || !boundary_decoded || continuation_decoded ||
      continuation_version.value != 1 || section.codec_version != 1)
    return {false, {}, common_failure("MovementBoundaryContinuationRestoreOrderMismatch").error};
  auto records = native_movement_boundary_codec_selection(catalog);
  if (!records.ok)
    return {false, {}, std::move(records.error)};
  r = visit_movement_boundary_continuation_checkpoint(
      reader, *movement, continuation_version.value, records.value);
  continuation_decoded = true;
}
else if (section.domain == local_move_module) {
  if (!movement || !transport_finished || local_move_decoded ||
      local_move_version.value != 1 || section.codec_version != 1)
    return {false, {}, common_failure("LocalMoveRestoreOrderMismatch").error};
  auto records = native_local_move_codec_selection(catalog);
  if (!records.ok)
    return {false, {}, std::move(records.error)};
  r = visit_local_move_checkpoint(
      reader, *movement, local_move_version.value, records.value);
  local_move_decoded = true;
}
else if (section.domain == "movement_group_member_releases") {
  if (!executions || !transport_finished || member_releases_decoded ||
      handoff_versions.value.releases != 1 || section.codec_version != 1)
    return {false,
            {},
            common_failure("GroupMemberReleaseRestoreOrderMismatch").error};
  auto selected = native_group_member_handoff_codec_selection(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  r = visit_group_member_release_checkpoint(
      reader, *executions, handoff_versions.value.releases, selected.value);
  member_releases_decoded = true;
}
else if (section.domain == "movement_group_member_losses") {
  if (!executions || !transport_finished || member_losses_decoded ||
      handoff_versions.value.losses != 2 || section.codec_version != 2)
    return {
        false, {}, common_failure("GroupMemberLossRestoreOrderMismatch").error};
  auto selected = native_group_member_handoff_codec_selection(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  r = visit_group_member_loss_handoff_checkpoint(
      reader, *executions, handoff_versions.value.losses, selected.value);
  member_losses_decoded = true;
}
