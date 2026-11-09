// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.family_apprenticeship) {
  auto parsed = decode_family_apprenticeship(
      field(source, "family_apprenticeship", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.family_apprenticeship =
      std::make_shared<const FamilyApprenticeshipState>(
          std::move(parsed.value));
} else if (source.find("family_apprenticeship"))
  fail("UnexpectedApprenticeshipState", "family_apprenticeship");
if (catalog.family_disciple_legacy) {
  auto parsed = decode_disciple_control_state(
      field(source, "family_disciple_legacy", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.family_disciple_legacy =
      std::make_shared<const DiscipleControlState>(std::move(parsed.value));
} else if (source.find("family_disciple_legacy"))
  fail("UnexpectedDiscipleControlState", "family_disciple_legacy");
if (catalog.family_biological_legacy) {
  auto parsed = decode_biological_control_state(
      field(source, "family_biological_legacy", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.family_biological_legacy =
      std::make_shared<const BiologicalControlState>(std::move(parsed.value));
} else if (source.find("family_biological_legacy"))
  fail("UnexpectedBiologicalControlState", "family_biological_legacy");
if (catalog.family_collateral_legacy) {
  auto parsed = decode_collateral_control_state(
      field(source, "family_collateral_legacy", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.family_collateral_legacy =
      std::make_shared<const CollateralControlState>(std::move(parsed.value));
} else if (source.find("family_collateral_legacy"))
  fail("UnexpectedCollateralControlState", "family_collateral_legacy");
