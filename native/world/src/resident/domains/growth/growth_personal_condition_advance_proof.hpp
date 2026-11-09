// 仅原成功advance尾的瞬态授权合同；不依赖条件账、Growth状态或持久访客。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
class CommonStore;
struct ActorConditionCaptureIdentity;
// 中央原advance尾实现不可外造的短栈凭据；它保存真from/to、同Common/capture、末ID/头。
// validate不消费，完整域重放成功后consume一次；失败、后续分配或rollback均不得再用。
class GrowthConditionDuelAdvanceProof {
public:
 virtual ~GrowthConditionDuelAdvanceProof()=default;
 virtual Status validate_duel_advance(const CommonStore&,Integer actual_from,Integer actual_to,
     const std::shared_ptr<const ActorConditionCaptureIdentity>&)const=0;
 virtual Status consume_duel_advance(const CommonStore&,Integer actual_from,Integer actual_to,
     const std::shared_ptr<const ActorConditionCaptureIdentity>&)=0;
};
}
