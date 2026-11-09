// 地方经济检查点恢复分派：沿原 else-if 链读取 fiscal_taxpayer_continuity,
// fiscal_settlement, public_relief；不改变段顺序、所有权或拒绝条件。
else if (section.domain == "fiscal_taxpayer_continuity") {
  auto state = make_state<Seed, FiscalTaxpayerContinuityState>();
  r = visit_fiscal_taxpayer_continuity_checkpoint(reader, *state);
  seed.fiscal_taxpayer_continuity = std::move(state);
}
else if (section.domain == "fiscal_settlement") r =
    visit_fiscal_settlement_checkpoint(reader, *settlement);
else if (section.domain == "public_relief") r =
    visit_public_relief_checkpoint(reader, *relief);
