// 社交世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto s = decode_social_catalog(data);
if (!s.ok)
  return {false, {}, s.error};
auto authored_social_candidate =
    checkpoint_make_shared<SocialCatalog>(std::move(s.value));
result->social = authored_social_candidate;
