// 原成人家庭消息预算与公开尾字段；不授予钱包、运送或获知许可。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct FamilyNoFeeCommunication {};
struct FamilyOwnWalletCommunicationCap {
  Integer max_money = 0;
};
using FamilyCommunicationBudget =
    std::variant<FamilyNoFeeCommunication, FamilyOwnWalletCommunicationCap>;
enum class FamilyCommunicationAudience {
  direct_only,
  accepted_carrier_delivery
};

// §2.2所有成人命令必需的原尾字段。public decoder必须显式读取，不能默认补同意。
// 预算只是本人后续发送上限，不预付、不授钱包权；当前真实渠道仅DirectOnly。
struct FamilyPersonalCommunicationOptions2 {
  FamilyCommunicationBudget budget;
  FamilyCommunicationAudience audience =
      FamilyCommunicationAudience::direct_only;
};
} // namespace hanmo::resident
