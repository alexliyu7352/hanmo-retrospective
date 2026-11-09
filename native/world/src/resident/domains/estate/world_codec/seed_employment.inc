// 庄园世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.estate_employment) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::estate_employment);
  auto state = decode_estate_employment_state(
      field(source, "estate_employment", "state"));
  if (!state.ok)
    return {false, {}, state.error};
  result.estate_employment =
      std::make_shared<const EstateEmploymentState>(std::move(state.value));
} else if (source.find("estate_employment"))
  fail("UnexpectedEstateEmploymentState", "state/estate_employment");
