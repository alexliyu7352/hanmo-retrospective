// 地方经济检查点恢复分派：沿原 else-if 链读取
// occupation_fiscal_settlement；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "occupation_fiscal_settlement") {
  auto state = make_state<Seed, OccupationFiscalSettlementState>();
  r = visit_occupation_fiscal_settlement_checkpoint(reader, *state);
  seed.occupation_fiscal_settlement = std::move(state);
}