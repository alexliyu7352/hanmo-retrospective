// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.political_successions) {
  auto parsed = decode_political_succession_state(
      field(source, political_succession_domain, "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_successions =
      std::make_shared<const PoliticalSuccessionState>(std::move(parsed.value));
} else if (source.find(political_succession_domain))
  fail("UnexpectedPoliticalSuccessionDomain", political_succession_domain);
