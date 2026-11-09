// 军事世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (military_tactic_learning_enabled(catalog)) {
  auto learned = military_tactic_learning_codec::decode(
      field(source, military_tactic_learning_module, "state"));
  if (!learned.ok)
    return {false, {}, learned.error};
  result.military_tactic_learning =
      std::make_shared<const MilitaryTacticLearningState>(
          std::move(learned.value));
} else if (source.find(military_tactic_learning_module))
  fail("UnexpectedMilitaryTacticLearning", military_tactic_learning_module);
auto battle_catalog =
    validate_native_military_battle_lifecycle_catalog(catalog);
if (!battle_catalog.ok)
  return battle_catalog;
auto battle_lifecycle =
    selected_native_military_battle_lifecycle(catalog.identity);
if (!battle_lifecycle.ok)
  return {false, {}, std::move(battle_lifecycle.error)};
const BoundaryValue *battle_lifecycle_source = nullptr;
for (const auto &row : object(source, "state"))
  if (row.first == "military_battle_lifecycle") {
    if (battle_lifecycle_source)
      fail("DuplicateMilitaryBattleLifecycleSection",
           "military_battle_lifecycle");
    battle_lifecycle_source = &row.second;
  }
if (bool(battle_lifecycle_source) != bool(battle_lifecycle.value))
  fail("MilitaryBattleLifecycleSelectedSectionMismatch",
       "military_battle_lifecycle");
auto m = decode_military_seed(
    source, catalog.selected_military_occupation_schema,
    MilitaryHeaderBootstrapSelection{
        military_versions.value.military_authority_time,
        military_versions.value.military_action_time,
        military_versions.value.military_training_completion_time,
        battle_lifecycle.value, result.military_action_time.get(),
        result.field_battle.get()});
if (!m.ok)
  return {false, {}, m.error};
if (siege_enabled(catalog)) {
  auto decisions = decode_siege_integration_state(
      field(source, "siege_integration", "state"));
  if (!decisions.ok)
    return {false, {}, decisions.error};
  m.value.siege_integration = std::move(decisions.value);
  auto transfers = decode_military_integration_facts(
      field(source, "military_integration", "state"));
  if (!transfers.ok)
    return {false, {}, transfers.error};
  m.value.integration_facts = std::move(transfers.value);
} else if (source.find("siege_integration") ||
           source.find("military_integration"))
  fail("UnexpectedSiegeIntegrationState", "state");
if (military_versions.value.military_authority_time) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::military_authority_time);
  auto attached = decode_military_authority_time(
      field(source, "military_authority_time", "state"), m.value, 1);
  if (!attached.ok)
    return {false, {}, std::move(attached.error)};
} else if (source.find("military_authority_time"))
  fail("UnexpectedMilitaryAuthorityTimeState", "military_authority_time");
if (battle_lifecycle_source) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::military_battle_lifecycle);
  auto attached = decode_military_battle_lifecycle(
      *battle_lifecycle_source, m.value, battle_lifecycle.value);
  if (!attached.ok)
    return attached;
}
auto battle_layout =
    validate_military_battle_lifecycle_layout(m.value, battle_lifecycle.value);
if (!battle_layout.ok)
  return battle_layout;
result.military = std::make_shared<const MilitaryState>(std::move(m.value));
{
  CommonStore original(result.common, catalog.common);
  auto training = validate_native_military_training_completion_headers(
      original, *result.military, result.military_action_time.get(),
      military_versions.value.military_training_completion_time);
  if (!training.ok)
    return {false, {}, std::move(training.error)};
}
if (result.military_action_time) {
  CommonStore original(result.common, catalog.common);
  auto reservations = validate_military_formation_reservation_sources(
      original, *catalog.military, *result.military_action_time,
      military_versions.value.military_formation_reservations);
  if (!reservations.ok)
    return {false, {}, std::move(reservations.error)};
}
for (const auto &row : result.common.facts.rows())
  if (row.second.kind == "campaign_accepted" ||
      row.second.kind == "military_office_completion_applied" ||
      row.second.kind == "military_formation_completion_checked") {
    if (!result.military_action_time)
      return {
          false,
          {},
          common_failure("MilitaryActionTimeSelectedSchemaRequired", row.first)
              .error};
    auto checked = validate_military_authority_header_source(
        row.second, *result.military, *result.military_action_time,
        military_versions.value.military_authority_time,
        military_versions.value.military_action_time);
    if (!checked.ok)
      return {false, {}, std::move(checked.error)};
  }
if (catalog.naval) {
  auto n = naval_codec::decode(field(source, "military_naval", "state"),
                               *catalog.naval);
  if (!n.ok)
    return {false, {}, n.error};
  result.naval = std::make_shared<const NavalState>(std::move(n.value));
} else if (source.find("military_naval"))
  fail("UnexpectedNavalState", "state/military_naval");
// 修缮行动的业务载荷必须与原工单一致；typed
// checkpoint恢复后仍走同一JSON边界。
if (catalog.naval && catalog.naval->repair_policy) {
  CommonStore view(result.common, catalog.common);
  auto payloads = naval_repair_codec::action_payloads(
      view, result.naval->repairs, bool(catalog.naval->repair_charge_policy));
  if (!payloads.ok)
    return {false, {}, payloads.error};
  for (const auto &row : object(payloads.value, "naval_repair/actions")) {
    const auto &actual =
        field(field(field(source, "actions", "state"), row.first, "actions"),
              "params", row.first);
    if (!compare_boundary(actual, row.second).exact())
      fail("NavalRepairActionPayloadMismatch", row.first);
  }
}
