// 人物底座世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto original_injury_layout = validate_native_character_injury_layout(w);
if (!original_injury_layout.ok)
  return original_injury_layout;
if (w.catalog->exact_time) {
  if (!w.world_timeline || !w.action_elapsed || !w.actor_foundation)
    return {false, {}, common_failure("MissingExactState").error};
  r = section("world_timeline", [&](auto &c) {
    return visit_world_timeline2(c, *w.world_timeline);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("action_elapsed", [&](auto &c) {
    return visit_action_elapsed2(c, *w.action_elapsed);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("actor_start_context", [&](auto &c) {
    return exact_complete_selected(*w.catalog->exact_time)
               ? visit_actor_start_context_v3(
                     c, w.actor_foundation->actor_start_context)
               : visit_actor_start_context_v2(
                     c, w.actor_foundation->actor_start_context);
  });
  if (!r.ok)
    return {false, {}, r.error};
  r = section("character_origin_records", [&](auto &c) {
    return visit_character_origin_records_v1(
        c, w.actor_foundation->character_origin_records,
        exact_complete_selected(*w.catalog->exact_time)
            ? ActorOriginWireProfile::complete_origin2
            : ActorOriginWireProfile::authored_only);
  });
  if (!r.ok)
    return {false, {}, r.error};
  if (w.catalog->growth->complete_capability_application_chain) {
    r = validate_native_growth_permanent_layout(w);
    if (!r.ok)
      return r;
    r = section("growth_permanent_contributions", [&](auto &c) {
      return visit_growth_permanent_section(
          c, *w.growth_permanent, w.card_acquisitions,
          GrowthPermanentSectionVersion::trade_grants);
    });
    if (!r.ok)
      return r;
  }
  r = section("character_condition_extensions", [&](auto &c) {
    if (w.character_injury_sources)
      return visit_character_condition_extensions_v8(
          c, w.actor_foundation->character_condition_extensions);
    return w.catalog->growth->complete_capability_application_chain
               ? visit_character_condition_extensions_v7(
                     c, w.actor_foundation->character_condition_extensions)
               : visit_character_condition_extensions_v1(
                     c, w.actor_foundation->character_condition_extensions,
                     exact_complete_selected(*w.catalog->exact_time)
                         ? ConditionWireProfile::receipt_v2_union
                         : ConditionWireProfile::legacy_rest_only);
  });
  if (!r.ok)
    return {false, {}, r.error};
}

if (w.character_injury_sources) {
  r = section("character-injury-sources", [&](auto &c) {
    return visit_character_injury_sources_v1(c, *w.character_injury_sources);
  });
  if (!r.ok)
    return r;
}
