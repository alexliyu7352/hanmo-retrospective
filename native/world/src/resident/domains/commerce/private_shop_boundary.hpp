// 玩家与NPC只提交具名私铺命令，价格、责任、技能和资产字段由原领域重报价，不收余额setter。
#pragma once
#include "private_shop_closing.hpp"
#include "private_shop_operation.hpp"
#include "private_shop_staff.hpp"
#include "private_shop_orders.hpp"
#include "private_shop_restock.hpp"
namespace hanmo::resident {
struct PrivateShopHouseholdCustodyContext;
// 明版营业输入只表达本人的明确托管选择；摘要与容器由原目录和实际营业读取。
struct PrivateShopOperateWithCustody {
  StableId shop_id, item_id, custody_policy_id;
  Integer custody_policy_version = 1;
};
struct PrivateShopCancelOperation {
  StableId action_id;
};
using PrivateShopCommand = std::variant<
    PrivateShopOpen, PrivateShopFund, PrivateShopSetMarkup, PrivateShopRestock,
    PrivateShopOperate, PrivateShopCancelOperation, PrivateShopEmploy,
    PrivateShopStaffAccept, PrivateShopStaffDecision, PrivateShopRequestOrder,
    PrivateShopClose, PrivateShopOperateWithCustody>;
struct PrivateShopCancelQuote {
  StableId action_id;
};
using PrivateShopQuoteData =
    std::variant<PrivateShopOpeningQuote, PrivateShopManagementQuote,
                 PrivateShopRestockAdmission, PrivateShopOperationQuote,
                 PrivateShopCancelQuote, PrivateShopStaffQuote,
                 PrivateShopOrderQuote, PrivateShopClosingQuote>;
struct PrivateShopCommandQuote {
  WorldIdentity source_identity;
  StableId actor_id;
  PrivateShopCommand command;
  PrivateShopQuoteData data;
};
// 所有借口均同World候选；Growth报价仍唯一负责原市场购买价格和实际接触许可。
struct PrivateShopCommandContext {
  const PrivateShopContext &shops;
  const PrivateShopMeetingContext &meeting;
  const GrowthState &growth;
  const SocialState &social;
  const GrowthAccess &growth_access;
  const PrivateShopHouseholdCustodyContext *private_custody = nullptr;
};
// 从实际选中目录取唯一政策原引用，不接受调用者输入摘要或未来事实。
Outcome<ActorFoundationRuleRef> read_private_shop_explicit_custody_policy(
    const PrivateShopHouseholdCustodyContext &,
    const PrivateShopOperateWithCustody &);
Outcome<PrivateShopCommand> decode_private_shop_command(const StableId &kind,
                                                        const BoundaryValue &);
StableId private_shop_command_kind(const PrivateShopCommand &);
BoundaryValue encode_private_shop_command(const PrivateShopCommand &);
Outcome<PrivateShopCommandQuote>
preview_private_shop_command(const PrivateShopCommandContext &,
                             const StableId &actor, const PrivateShopCommand &);
BoundaryValue encode_private_shop_quote(const PrivateShopCommandQuote &);
} // namespace hanmo::resident
