// 日租仅有原欠款自愿偿还和本人明确复业；外层仍须逐精确 kind 经过原公共占用门。
#pragma once
#include "private_shop_daily_rent_business.hpp"
#include "private_shop_daily_rent_cached_receipt.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t private_shop_daily_rent_checkpoint_feature=536870912u;
struct ReceiptDetails;
Status validate_private_shop_daily_rent_receipt(const StableId& kind,const ReceiptDetails&);
StableId private_shop_daily_rent_command_kind(const PrivateShopDailyRentCommand&);
Outcome<PrivateShopDailyRentCommand> decode_private_shop_daily_rent_command(const StableId& kind,const BoundaryValue& params);
BoundaryValue encode_private_shop_daily_rent_command(const PrivateShopDailyRentCommand&);
BoundaryValue encode_private_shop_daily_rent_quote(const PrivateShopDailyRentQuote&);
Status validate_private_shop_daily_rent_pair(bool enabled,const std::vector<std::pair<StableId,StableId>>& module_versions);
}
