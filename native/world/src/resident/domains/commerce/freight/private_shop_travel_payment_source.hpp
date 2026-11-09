// 仅匹配本次真实Travel step与Common成功recipe草稿，不授永久运费、货批连续性或报销权。
#pragma once
#include "resident/domains/travel/travel_step_expense.hpp"
#include "resident/storage/resource_execution_draft.hpp"
#include <memory>
namespace hanmo::resident {
class CommonStore;struct ResourceExecutionCaptureIdentity;
// 在已启用Common临时捕获的同一栈、原step之前取得，不能序列化或跨candidate复用。
struct PrivateShopTravelPaymentBoundary {
    WorldIdentity identity;
    ActionCoreView action;
    WalletView payer_before;
    Integer next_instance_id=0;
    std::size_t recipe_begin=0;
    // 免费无recipe也必须属于这次原capture；结束/重开或换候选都不能复用前界。
    const CommonStore* original_common=nullptr;
    std::shared_ptr<const ResourceExecutionCaptureIdentity> capture_identity;
};
// 零价移动仍保真实钱包原语；免费端点没有该原语。金额仅来自匹配后的实际扣款。
struct PrivateShopTravelStepPayment {
    WorldIdentity source_identity;
    StableId action_id,actor_id,payer_wallet_id;
    Integer paid_amount=0;
    bool free_endpoint_finalization=false;
    std::optional<ResourceWalletExecutionDraft> actual_payment;
};
// 调用者须先begin_resource_execution_capture，之后只执行一次原step，并立即调用下口。
// 本前界只拥有原行动/钱包/ID和草稿前缀，不启用捕获、不执行资源写、不改原Travel资格。
Outcome<PrivateShopTravelPaymentBoundary> capture_private_shop_travel_payment_boundary(
    const CommonStore&,const TravelState&,const StableId& action_id);
// 只读同一个真实Common缓冲；任何夹入的recipe、跨clock/result、换钱包或额外分ID均拒绝。
// 不能在接触截停/行动释放后调用，也不能以此取代原起行、价款和永久来源完整验。
Outcome<PrivateShopTravelStepPayment> read_private_shop_travel_step_payment(
    const CommonStore&,const PrivateShopTravelPaymentBoundary&,const TravelStepExpenseDraft&);
}
