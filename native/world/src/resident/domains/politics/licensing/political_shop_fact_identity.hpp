// 原六表及既有开店头的唯一ID前缀映射；借用静态文本，不创建暂存字符串。
#pragma once
#include <string_view>
namespace hanmo::resident::shop_law_detail {
inline std::string_view prefix_view(std::string_view k) {
  if (k == "political_shop_registry_initialized")
    return "political_shop_registry";
  if (k == "political_wanted_registered")
    return "political_wanted";
  if (k == "political_wanted_released")
    return "political_wanted_release";
  if (k == "political_shop_clearance_recorded")
    return "political_shop_clearance";
  if (k == "political_shop_permit_issued")
    return "political_shop_permit";
  if (k == "political_shop_permit_revoked")
    return "political_shop_revocation";
  if (k == "private_shop_opened")
    return "private_shop";
  return {};
}
} // namespace hanmo::resident::shop_law_detail
