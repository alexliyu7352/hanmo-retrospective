// S2只借S4既有伤因/疗程纯读，转换具名证明；不拥有或写入人物康复账。
#pragma once
#include "military_recovery_actor_contract.hpp"
namespace hanmo::resident {
class GrowthRecoveredActorRead;
// 借用期必须覆盖本次完整候选调用；上层工厂持有真正World读及严格phase生命周期。
class MilitaryGrowthRecoveryRead final:public MilitaryActorRecoveryRead {
public:
 explicit MilitaryGrowthRecoveryRead(const GrowthRecoveredActorRead& source):source_(source){}
 Outcome<std::optional<MilitaryActorRecoveryProof>> recovered_actor(
  const StableId&,const StableId&,Integer,const StableId&,const DynamicStepInput* phase=nullptr)const override;
private:
 const GrowthRecoveredActorRead& source_;
};
}
