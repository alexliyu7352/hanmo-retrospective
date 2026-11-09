// 显式匿名士卒缺粮逃散：军官与具名亲兵仍由其本人行动和任免来源处置。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitarySupplyAttritionPolicy {
 StableId profile="anonymous_shortage_v1";
 Integer shortage_threshold_half_days=2,desertion_basis_points=200,morale_loss=10;
};
// 每项记录只消费原供给前的具体匿名fit；不生成替代伤亡或搬动人物私产。
struct MilitarySupplyAttritionFact {
 StableId id,unit_id,supply_fact_id,previous_supply_fact_id;
 Integer tick=0,consecutive_shortage_half_days=0;
 std::vector<StableId> fit_before,eligible_fit_before,deserted_ids;
 FieldOrder field_order;
};
struct MilitarySupplyAttritionState {
 std::vector<MilitarySupplyAttritionFact> facts;
 FieldOrder field_order;
};
}
