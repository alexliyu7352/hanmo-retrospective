// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (politics_enabled(catalog)) {
  auto selected =
      read_political_locked_body_version(catalog.identity, "politics");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value)
    fail("PoliticalPhysicalModuleRequired", "politics");
  auto politics =
      selected.value == 2
          ? decode_politics2(field(source, "politics", "state"))
          : decode_politics_state(field(source, "politics", "state"));
  if (!politics.ok)
    return {false, {}, politics.error};
  result.politics =
      std::make_shared<const PoliticsState>(std::move(politics.value));
} else if (source.find("politics"))
  fail("UnexpectedPoliticsState", "politics");
if (treaty_passage_enabled(catalog)) {
  auto decoded =
      decode_treaty_passage_state(field(source, "treaty_passage", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result.treaty_passage =
      std::make_shared<const TreatyPassageState>(std::move(decoded.value));
} else if (source.find("treaty_passage"))
  fail("UnexpectedTreatyPassageState", "state");
if (occupation_fiscal_enabled(catalog)) {
  auto political = decode_occupation_fiscal_state(
      field(source, "political_occupation_fiscal", "state"));
  if (!political.ok)
    return {false, {}, political.error};
  result.occupation_fiscal =
      std::make_shared<const OccupationFiscalState>(std::move(political.value));
  auto economic = decode_occupation_fiscal_settlement(
      field(source, "occupation_fiscal_settlement", "state"));
  if (!economic.ok)
    return {false, {}, economic.error};
  result.occupation_fiscal_settlement =
      std::make_shared<const OccupationFiscalSettlementState>(
          std::move(economic.value));
} else if (source.find("political_occupation_fiscal") ||
           source.find("occupation_fiscal_settlement"))
  fail("UnexpectedOccupationFiscalState", "state");
if (political_fiscal_enabled(catalog)) {
  auto selected =
      read_political_locked_body_version(catalog.identity, "political_fiscal");
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value || selected.value != catalog.political_fiscal->version)
    fail("PoliticalFiscalPhysicalVersionMismatch", "political_fiscal");
  auto fiscal = selected.value == 2
                    ? decode_political_fiscal_state2(
                          field(source, "political_fiscal", "state"))
                    : decode_political_fiscal_state(
                          field(source, "political_fiscal", "state"));
  if (!fiscal.ok)
    return {false, {}, fiscal.error};
  result.political_fiscal =
      std::make_shared<const PoliticalFiscalState>(std::move(fiscal.value));
} else if (source.find("political_fiscal"))
  fail("UnexpectedPoliticalFiscalState", "political_fiscal");
