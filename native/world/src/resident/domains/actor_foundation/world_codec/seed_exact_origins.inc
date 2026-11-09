// 人物底座世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto injury_version =
    selected_native_character_injury_source_version(catalog.identity);
if (!injury_version.ok)
  return {false, {}, std::move(injury_version.error)};
if (injury_version.value &&
    (!catalog.field_battle || !catalog.exact_time ||
     !exact_complete_selected(*catalog.exact_time) || !catalog.growth ||
     !catalog.growth->complete_capability_application_chain))
  return common_failure("CharacterInjuryCompleteGrowth4Required");
if (catalog.exact_time) {
  auto timeline =
      decode_world_timeline2(field(source, "world_timeline", "state"));
  if (!timeline.ok)
    return {false, {}, timeline.error};
  auto elapsed =
      decode_action_elapsed2(field(source, "action_elapsed", "state"));
  if (!elapsed.ok)
    return {false, {}, elapsed.error};
  auto start = exact_complete_selected(*catalog.exact_time)
                   ? decode_actor_start_context_v3(
                         field(source, "actor_start_context", "state"))
                   : decode_actor_start_context_v2(
                         field(source, "actor_start_context", "state"));
  if (!start.ok)
    return {false, {}, start.error};
  auto origin = decode_character_origin_records_v1(
      field(source, "character_origin_records", "state"),
      exact_complete_selected(*catalog.exact_time)
          ? ActorOriginWireProfile::complete_origin2
          : ActorOriginWireProfile::authored_only);
  if (!origin.ok)
    return {false, {}, origin.error};
  auto condition =
      catalog.growth->complete_capability_application_chain
          ? [&] {
              std::optional<ScopedCheckpointJsonDecode> section;
              if (owned_budget)
                section.emplace(*owned_budget, *owned_job, "character_condition_extensions", current_checkpoint_memory());
              return injury_version.value==1
                ? decode_character_condition_extensions_v8(field(source,"character_condition_extensions","state"))
                : decode_character_condition_extensions_v7(field(source,"character_condition_extensions","state"));
            }()
          : decode_character_condition_extensions_v1(
                field(source, "character_condition_extensions", "state"),
                exact_complete_selected(*catalog.exact_time)
                    ? ConditionWireProfile::receipt_v2_union
                    : ConditionWireProfile::legacy_rest_only);
  if (!condition.ok)
    return {false, {}, condition.error};
  auto foundation = std::make_shared<AuthoredActorFoundationState>();
  foundation->actor_start_context = std::move(start.value);
  foundation->character_origin_records = std::move(origin.value);
  foundation->character_condition_extensions = std::move(condition.value);
  result.actor_foundation = std::move(foundation);
  result.world_timeline =
      std::make_shared<const WorldTimelineState>(std::move(timeline.value));
  result.action_elapsed =
      std::make_shared<const ActionElapsedStore>(std::move(elapsed.value));
} else
  for (const auto *key :
       {"world_timeline", "action_elapsed", "actor_start_context",
        "character_origin_records", "character_condition_extensions"})
    if (source.find(key))
      fail("UnexpectedExactState", key);
if (injury_version.value == 1) {
  auto decoded = [&] {
    std::optional<ScopedCheckpointJsonDecode> section;
    if (owned_budget)
      section.emplace(*owned_budget, *owned_job, "character-injury-sources",
                      current_checkpoint_memory());
    return decode_character_injury_sources_v1(
        field(source, "character-injury-sources", "state"));
  }();
  if (!decoded.ok)
    return {false, {}, std::move(decoded.error)};
  result.character_injury_sources =
      checkpoint_make_shared<const CharacterInjurySourcesStateV1>(
          std::move(decoded.value));
} else if (source.find("character-injury-sources"))
  fail("UnexpectedCharacterInjurySources", "character-injury-sources");
auto injury_layout = validate_character_injury_sources_v1_layout(
    catalog.identity, result.character_injury_sources.get());
if (!injury_layout.ok)
  return injury_layout;
if (catalog.exact_time && exact_complete_selected(*catalog.exact_time)) {
  auto claims =
      decode_facility_claims2(field(source, "facility_claims", "state"));
  if (!claims.ok)
    return {false, {}, std::move(claims.error)};
  result.facility_claims =
      std::make_shared<const FacilityClaimsState>(std::move(claims.value));
} else if (source.find("facility_claims"))
  fail("UnexpectedFacilityClaims", "facility_claims");
if (catalog.exact_time && (exact_social_selected(*catalog.exact_time) ||
                           exact_complete_selected(*catalog.exact_time))) {
  ScopedCheckpointJsonDecode original_decode(
      CheckpointWorldJsonSection::social_period_sources);
  auto period = decode_social_period_sources(
      field(source, "social_period_sources", "state"));
  if (!period.ok)
    return {false, {}, period.error};
  result.social_period_sources =
      std::make_shared<const SocialPeriodSources>(std::move(period.value));
} else if (source.find("social_period_sources"))
  fail("UnexpectedSocialPeriodSources", "social_period_sources");
