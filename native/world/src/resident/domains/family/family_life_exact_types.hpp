// LIFE4：原FamilyDeathState的精确payload，不增第二死亡表或第二生命布尔。
#pragma once
#include "hanmo/resident/world_moment.hpp"
#include <variant>
namespace hanmo::resident {
struct MilitaryDeathCause4 {StableId casualty_fact_id,battle_id,actor_id;WorldMoment occurrence_at;};
struct NaturalAgeDeathCause4 {StableId age_check_fact_id;};
struct EnvironmentalSeaDeathCause4 {StableId sea_loss_result_id,actor_id,vessel_passenger_binding_ref;WorldMoment occurrence_at;};
using LifeCauseRef4=std::variant<MilitaryDeathCause4,NaturalAgeDeathCause4,EnvironmentalSeaDeathCause4>;
struct FamilyDeathPayload4 {StableId death_id;WorldMoment effective_at,recorded_at;LifeCauseRef4 cause;};
// 由已核Catalog/profile显式选择，不能由record字段存在推断物理版。
enum class FamilyLifeWireProfile {legacy_military_v1,complete_v4};
bool same_life_cause4(const LifeCauseRef4&,const LifeCauseRef4&);
}
