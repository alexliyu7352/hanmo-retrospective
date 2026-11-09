// 原S4返回empty/error原样保留；映射不据today injury/available增补历史或放宽来源。
#include "military_recovery_growth.hpp"
#include "resident/domains/growth/growth_recovery_model.hpp"
namespace hanmo::resident {
Outcome<std::optional<MilitaryActorRecoveryProof>> MilitaryGrowthRecoveryRead::recovered_actor(const StableId& actor,const StableId& casualty,Integer tick,const StableId& before,const DynamicStepInput* phase)const{
 auto source=source_.recovered_actor(actor,casualty,tick,before,phase);if(!source.ok)return {false,{},source.error};if(!source.value)return {true,std::nullopt,{}};
 const auto& original=*source.value;MilitaryActorRecoveryProof proof;proof.source_identity=original.source_identity;proof.actor_id=original.actor_id;proof.casualty_fact_id=original.original_casualty_id;proof.recovery_source_id=original.last_recovery_fact_id;proof.before_fact_id=original.before_fact_id;proof.observed_tick=original.observed_tick;proof.recovered_tick=original.recovered_tick;
 return {true,std::move(proof),{}};
}
}
