// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto innate = decode_innate_learning_profile(scenario, required_capabilities());
if (!innate.ok)
  return {false, {}, innate.error};
auto growth = decode_growth_catalog(data, *result->common,
                                    result->regional_life, innate.value);
if (!growth.ok)
  return {false, {}, growth.error};
if (innate.value && (!result->family_birth || !result->family_lineage ||
                     !growth.value.study_segments))
  fail("InnateLearningDependencyRequired", "family_innate_learning");
if (const auto *config = scenario.find("growth_cards")) {
  if (!scenario.find("dynamic_world"))
    fail("CardsNativeProfileRequired", "scenario/growth_cards");
  if (!growth.value.cultivation)
    fail("CardsCultivationRequired", "scenario/growth_cards");
  auto cards = decode_growth_card_catalog(field(data, "cards", "data"), *config,
                                          *result->common);
  if (!cards.ok)
    return {false, {}, cards.error};
  growth.value.cards = std::move(cards.value);
}
if (const auto *config = scenario.find("insight_cards")) {
  if (!scenario.find("dynamic_world"))
    fail("InsightsNativeProfileRequired", "scenario/insight_cards");
  auto cards = decode_growth_insight_card_catalog(
      field(data, "insight_cards", "data"), *config, *result->common);
  if (!cards.ok)
    return {false, {}, cards.error};
  growth.value.insights = std::move(cards.value);
}
if (const auto *config = scenario.find("actor_card_unlocks")) {
  const auto *dynamic = scenario.find("dynamic_world");
  if (!dynamic || !native_existing_actor_policy(
                      get_string(*dynamic, "founder_policy", "dynamic_world")))
    fail("ActorCardsExistingRosterRequired", "scenario/actor_card_unlocks");
  auto actor_cards = decode_growth_actor_start_policy(
      field(data, "actor_cards", "data"), *config, *result->common,
      strings(field(scenario, "playable_actor_ids", "scenario"),
              "playable_actor_ids"),
      get_string(scenario, "player_actor", "scenario"));
  if (!actor_cards.ok)
    return {false, {}, actor_cards.error};
  if (std::any_of(actor_cards.value.cards.definitions.rows().begin(),
                  actor_cards.value.cards.definitions.rows().end(),
                  [](const auto &row) {
                    return row.second.condition ==
                           "completed_envoy_round_trip_v1";
                  })) {
    const auto *diplomacy = dynamic->find("diplomacy");
    if (!diplomacy ||
        !get_bool(*diplomacy, "enabled", "dynamic_world/diplomacy") ||
        get_int(*diplomacy, "version", "dynamic_world/diplomacy") != 2)
      fail("ActorCardEnvoyProfileRequired", "scenario/actor_card_unlocks");
  }
  growth.value.actor_start = std::move(actor_cards.value);
}
if (const auto *config = scenario.find("teaching_requests")) {
  if (!scenario.find("dynamic_world"))
    fail("TeachingNativeProfileRequired", "scenario/teaching_requests");
  if (!growth.value.cultivation)
    fail("TeachingCultivationRequired", "scenario/teaching_requests");
  auto requests = decode_growth_teaching_request_catalog(
      field(data, "teaching_requests", "data"), *config, *result->common,
      *result->interaction);
  if (!requests.ok)
    return {false, {}, requests.error};
  const auto &enabled = field(scenario, "enabled", "scenario");
  const auto actors =
                 strings(field(enabled, "actors", "enabled"), "enabled/actors"),
             roles =
                 strings(field(enabled, "roles", "enabled"), "enabled/roles"),
             dialogues = strings(field(enabled, "dialogues", "enabled"),
                                 "enabled/dialogues");
  for (const auto &row : requests.value.definitions.rows()) {
    const auto &request = row.second;
    for (const auto *actor :
         {&request.teacher_actor_id, &request.student_actor_id})
      if (std::find(actors.begin(), actors.end(), *actor) == actors.end())
        fail("InactiveTeachingRequestActor", row.first);
    auto dialogue = std::find_if(
        result->interaction->dialogues.begin(),
        result->interaction->dialogues.end(),
        [&](const auto &value) { return value.id == request.dialogue_id; });
    if (std::find(dialogues.begin(), dialogues.end(), request.dialogue_id) ==
            dialogues.end() ||
        dialogue == result->interaction->dialogues.end() ||
        std::find(roles.begin(), roles.end(), dialogue->context_role) ==
            roles.end())
      fail("InactiveTeachingRequestDialogue", row.first);
  }
  growth.value.teaching_requests = std::move(requests.value);
}
if (const auto *config = scenario.find("study_teaching_requests")) {
  if (!scenario.find("dynamic_world"))
    fail("StudyTeachingNativeProfileRequired",
         "scenario/study_teaching_requests");
  auto requests = decode_growth_study_teaching_catalog(
      field(data, "study_teaching_requests", "data"), *config, *result->common,
      growth.value, *result->interaction);
  if (!requests.ok)
    return {false, {}, requests.error};
  const auto &enabled = field(scenario, "enabled", "scenario");
  const auto actors =
                 strings(field(enabled, "actors", "enabled"), "enabled/actors"),
             roles =
                 strings(field(enabled, "roles", "enabled"), "enabled/roles"),
             dialogues = strings(field(enabled, "dialogues", "enabled"),
                                 "enabled/dialogues");
  for (const auto &row : requests.value.definitions.rows()) {
    const auto &request = row.second;
    for (const auto *actor :
         {&request.teacher_actor_id, &request.student_actor_id})
      if (std::find(actors.begin(), actors.end(), *actor) == actors.end())
        fail("InactiveStudyTeachingRequestActor", row.first);
    auto dialogue = std::find_if(
        result->interaction->dialogues.begin(),
        result->interaction->dialogues.end(),
        [&](const auto &value) { return value.id == request.dialogue_id; });
    if (std::find(dialogues.begin(), dialogues.end(), request.dialogue_id) ==
            dialogues.end() ||
        dialogue == result->interaction->dialogues.end() ||
        std::find(roles.begin(), roles.end(), dialogue->context_role) ==
            roles.end() ||
        std::find(roles.begin(), roles.end(), request.course_role_id) ==
            roles.end())
      fail("InactiveStudyTeachingRequestDialogue", row.first);
  }
  growth.value.study_teaching_requests = std::move(requests.value);
}
result->growth =
    checkpoint_make_shared<const GrowthCatalog>(std::move(growth.value));
