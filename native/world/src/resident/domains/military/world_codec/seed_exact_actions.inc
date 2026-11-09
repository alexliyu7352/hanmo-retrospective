// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto military_catalog = validate_native_military_state_catalog(catalog);
if (!military_catalog.ok)
  return {false, {}, std::move(military_catalog.error)};
auto military_versions =
    selected_native_military_state_versions(catalog.identity);
if (!military_versions.ok)
  return {false, {}, std::move(military_versions.error)};
if (military_versions.value.dynamic_military) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::dynamic_military);
  auto parsed =
      decode_military_exact_state(field(source, "dynamic_military", "state"),
                                  military_versions.value.dynamic_military);
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  result.military_exact =
      checkpoint_make_shared<const MilitaryExactState>(std::move(parsed.value));
} else if (source.find("dynamic_military"))
  fail("UnexpectedDynamicMilitaryState", "dynamic_military");
if (military_versions.value.military_action_time) {
  auto parsed = [&]() {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_action_time);
    return decode_military_action_time_state(
        field(source, "military_action_time", "state"),
        military_versions.value.military_action_time);
  }();
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  if (military_versions.value.military_action_resources) {
    auto selected = select_common_codec(5, catalog.identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    const auto &f = selected.value.features;
    CommonResourceCodecContext resources{f.item_instances, f.lot_properties,
                                         f.domain_activity, f.carrier_location};
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_action_resources);
    auto attached = decode_military_action_resources(
        field(source, "military_action_resources", "state"), parsed.value, 1,
        catalog.dynamic_profile == DynamicProfile::local_society_v1, resources);
    if (!attached.ok)
      return {false, {}, std::move(attached.error)};
  }
  if (military_versions.value.military_return_detachments) {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_return_detachments);
    auto attached = decode_military_return_detachments(
        field(source, "military_return_detachments", "state"), parsed.value, 1);
    if (!attached.ok)
      return {false, {}, std::move(attached.error)};
  }
  auto detachments = validate_military_return_detachment_layout(
      parsed.value, military_versions.value.military_return_detachments);
  if (!detachments.ok)
    return {false, {}, std::move(detachments.error)};
  if (military_versions.value.military_formation_reservations) {
    auto selected = select_common_codec(5, catalog.identity);
    if (!selected.ok)
      return {false, {}, std::move(selected.error)};
    const auto &f = selected.value.features;
    CommonResourceCodecContext resources{f.item_instances, f.lot_properties,
                                         f.domain_activity, f.carrier_location};
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_formation_reservations);
    auto attached = decode_military_formation_reservations(
        field(source, "military_formation_reservations", "state"), parsed.value,
        military_versions.value.military_formation_reservations,
        catalog.dynamic_profile == DynamicProfile::local_society_v1, resources);
    if (!attached.ok)
      return {false, {}, std::move(attached.error)};
  }
  auto reservations = validate_military_formation_reservation_layout(
      parsed.value, military_versions.value.military_formation_reservations);
  if (!reservations.ok)
    return {false, {}, std::move(reservations.error)};
  if (military_versions.value.military_action_conditions) {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_action_conditions);
    auto attached = decode_military_action_conditions(
        field(source, "military_action_conditions", "state"), parsed.value,
        military_versions.value.military_action_conditions);
    if (!attached.ok)
      return {false, {}, std::move(attached.error)};
  }
  auto conditions = validate_military_action_condition_layout(
      parsed.value, military_versions.value.military_action_conditions);
  if (!conditions.ok)
    return {false, {}, std::move(conditions.error)};
  if (military_versions.value.military_training_completion_time) {
    ScopedCheckpointJsonDecode original(
        CheckpointWorldJsonSection::military_training_completion_time);
    auto attached = decode_military_training_completion_time(
        field(source, "military_training_completion_time", "state"),
        parsed.value, 1);
    if (!attached.ok)
      return {false, {}, std::move(attached.error)};
  }
  auto training = validate_military_training_completion_time_layout(
      parsed.value, military_versions.value.military_training_completion_time);
  if (!training.ok)
    return {false, {}, std::move(training.error)};
  result.military_action_time =
      checkpoint_make_shared<const MilitaryActionTimeState>(
          std::move(parsed.value));
} else if (source.find("military_action_time"))
  fail("UnexpectedMilitaryActionTimeState", "military_action_time");
if (!military_versions.value.military_action_resources &&
    source.find("military_action_resources"))
  fail("UnexpectedMilitaryActionResourceState", "military_action_resources");
if (!military_versions.value.military_return_detachments &&
    source.find("military_return_detachments"))
  fail("UnexpectedMilitaryReturnDetachmentState",
       "military_return_detachments");
if (!military_versions.value.military_action_conditions &&
    source.find("military_action_conditions"))
  fail("UnexpectedMilitaryActionConditionState", "military_action_conditions");
if (!military_versions.value.military_training_completion_time &&
    source.find("military_training_completion_time"))
  fail("UnexpectedMilitaryTrainingCompletionState",
       "military_training_completion_time");
if (!military_versions.value.military_formation_reservations &&
    source.find("military_formation_reservations"))
  fail("UnexpectedMilitaryFormationReservationState",
       "military_formation_reservations");
