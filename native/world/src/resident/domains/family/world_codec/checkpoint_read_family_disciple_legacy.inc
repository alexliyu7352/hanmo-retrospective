// 家庭检查点恢复分派：沿原 else-if 链读取
// family_disciple_legacy；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family_disciple_legacy") {
  auto state = make_state<Seed, DiscipleControlState>();
  r = visit_disciple_control_checkpoint(reader, *state);
  seed.family_disciple_legacy = std::move(state);
}