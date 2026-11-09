// 商业世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
auto funding_catalog =
    validate_native_private_transport_funding_catalog(catalog);
if (!funding_catalog.ok)
  return {false, {}, std::move(funding_catalog.error)};
if (catalog.private_transport_funding_selected) {
  ScopedCheckpointJsonDecode original(
      CheckpointWorldJsonSection::private_transport_funding);
  auto parsed = decode_private_transport_funding(
      field(source, "private_transport_funding", "state"));
  if (!parsed.ok)
    return {false, {}, std::move(parsed.error)};
  result.private_transport_funding =
      checkpoint_make_shared<const PrivateTransportFundingState>(
          std::move(parsed.value));
} else if (source.find("private_transport_funding"))
  fail("UnexpectedPrivateTransportFundingState", "private_transport_funding");
