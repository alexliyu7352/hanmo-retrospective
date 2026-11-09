// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto tactic_lessons =
    decode_growth_tactic_lesson_profile(scenario, required_capabilities());
if (!tactic_lessons.ok)
  return {false, {}, tactic_lessons.error};
if (tactic_lessons.value) {
  if (!result->family || !result->family->enabled || !result->family_life ||
      !result->growth || !result->growth->cultivation || !result->military ||
      !result->military->battle_catalog.tactic_learning)
    fail("GrowthTacticLessonProfileRequired", "scenario");
  auto parsed = decode_growth_tactic_lesson_catalog(
      field(field(scenario, "dynamic_world", "scenario"),
            "growth_tactic_lessons", "dynamic_world"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result->growth_tactic_lessons =
      checkpoint_make_shared<const GrowthTacticLessonCatalog>(
          std::move(parsed.value));
}
