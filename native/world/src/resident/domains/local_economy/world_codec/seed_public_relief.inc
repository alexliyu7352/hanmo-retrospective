// 地方经济世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (relief_enabled(catalog)) {
  auto relief =
      decode_public_relief_state(field(source, "public_relief", "state"));
  if (!relief.ok)
    return {false, {}, relief.error};
  result.public_relief =
      std::make_shared<const PublicReliefState>(std::move(relief.value));
} else if (source.find("public_relief"))
  fail("UnexpectedPublicReliefState", "public_relief");
