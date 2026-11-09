// 家庭检查点恢复分派：沿原 else-if 链读取
// family_apprenticeship；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "family_apprenticeship") {
  auto state = make_state<Seed, FamilyApprenticeshipState>();
  r = visit_family_apprenticeship_checkpoint(reader, *state);
  seed.family_apprenticeship = std::move(state);
}