// 政治检查点恢复分派：沿原 else-if 链读取
// political_governance_continuations；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "political_governance_continuations") {
  auto state = make_state<Seed, PoliticalGovernanceContinuationState>();
  r = visit_political_governance_continuations_checkpoint(reader, *state);
  seed.political_governance_continuations = std::move(state);
}