// Native热准备及冷prepare均从同一正规effective
// catalog选中场景取初态；禁止用state人物补默认。 旧非Installed
// seed没有initial时保原准入，纯读会明确拒缺目录，不替其发明空习得。
if (result->growth->cultivation && result->family && result->family->enabled &&
    result->personal->enabled) {
  if (const auto *initial = scenario.find("initial")) {
    auto mastery = make_growth_personal_mastery_initial_catalog(
        *initial, *result->common, get_string(state, "scenario_id", ""));
    if (!mastery.ok)
      return {false, {}, mastery.error};
    result->personal_mastery_initial =
        checkpoint_make_shared<const GrowthPersonalMasteryInitialCatalog>(
            std::move(mastery.value));
  }
}
if (const auto *dynamic = scenario.find("dynamic_world"))
  if (const auto *config = dynamic->find("personal_injury_recovery")) {
    auto unique = validate_succession_json_unique(catalog);
    if (!unique.ok)
      return {false, {}, unique.error};
    const auto &caps = required_capabilities();
    if (!result->regional_life || !result->family_life || !result->family ||
        !result->family->enabled ||
        std::find(caps.begin(), caps.end(), "personal_injury_recovery_v1") ==
            caps.end())
      fail("PersonalInjuryRecoveryProfileRequired",
           "scenario/dynamic_world/personal_injury_recovery");
    auto policy = decode_growth_recovery_catalog(*config, *result->common,
                                                 *result->growth);
    if (!policy.ok)
      return {false, {}, policy.error};
    result->personal_recovery =
        checkpoint_make_shared<const GrowthRecoveryCatalog>(
            std::move(policy.value));
  }
// 只吃正规effective Installed剧本初态；银行当前资产不参与原市场快照。
if (const auto *dynamic = scenario.find("dynamic_world"))
  if (const auto *config = dynamic->find("growth_market_history")) {
    if (!data.find("active_static_actor_ids") ||
        !data.find("active_entity_ids"))
      fail("GrowthMarketEffectiveInstalledCatalogRequired",
           "growth_market_history");
    const auto version = get_int(*config, "version", "growth_market_history");
    auto selected = version == 2 ? decode_growth_market_history_selection_v2(*config)
                                : decode_growth_market_history_selection(*config);
    if (!selected.ok)
      return {false, {}, selected.error};
    auto initial_assets = seed_native_inventory_assets(
        data, field(scenario, "initial", "scenario"));
    if (!initial_assets.ok)
      return {false, {}, initial_assets.error};
    auto market = version == 2
        ? make_growth_market_initial_catalog_v2(*result->common, *result->growth,
            initial_assets.value.inventories, initial_assets.value.next_instance_id, selected.value)
        : make_growth_market_initial_catalog(*result->common, *result->growth,
            initial_assets.value.inventories, initial_assets.value.next_instance_id, selected.value);
    if (!market.ok)
      return {false, {}, market.error};
    auto guarded = checkpoint_make_shared<CommonCatalog>(*result->common);
    if (!guarded->protected_market_inventory_ids.empty())
      fail("GrowthMarketProtectionAlreadyConfigured", "growth_market_history");
    guarded->protected_market_inventory_ids =
        market.value.initial_inventories.order();
    std::sort(guarded->protected_market_inventory_ids.begin(),
              guarded->protected_market_inventory_ids.end());
    if (guarded->protected_market_inventory_ids.empty())
      fail("GrowthMarketInitialSelectionInvalid", "growth_market_history");
    // 后续Time/S1/I-L原选择必须继续修改这个私有同一Common，不能留在旧未保护副本。
    authored_common_candidate = guarded;
    result->common = std::move(guarded);
    // InteractionCatalog是此点唯一已建且自持Common目录的领域；重绑到同一不可变保护目录。
    auto interaction =
        checkpoint_make_shared<InteractionCatalog>(*result->interaction);
    interaction->common = result->common;
    result->interaction = std::move(interaction);
    authored_market_candidate = checkpoint_make_shared<GrowthMarketHistoryCatalog>(std::move(market.value));
    result->growth_market_history = authored_market_candidate;
  }
