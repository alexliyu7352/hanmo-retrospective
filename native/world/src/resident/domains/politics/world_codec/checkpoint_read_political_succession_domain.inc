// 政治检查点恢复分派：沿原 else-if 链读取 political_succession_domain,
// political_head_handover_domain, political_successor_rounds_domain,
// political_term_handover_domain；不改变段顺序、所有权或拒绝条件。
else if (section.domain == political_succession_domain) {
  auto state = make_state<Seed, PoliticalSuccessionState>();
  r = visit_political_succession_checkpoint(reader, *state);
  seed.political_successions = std::move(state);
}
else if (section.domain == political_head_handover_domain) {
  auto state = make_state<Seed, PoliticalHeadHandoverState>();
  r = visit_political_head_handover_checkpoint(reader, *state);
  seed.political_head_handovers = std::move(state);
}
else if (section.domain == political_successor_rounds_domain) {
  auto state = make_state<Seed, PoliticalSuccessorRoundState>();
  r = visit_political_successor_round_checkpoint(reader, *state);
  seed.political_successor_rounds = std::move(state);
}
else if (section.domain == political_term_handover_domain) {
  auto state = make_state<Seed, PoliticalTermHandoverState>();
  r = visit_political_term_handover_checkpoint(reader, *state);
  seed.political_term_handovers = std::move(state);
}