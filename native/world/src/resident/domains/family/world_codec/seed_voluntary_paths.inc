// 家庭世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.family_voluntary_paths) {
  auto decoded = decode_voluntary_path_state(
      field(source, "family_voluntary_paths", "state"));
  if (!decoded.ok)
    return {false, {}, decoded.error};
  result.family_voluntary_paths =
      std::make_shared<const VoluntaryPathState>(std::move(decoded.value));
} else if (source.find("family_voluntary_paths"))
  fail("UnexpectedVoluntaryPathDomain", "family_voluntary_paths");
{
  CommonStore view(result.common, catalog.common);
  auto checked = validate_voluntary_path_json_payloads(
      view, result.family_voluntary_paths.get(), source);
  if (!checked.ok)
    return {false, {}, checked.error};
}
