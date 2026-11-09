// 作者目录仍为原版1；显式包装只选择真实来源布局，不改法院或权限值。
#pragma once
#include "political_shop_licensing.hpp"
namespace hanmo::resident {
struct PoliticalShopLicensingAuthorProfile {
  Integer source_version = 1;
  PoliticalShopLicensingCatalog catalog;
};
Outcome<PoliticalShopLicensingAuthorProfile>
decode_political_shop_licensing_author_profile(const BoundaryValue &);
} // namespace hanmo::resident
