// CharacterOrigin/2 的唯一聚合人口来源引用；只由原S5已扣一人的结果证明。
#pragma once
#include "resident/model/common_model.hpp"
namespace hanmo::resident {
struct PopulationOriginRef {StableId cohort_id,split_receipt_id;Integer count=1;};
struct PopulationCharacterAllocationRead {StableId actor_id,registration_result_id,allocation_source_id;PopulationOriginRef origin_ref;};
// 反验同candidate已经实际注册的人物，不提供造人或扣人口的泛setter。
class PopulationCharacterOriginRead {
public:virtual ~PopulationCharacterOriginRead()=default;
    virtual Outcome<PopulationCharacterAllocationRead> read_population_character_origin(const StableId& actor_id,const PopulationOriginRef&)const=0;
};
}
