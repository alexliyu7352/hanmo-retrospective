// 家庭检查点恢复分派：沿原 else-if 链读取
// family_biological_legacy；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family_biological_legacy") {
  auto state = make_state<Seed, BiologicalControlState>();
  r = visit_biological_control_checkpoint(reader, *state);
  seed.family_biological_legacy = std::move(state);
}