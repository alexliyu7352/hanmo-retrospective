// 成长世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.personal_recovery) {
  auto parsed = decode_growth_recovery_state(
      field(source, "personal_injury_recovery", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.personal_recovery =
      std::make_shared<const GrowthRecoveryState>(std::move(parsed.value));
} else if (source.find("personal_injury_recovery"))
  fail("UnexpectedPersonalInjuryRecoveryState", "personal_injury_recovery");
if (catalog.growth_tactic_lessons) {
  auto parsed = decode_growth_tactic_lessons(
      field(source, "growth_tactic_lessons", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.growth_tactic_lessons =
      std::make_shared<const GrowthTacticLessonState>(std::move(parsed.value));
} else if (source.find("growth_tactic_lessons"))
  fail("UnexpectedGrowthTacticLessonState", "growth_tactic_lessons");
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_growth_tactic_lesson_json_payloads(
      view, result.growth_tactic_lessons.get(), source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
