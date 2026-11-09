// 行旅世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
// 原base先由完整旧selector确定，P06能力位与三个S1段仅沿同锁身份复合。
template <class Domains>
Status apply_native_transport_domains(Domains &domains,
                                      const CatalogIdentity &identity) {
  auto selected = selected_native_transport_versions(identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  auto travel = std::find_if(domains.begin(), domains.end(),
                             [](const auto &d) { return d.name == "travel"; });
  if (travel == domains.end())
    return common_failure("OriginalTravelCheckpointDomainRequired", {},
                          ErrorKind::internal);
  if (selected.value.travel)
    travel->codec_version = static_cast<std::uint32_t>(selected.value.travel);
  for (const auto &row :
       std::initializer_list<std::pair<const char *, Integer>>{
           {"group_transport", selected.value.group==6?7:selected.value.group},
           {"transport_manifest_bindings", selected.value.manifest}}) {
    if (!row.second)
      continue;
    if (std::any_of(domains.begin(), domains.end(),
                    [&](const auto &d) { return d.name == row.first; }))
      return common_failure("DuplicateTransportCheckpointDomain", row.first,
                            ErrorKind::internal);
    domains.push_back({row.first, row.second});
  }
  auto boundary = selected_native_movement_boundary_version(identity);
  if (!boundary.ok)
    return {false, {}, std::move(boundary.error)};
  if (boundary.value) {
    if (std::any_of(domains.begin(), domains.end(), [](const auto &d) {
          return d.name == "movement_boundary_contacts";
        }))
      return common_failure("DuplicateMovementBoundaryCheckpointDomain");
    domains.push_back({"movement_boundary_contacts", boundary.value});
  }
  auto continuation = selected_native_movement_boundary_continuation_version(identity);
  if (!continuation.ok)
    return {false, {}, std::move(continuation.error)};
  if (continuation.value) {
    if (std::any_of(domains.begin(), domains.end(), [](const auto &d) {
          return d.name == movement_boundary_continuation_module;
        }))
      return common_failure("DuplicateMovementBoundaryContinuationCheckpointDomain");
    domains.push_back({movement_boundary_continuation_module, continuation.value});
  }
  auto local_move = selected_native_local_move_version(identity);
  if (!local_move.ok)
    return {false, {}, std::move(local_move.error)};
  if (local_move.value) {
    if (std::any_of(domains.begin(), domains.end(), [](const auto &d) {
          return d.name == local_move_module;
        }))
      return common_failure("DuplicateLocalMoveCheckpointDomain");
    domains.push_back({local_move_module, local_move.value});
  }
  auto handoffs = selected_native_group_member_handoff_versions(identity);
  if (!handoffs.ok)
    return {false, {}, std::move(handoffs.error)};
  for (const auto &part :
       std::initializer_list<std::pair<const char *, Integer>>{
           {"movement_group_member_releases", handoffs.value.releases},
           {"movement_group_member_losses", handoffs.value.losses}}) {
    if (!part.second)
      continue;
    if (std::any_of(domains.begin(), domains.end(),
                    [&](const auto &d) { return d.name == part.first; }))
      return common_failure("DuplicateGroupMemberHandoffCheckpointDomain",
                            part.first);
    domains.push_back({part.first, part.second});
  }
  auto common = std::find_if(domains.begin(), domains.end(),
                             [](const auto &d) { return d.name == "common"; });
  if (common == domains.end())
    return common_failure("CommonCheckpointDomainRequired", {},
                          ErrorKind::internal);
  auto original = decode_common_codec_selection(common->codec_version);
  if (!original.ok)
    return {false, {}, std::move(original.error)};
  auto physical = select_common_codec(original.value.base_version, identity);
  if (!physical.ok)
    return {false, {}, std::move(physical.error)};
  common->codec_version = static_cast<std::uint32_t>(physical.value.version);
  return common_success();
}
