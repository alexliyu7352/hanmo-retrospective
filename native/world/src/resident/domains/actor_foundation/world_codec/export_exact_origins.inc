// 人物底座世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto original_injury_layout = validate_native_character_injury_layout(w);
if (!original_injury_layout.ok)
  return original_injury_layout;
if (w.catalog->exact_time) {
  if (!w.world_timeline || !w.action_elapsed || !w.actor_foundation)
    return common_failure("MissingExactState");
  WORLD_WHOLE(P::section, "world_timeline",
              emit_world_timeline2_projection(out, "", *w.world_timeline));
  WORLD_WHOLE(P::section, "action_elapsed",
              emit_action_elapsed2_projection(out, "", *w.action_elapsed));
  WORLD_WHOLE(P::section, "actor_start_context",
              exact_complete_selected(*w.catalog->exact_time)
                  ? emit_actor_start_context_v3_projection(
                        out, "", w.actor_foundation->actor_start_context)
                  : emit_actor_start_context_v2_projection(
                        out, "", w.actor_foundation->actor_start_context));
  WORLD_WHOLE(P::section, "character_origin_records",
              emit_character_origin_records_v1_projection(
                  out, "", w.actor_foundation->character_origin_records,
                  exact_complete_selected(*w.catalog->exact_time)
                      ? ActorOriginWireProfile::complete_origin2
                      : ActorOriginWireProfile::authored_only));
  if (w.catalog->growth->complete_capability_application_chain)
    WORLD_WHOLE(P::section, "growth_permanent_contributions",
                emit_growth_permanent_section_projection(
                    out, "", *w.growth_permanent, w.card_acquisitions,
                    GrowthPermanentSectionVersion::trade_grants));
  WORLD_WHOLE(
      P::section, "character_condition_extensions",
      w.character_injury_sources
          ? emit_character_condition_extensions_v8_projection(
                out, "", w.actor_foundation->character_condition_extensions)
      : w.catalog->growth->complete_capability_application_chain
          ? emit_character_condition_extensions_v7_projection(
                out, "", w.actor_foundation->character_condition_extensions)
          : emit_character_condition_extensions_v1_projection(
                out, "", w.actor_foundation->character_condition_extensions,
                exact_complete_selected(*w.catalog->exact_time)
                    ? ConditionWireProfile::receipt_v2_union
                    : ConditionWireProfile::legacy_rest_only));
}

if (w.character_injury_sources)
  WORLD_WHOLE(P::section, "character-injury-sources",
              emit_character_injury_sources_v1_projection(
                  out, "", *w.character_injury_sources));
