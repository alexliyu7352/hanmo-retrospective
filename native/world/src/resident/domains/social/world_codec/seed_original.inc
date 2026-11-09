// 社交世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto s = decode_native_social_seed(source, catalog);
if (!s.ok)
  return {false, {}, s.error};
result.social = std::make_shared<const SocialState>(std::move(s.value));
