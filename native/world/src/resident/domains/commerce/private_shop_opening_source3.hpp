// 原开设实际时刻仅反读本源，不回许可/资格全读，供历史权限校验切断循环。
#pragma once
#include "private_shop.hpp"
namespace hanmo::resident {
class CurrentMomentRead;
// 先拥有原根与唯一时钟，再构造许可与完整 ShopContext；不反借许可形成构造环。
struct PrivateShopOpeningSources3 {
  const CommonStore &common;
  const PrivateShopState &state;
  const PrivateShopCatalog &catalog;
  const CurrentMomentRead &clock;
};
Outcome<const WorldMoment *>
read_private_shop_opening_actual3(const PrivateShopOpeningSources3 &,
                                  const SourceRef &);
Status validate_private_shop_opening_sources3(const PrivateShopContext &);
Status validate_private_shop_opening_time_layout3(const PrivateShopState &);
Outcome<WorldMoment>
read_private_shop_opening_context_moment3(const PrivateShopContext &);
Outcome<WorldMoment>
read_private_shop_opening_write_moment3(const PrivateShopContext &);
Outcome<const WorldMoment *>
read_private_shop_opening_actual3(const PrivateShopContext &,
                                  const SourceRef &);
} // namespace hanmo::resident
