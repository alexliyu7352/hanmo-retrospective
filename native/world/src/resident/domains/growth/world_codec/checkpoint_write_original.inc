// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
r = section("growth", [&](auto &c) {
  return w.catalog->exact_time &&
                 exact_complete_selected(*w.catalog->exact_time)
             ? (w.catalog->growth->complete_capability_application_chain
                    ? visit_growth_trade_grant_checkpoint(
                          c, w.growth, *w.catalog->growth)
                : w.catalog->growth->complete_exact_rest_mouth_sources
                    ? visit_growth_exact_rest_mouth_checkpoint(
                          c, w.growth, *w.catalog->growth)
                : w.catalog->growth->complete_exact_command_cuts
                    ? visit_growth_exact_command_cut_checkpoint(
                          c, w.growth, *w.catalog->growth)
                    : visit_growth_exact_period_checkpoint(c, w.growth,
                                                           *w.catalog->growth))
         : w.catalog->exact_time
             ? visit_growth_fixed_service_checkpoint(
                   c, w.growth, w.regional_life_enabled(),
                   w.catalog->growth->cards.enabled)
             : visit_growth_checkpoint(c, w.growth, w.regional_life_enabled(),
                                       w.catalog->growth->cards.enabled);
});
if (!r.ok)
  return {false, {}, r.error};
