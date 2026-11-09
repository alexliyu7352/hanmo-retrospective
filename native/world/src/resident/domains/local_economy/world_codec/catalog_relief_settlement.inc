// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (const auto *config = dynamic->find("public_relief")) {
  auto decoded = decode_public_relief_catalog(*config);
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->public_relief = checkpoint_make_shared<const PublicReliefCatalog>(
      std::move(decoded.value));
}
if (const auto *config = dynamic->find("fiscal_settlement")) {
  if (!political_fiscal_enabled(*result))
    fail("FiscalSettlementRequiresPoliticalFiscal",
         "dynamic_world/fiscal_settlement");
  auto decoded = [&] {
    std::optional<ScopedCheckpointJsonDecode> scope;
    if (ScopedCheckpointBoundaryDecode::current())
      scope.emplace(CheckpointCatalogJsonKind::fiscal_settlement);
    return decode_fiscal_settlement_catalog(*config);
  }();
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result->fiscal_settlement =
      checkpoint_make_shared<const FiscalSettlementCatalog>(
          std::move(decoded.value));
}
// 领域共同开启；真实家庭、公库与规则交叉校验在新局和WorldBundle发布前执行。
