// 清算命令自有feature位和精确四种语义；预览不接受他人代签，旧Shop命令布局不增标签。
#pragma once
#include "private_shop_liquidation_codec.hpp"
namespace hanmo::resident {
inline constexpr std::uint32_t private_shop_liquidation_checkpoint_feature=4194304u;
using PrivateShopLiquidationCommand=std::variant<PrivateShopAppointExecutor,PrivateShopAcceptExecutorship,PrivateShopRevokeExecutor,PrivateShopEstateClose>;
struct PrivateShopLiquidationQuote {WorldIdentity source_identity;StableId actor_id;PrivateShopLiquidationCommand command;std::variant<PrivateShopExecutorQuote,PrivateShopEstateClosingQuote> data;};
Status validate_private_shop_liquidation_pair(bool enabled,const std::vector<std::pair<StableId,StableId>>& modules);
StableId private_shop_liquidation_command_kind(const PrivateShopLiquidationCommand&);
Outcome<PrivateShopLiquidationCommand> decode_private_shop_liquidation_command(const StableId& kind,const BoundaryValue& params);
BoundaryValue encode_private_shop_liquidation_command(const PrivateShopLiquidationCommand&);
Outcome<PrivateShopLiquidationQuote> preview_private_shop_liquidation_command(const PrivateShopContext&,const PrivateShopLiquidationContext&,const PrivateShopMeetingContext&,const StableId& actor,const PrivateShopLiquidationCommand&);
BoundaryValue encode_private_shop_liquidation_quote(const PrivateShopLiquidationQuote&);
// 缺依赖不把已声明清算静默当作关闭；无声明的旧profile仍返回空。
// 只核不可变原文书目录能实际送达指定执行人；不制造已收信状态或替代运行知情口。
Status validate_private_shop_liquidation_notice_routes(const PrivateShopLiquidationCatalog&,const TaskCatalog&,const TaskMessageNoticeCatalog&);
Outcome<std::optional<PrivateShopLiquidationCatalog>> decode_private_shop_liquidation_profile(const BoundaryValue& scenario,const std::vector<StableId>& capabilities);
}
