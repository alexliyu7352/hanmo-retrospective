// 地方经济检查点恢复分派：沿原 else-if 链读取
// economic_takeover_execution；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "economic_takeover_execution") {
  auto state = make_state<Seed, EconomicTakeoverExecutionState>();
  r = visit_economic_takeover_execution_checkpoint(reader, *state);
  seed.economic_takeover_execution = std::move(state);
}