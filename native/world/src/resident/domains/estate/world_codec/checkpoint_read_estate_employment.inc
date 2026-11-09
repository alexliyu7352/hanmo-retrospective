// 庄园检查点恢复分派：沿原 else-if 链读取
// estate_employment；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "estate_employment") {
  auto state = make_state<Seed, EstateEmploymentState>();
  r = visit_estate_employment_checkpoint(reader, *state);
  seed.estate_employment = std::move(state);
}