// 驻地休整沿原部队疲劳真值；实际合格供给半日减10，不修复伤势或补发士气。
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct MilitaryGarrisonRestPolicy {
 StableId profile="supplied_garrison_rest_v1";
 std::vector<StableId> garrison_ids;
 Integer fatigue_reduction=10;
};
// 每项只对应同一单位的一个已提交供给半日；起始条件须由原训练/行军/休整全链重建。
struct MilitaryGarrisonRestFact {
 StableId id,unit_id,garrison_id,organization_id,ownership_id,supply_fact_id;
 Integer tick=0,training=0,fatigue_before=0,fatigue_after=0;
 FieldOrder field_order;
};
struct MilitaryGarrisonRestState {std::vector<MilitaryGarrisonRestFact> facts;FieldOrder field_order;};
}
