// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (w.catalog->exact_time && exact_complete_selected(*w.catalog->exact_time)) {
  WORLD_WHOLE(P::split, "growth",
              w.catalog->growth->complete_capability_application_chain
                  ? emit_growth_trade_grant_state_projection(
                        out, "", w.growth, *w.catalog->growth)
              : w.catalog->growth->complete_exact_rest_mouth_sources
                  ? emit_growth_exact_rest_mouth_state_projection(
                        out, "", w.growth, *w.catalog->growth)
              : w.catalog->growth->complete_exact_command_cuts
                  ? emit_growth_exact_command_cut_state_projection(
                        out, "", w.growth, *w.catalog->growth)
                  : emit_growth_exact_period_state_projection(
                        out, "", w.growth, *w.catalog->growth));
} else {
  WORLD_WHOLE(P::split, "growth",
              emit_growth_state_projection(
                  out, "", w.growth, w.regional_life_enabled(),
                  w.catalog->growth->cards.enabled,
                  w.catalog->growth->teaching_requests.enabled,
                  w.catalog->growth->study_teaching_requests.enabled,
                  w.catalog->growth->innate_learning,
                  bool(w.catalog->exact_time)));
}
