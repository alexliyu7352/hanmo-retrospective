// 作者必须明确选择永久回执；旧法律1既有档案不补造过去提交来源。
#include "political_shop_licensing_result_bindings.hpp"
#include "resident/domains/politics/takeover/politics_takeover_payload_guard.hpp"
#include "resident/storage/common_codec.hpp"
#include <algorithm>
namespace hanmo::resident {
Status validate_political_shop_licensing_receipt_layout(
    const std::vector<std::pair<StableId, StableId>> &modules, bool enabled,
    const PoliticalShopLicensingReceiptState *state) {
  Integer count = 0, law = 0, shops = 0;
  for (const auto &m : modules) {
    if (m.first == political_shop_licensing_receipts_module) {
      if (m.second != "1")
        return common_failure(
            "UnsupportedPoliticalShopLicensingReceiptsVersion");
      ++count;
    } else if (m.first == "political_shop_licensing") {
      if (m.second != "1" && m.second != "2")
        return common_failure(
            "PoliticalShopLicensingReceiptsDependenciesRequired");
      ++law;
    } else if (m.first == "private_shops") {
      if (m.second != "1" && m.second != "2" && m.second != "3")
        return common_failure(
            "PoliticalShopLicensingReceiptsDependenciesRequired");
      ++shops;
    }
  }
  if (count != (enabled ? 1 : 0) || bool(state) != enabled ||
      (state && state->version != 1))
    return common_failure("PoliticalShopLicensingReceiptsProfileMismatch");
  if (enabled && (law != 1 || shops != 1))
    return common_failure("PoliticalShopLicensingReceiptsDependenciesRequired");
  return common_success();
}
Outcome<bool> decode_political_shop_licensing_receipt_profile(
    const BoundaryValue &scenario, const std::vector<StableId> &capabilities) {
  const auto *dynamic = scenario.find("dynamic_world");
  const auto *group =
      dynamic ? dynamic->find(political_shop_licensing_receipts_module)
              : nullptr;
  if (!group)
    return {true, false, {}};
  auto strict = validate_political_takeover_payload(*group);
  if (!strict.ok)
    return {false, false, strict.error};
  try {
    using namespace boundary;
    const auto &fields = object(*group, "political_shop_licensing_receipts");
    if (fields.size() != 1 ||
        integer(field(*group, "version", "political_shop_licensing_receipts"),
                "political_shop_licensing_receipts/version") != 1)
      return {false, false,
              common_failure("UnsupportedPoliticalShopLicensingReceiptsVersion")
                  .error};
    if (std::find(capabilities.begin(), capabilities.end(),
                  "political_shop_licensing_receipts_v1") == capabilities.end())
      return {false, false,
              common_failure("PoliticalShopLicensingReceiptsCapabilityRequired")
                  .error};
    if (!dynamic->find("political_shop_licensing") ||
        !dynamic->find("private_shops"))
      return {
          false, false,
          common_failure("PoliticalShopLicensingReceiptsDependenciesRequired")
              .error};
    return {true, true, {}};
  } catch (const boundary::DecodeFailure &error) {
    return {false, false, error.error};
  }
}
} // namespace hanmo::resident
