// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
namespace {
// 旧裸入口仍拒活跃 job 没有持久 owner；只有原 owned decoder 传同 job 预算，
// 实际 basis 与两个交接表的拥有量留在原封存 seed，不于本函数返回提前释放。
Status decode_native_transport_json(const BoundaryValue &source,
                                    const PreparedCatalog &catalog,
                                    InitialWorldSeed &seed,
                                    CodecDecodeBudget *owned_budget,
                                    CheckpointJobBudget *owned_job) {
  if (bool(owned_budget) != bool(owned_job))
    return common_failure("CheckpointJsonOwnedContextRequired");
  auto versions = selected_native_transport_versions(catalog.identity);
  if (!versions.ok)
    return {false, {}, std::move(versions.error)};
  auto boundary_version =
      selected_native_movement_boundary_version(catalog.identity);
  if (!boundary_version.ok)
    return {false, {}, std::move(boundary_version.error)};
  if (!boundary_version.value && source.find("movement_boundary_contacts"))
    return common_failure("UnexpectedMovementBoundaryPhysicalSection");
  auto continuation_version =
      selected_native_movement_boundary_continuation_version(catalog.identity);
  if (!continuation_version.ok)
    return {false, {}, std::move(continuation_version.error)};
  if (!continuation_version.value && source.find(movement_boundary_continuation_module))
    return common_failure("UnexpectedMovementBoundaryContinuationPhysicalSection");
  auto local_move_version =
      selected_native_local_move_version(catalog.identity);
  if (!local_move_version.ok)
    return {false, {}, std::move(local_move_version.error)};
  if (!local_move_version.value && source.find(local_move_module))
    return common_failure("UnexpectedLocalMovePhysicalSection");
  auto handoffs =
      selected_native_group_member_handoff_versions(catalog.identity);
  if (!handoffs.ok)
    return {false, {}, std::move(handoffs.error)};
  if ((!handoffs.value.releases &&
       source.find("movement_group_member_releases")) ||
      (!handoffs.value.losses && source.find("movement_group_member_losses")))
    return common_failure("UnexpectedGroupMemberHandoffPhysicalSection");
  if (!versions.value.travel) {
    auto continuation_headers = validate_native_movement_boundary_continuation_headers(
        seed.common, catalog.identity, nullptr);
    if (!continuation_headers.ok)
      return continuation_headers;
    auto local_move_headers = validate_native_local_move_headers(
        seed.common, catalog.identity, nullptr);
    if (!local_move_headers.ok)
      return local_move_headers;
    auto headers = validate_native_group_member_handoff_headers(
        seed.common, catalog.identity, nullptr);
    if (!headers.ok)
      return headers;
    for (const auto *name :
         {"travel", "group_transport", "transport_manifest_bindings"})
      if (source.find(name))
        return common_failure("UnexpectedTransportPhysicalSection", name,
                              ErrorKind::internal);
    auto original = decode_travel_seed(source);
    if (!original.ok)
      return {false, {}, std::move(original.error)};
    seed.travel =
        checkpoint_make_shared<const TravelState>(std::move(original.value));
    return common_success();
  }
  if (owned_budget) {
    auto active = owned_budget->account_allocator_candidate(
        0, current_checkpoint_memory());
    if (!active.ok)
      return active;
  } else if (current_checkpoint_memory())
    return common_failure("WorldJsonDecodedOwnerReservationRequired", {},
                          ErrorKind::unsupported_capability);
  auto selected = native_transport_physical_selection(catalog);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto travel = checkpoint_make_shared<TravelState>();
  auto movement = checkpoint_make_shared<MovementExactState>();
  auto carriers = checkpoint_make_shared<SpatialCarrierState>();
  auto executions = checkpoint_make_shared<GroupMovementExecutionState>();
  auto registrations = checkpoint_make_shared<TransportRegistrationState>();
  auto payments = checkpoint_make_shared<EconomicTransportExpenseState>();
  CommonStore common(std::move(seed.common), catalog.common);
  CodecDecodeBudget legacy_budget(engineering_checkpoint_capacity_profile());
  NativeMovementBasisDecodeAllocation allocation(owned_budget ? *owned_budget
                                                              : legacy_budget);
  TransportPhysicalCodecRoots roots{
      *travel,
      *movement,
      versions.value.manifest ? carriers.get() : nullptr,
      versions.value.manifest ? executions.get() : nullptr,
      versions.value.manifest ? registrations.get() : nullptr,
      payments.get()};
  auto context = make_transport_physical_codec_context(
      common, roots, selected.value, true, {}, &allocation);
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  auto read = decode_travel_physical(source, field(source, "travel", "state"),
                                     *context.value);
  if (!read.ok)
    return read;
  if (versions.value.group) {
    read = decode_group_transport_physical(
        field(source, "group_transport", "state"), *context.value);
    if (!read.ok)
      return read;
  } else if (source.find("group_transport"))
    return common_failure("UnexpectedTransportPhysicalSection",
                          "group_transport", ErrorKind::internal);
  if (versions.value.manifest) {
    read = decode_transport_manifest_bindings_physical(
        field(source, "transport_manifest_bindings", "state"), *context.value);
    if (!read.ok)
      return read;
  } else if (source.find("transport_manifest_bindings"))
    return common_failure("UnexpectedTransportPhysicalSection",
                          "transport_manifest_bindings", ErrorKind::internal);
  read = finish_transport_physical_codec(*context.value);
  if (!read.ok)
    return read;
  // 旧 transport 完整恢复后只在同 movement 原表读独立边界段。
  if (boundary_version.value) {
    auto records = native_movement_boundary_codec_selection(catalog);
    if (!records.ok)
      return {false, {}, std::move(records.error)};
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget && owned_job)
      section.emplace(*owned_budget, *owned_job, "movement_boundary_contacts",
                      current_checkpoint_memory());
    read = decode_movement_boundary_contacts(
        field(source, "movement_boundary_contacts", "state"), *movement,
        boundary_version.value, records.value);
    if (!read.ok)
      return read;
  }
  if (continuation_version.value) {
    auto records = native_movement_boundary_codec_selection(catalog);
    if (!records.ok)
      return {false, {}, std::move(records.error)};
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget && owned_job)
      section.emplace(*owned_budget, *owned_job, movement_boundary_continuation_module,
                      current_checkpoint_memory());
    read = decode_movement_boundary_continuations(
        field(source, movement_boundary_continuation_module, "state"), *movement,
        continuation_version.value, records.value);
    if (!read.ok)
      return read;
  }
  read = validate_native_movement_boundary_continuation_headers(
      common.state(), catalog.identity, movement.get());
  if (!read.ok)
    return read;
  if (local_move_version.value) {
    auto records = native_local_move_codec_selection(catalog);
    if (!records.ok)
      return {false, {}, std::move(records.error)};
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget && owned_job)
      section.emplace(*owned_budget, *owned_job, local_move_module,
                      current_checkpoint_memory());
    read = decode_local_moves(
        field(source, local_move_module, "state"), *movement,
        local_move_version.value, records.value);
    if (!read.ok)
      return read;
  }
  read = validate_native_local_move_headers(
      common.state(), catalog.identity, movement.get());
  if (!read.ok)
    return read;
  read = validate_native_movement_boundary_headers(common.state(),
                                                   catalog.identity, *movement);
  if (!read.ok)
    return read;
  if (handoffs.value.releases || handoffs.value.losses) {
    auto records = native_group_member_handoff_codec_selection(catalog);
    if (!records.ok)
      return {false, {}, std::move(records.error)};
    if (handoffs.value.releases) {
      std::optional<ScopedCheckpointJsonDecode> section;
      if (owned_budget && owned_job)
        section.emplace(*owned_budget, *owned_job,
                        "movement_group_member_releases",
                        current_checkpoint_memory());
      read = decode_group_member_releases(
          field(source, "movement_group_member_releases", "state"), *executions,
          handoffs.value.releases, records.value);
      if (!read.ok)
        return read;
    }
    if (handoffs.value.losses) {
      std::optional<ScopedCheckpointJsonDecode> section;
      if (owned_budget && owned_job)
        section.emplace(*owned_budget, *owned_job,
                        "movement_group_member_losses",
                        current_checkpoint_memory());
      read = decode_group_member_loss_handoffs(
          field(source, "movement_group_member_losses", "state"), *executions,
          handoffs.value.losses, records.value);
      if (!read.ok)
        return read;
    }
  }
  read = validate_native_group_member_handoff_headers(
      common.state(), catalog.identity, executions.get());
  if (!read.ok)
    return read;
  // 原Common只持头；不得把同一实际来源又存入facts JSON正文。
  for (const auto &row : common.state().facts.rows())
    if (owns_transport_physical_fact_kind(row.second.domain, row.second.kind) ||
        (row.second.domain == "travel" &&
         (row.second.kind == "movement_boundary_contact" ||
          row.second.kind == "movement_boundary_continuation" ||
          row.second.kind == "local_move" ||
          row.second.kind == "group_contact_member_release" ||
          row.second.kind == "group_member_loss_handoff")))
      fields(field(field(source, "facts", "state"), row.first, "facts"),
             {"kind", "domain"}, {}, row.first);
  context.value.reset();
  seed.common = std::move(common.state());
  seed.travel = std::move(travel);
  seed.movement_exact = std::move(movement);
  seed.spatial_carriers = std::move(carriers);
  seed.group_movement_executions = std::move(executions);
  seed.transport_registrations = std::move(registrations);
  seed.economic_transport_expenses = std::move(payments);
  return common_success();
}
Status emit_native_transport_json_section(ProjectionWriter &writer,
                                          const WorldBundle &world,
                                          TransportPhysicalSection part) {
  auto roots =
      native_transport_physical_roots(const_cast<WorldBundle &>(world));
  if (!roots.ok)
    return {false, {}, std::move(roots.error)};
  if (!roots.value)
    return common_failure("SelectedTransportRootsRequired", {},
                          ErrorKind::internal);
  auto selection = native_transport_physical_selection(*world.catalog);
  if (!selection.ok)
    return {false, {}, std::move(selection.error)};
  auto context = make_transport_physical_section_write_context(
      world.common, *roots.value, selection.value, part);
  if (!context.ok)
    return {false, {}, std::move(context.error)};
  auto emitted =
      part == TransportPhysicalSection::travel
          ? emit_travel_physical_projection(writer, "", *context.value)
      : part == TransportPhysicalSection::group_transport
          ? emit_group_transport_physical_projection(writer, "", *context.value)
          : emit_transport_manifest_bindings_physical_projection(
                writer, "", *context.value);
  if (!emitted.ok)
    return emitted;
  return finish_transport_physical_codec(*context.value);
}
} // namespace
