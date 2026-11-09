// 政治世界编解码片段：保持原调用位置、字段与首错顺序；仅由 world_codec.cpp
// 登记包含。
if (catalog.political_shop_licensing) {
  auto selected =
      selected_native_political_shop_licensing_version(catalog.identity);
  if (!selected.ok)
    return {false, {}, std::move(selected.error)};
  if (!selected.value)
    return {
        false, {}, common_failure("PoliticalShopLicensingPairRequired").error};
  const auto &body = field(source, "political_shop_licensing", "state");
  auto parsed = selected.value == 2 ? decode_political_shop_licensing2(body)
                                    : decode_political_shop_licensing(body);
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_shop_licensing =
      std::make_shared<const PoliticalShopLicensingState>(
          std::move(parsed.value));
} else if (source.find("political_shop_licensing"))
  fail("UnexpectedShopMarketDomain", "political_shop_licensing");
if (catalog.political_shop_registry_successions) {
  auto parsed = decode_political_shop_registry_successions(
      field(source, "political_shop_registry_successions", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_shop_registry_successions =
      std::make_shared<const PoliticalShopRegistrySuccessionState>(
          std::move(parsed.value));
} else if (source.find("political_shop_registry_successions"))
  fail("UnexpectedPoliticalShopRegistrySuccessionState",
       "political_shop_registry_successions");
if (catalog.political_shop_licensing_receipts) {
  auto parsed = decode_political_shop_licensing_receipts(
      field(source, "political_shop_licensing_receipts", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_shop_licensing_receipts =
      std::make_shared<const PoliticalShopLicensingReceiptState>(
          std::move(parsed.value));
} else if (source.find("political_shop_licensing_receipts"))
  fail("UnexpectedPoliticalShopLicensingReceipts",
       "political_shop_licensing_receipts");
if (catalog.political_governance_continuations) {
  auto parsed = decode_political_governance_continuations(
      field(source, "political_governance_continuations", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.political_governance_continuations =
      std::make_shared<const PoliticalGovernanceContinuationState>(
          std::move(parsed.value));
} else if (source.find("political_governance_continuations"))
  fail("UnexpectedGovernanceContinuations",
       "political_governance_continuations");
if (catalog.political_governance_invitation_replies) {
  auto parsed = decode_governance_invitation_replies(
      field(source, "political_governance_invitation_replies", "state"));
  if (!parsed.ok)
    return {false, {}, parsed.error};
  result.governance_invitation_replies =
      std::make_shared<const GovernanceInvitationReplyState>(
          std::move(parsed.value));
} else if (source.find("political_governance_invitation_replies"))
  fail("UnexpectedGovernanceInvitationReplies",
       "political_governance_invitation_replies");
