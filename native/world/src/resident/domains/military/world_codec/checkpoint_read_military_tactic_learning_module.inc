// 军事检查点恢复分派：沿原 else-if 链读取
// military_tactic_learning_module；不改变段顺序、所有权或拒绝条件。
else if (section.domain == military_tactic_learning_module) {
  auto learned = make_state<Seed, MilitaryTacticLearningState>();
  r = military_tactic_learning_codec::visit(reader, *learned,
                                            section.codec_version);
  seed.military_tactic_learning = std::move(learned);
}