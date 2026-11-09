// S1真实单位用力→S4唯一Condition写入；从不通过任意delta或伪Action驱动人物。
#pragma once
#include "resident/domains/actor_foundation/actor_foundation_refs.hpp"
#include "condition_capture.hpp"
namespace hanmo::resident {
class CommonStore;struct MovementExactState;struct CharacterConditionSources;class GrowthCapabilityRevisionWrite;
struct TravelConditionApplication {std::vector<ConditionReceiptRefV2> receipt_refs;std::vector<BoundActorConditionWrite> actual_writes;};
// 原S1已经把unit及其真正关闭/exertion源写在同候选；本叶保每个实际参与者一次效应。
Outcome<TravelConditionApplication> apply_travel_exertion_condition(CommonStore&,AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MovementExactState&,const StableId& exertion_id,const CharacterConditionSources* original_sources=nullptr,GrowthCapabilityRevisionWrite* capability_changes=nullptr);
// 冷读沿真实source核原单位费用、实际参与、精确时刻及原clamp规则，不重施效果。
Status validate_travel_exertion_condition_receipt(const CommonState&,const ConditionReceiptV2&,const ActorFoundationCatalog&,const MovementExactState&);
// 源与结果双向覆盖；删掉已用力的Condition结果不能靠回退head通过恢复。
Status validate_travel_exertion_condition_coverage(const CommonState&,const AuthoredActorFoundationState&,const ActorFoundationCatalog&,const MovementExactState&);
} // 外层唯一WorldBundle候选负责全部身体/来源/运动的共同回滚。
