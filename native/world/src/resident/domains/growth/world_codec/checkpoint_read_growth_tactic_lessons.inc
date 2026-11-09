// 成长检查点恢复分派：沿原 else-if 链读取
// growth_tactic_lessons；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "growth_tactic_lessons") {
  auto state = make_state<Seed, GrowthTacticLessonState>();
  r = visit_growth_tactic_lessons_checkpoint(reader, *state);
  seed.growth_tactic_lessons = std::move(state);
}