// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto life_age_catalog = validate_native_life_age_catalog(catalog);
if (!life_age_catalog.ok)
  return {false, {}, std::move(life_age_catalog.error)};
if (catalog.life_age_checks) {
  auto parsed =
      decode_life_age_checks(field(source, "life_age_checks", "state"));
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  result.life_age_checks =
      checkpoint_make_shared<const LifeAgeState>(std::move(parsed.value));
} else if (source.find("life_age_checks"))
  fail("UnexpectedLifeAgeState", "life_age_checks");
if (catalog.control_continuation) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::control_continuation);
  const auto &value = field(source, "control_continuation", "state");
  auto parsed = catalog.control_continuation->selected_version == 3
                    ? decode_control_continuation_v3(value)
                    : decode_control_continuation_v2(value);
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  result.control_continuation =
      checkpoint_make_shared<const ControlContinuationState>(
          std::move(parsed.value));
} else if (source.find("control_continuation"))
  fail("UnexpectedControlContinuationState", "control_continuation");
