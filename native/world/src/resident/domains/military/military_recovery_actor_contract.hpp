// 具名伤兵回队须借原人物康复账；匿名兵token不拥有Common人物伤势。
#pragma once
#include "military_recovery_model.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
struct MilitaryActorRecoveryProof {
 WorldIdentity source_identity;
 StableId actor_id,casualty_fact_id,recovery_source_id,before_fact_id;
 Integer observed_tick=0,recovered_tick=0;
};
// 只证明原wound之后真实康复至0级；无康复/仍伤返回empty，缺史返回error。
// before为空仅当前稳定或严格单步，非空须真实military_recovery:/military_wounded_recovery/military头。
class MilitaryActorRecoveryRead {
public:
 virtual ~MilitaryActorRecoveryRead()=default;
 virtual Outcome<std::optional<MilitaryActorRecoveryProof>> recovered_actor(
  const StableId& actor_id,const StableId& casualty_fact_id,Integer observed_tick,
  const StableId& before_fact_id,const DynamicStepInput* phase=nullptr) const=0;
};
}
