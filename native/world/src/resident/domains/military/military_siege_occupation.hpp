// 围城占用只由完整Military17参与链派生；旧无创建/加入/退出头的围城仍返回未覆盖。
#pragma once
#include "military_siege_lifecycle.hpp"
namespace hanmo::resident {
struct MilitarySiegeOccupationProjection {
 StableId siege_id,before_fact_id;Integer observed_tick=0;
 // 每个仍被占用的原Army对应真实created或joined头；确认撤退等待沿原结果页语义保留。
 std::vector<std::pair<StableId,StableId>> army_source_ids;
};
// 调用者先验证完整军事生命周期，再按原人物成员历史判断actor是否属于这些军团。
Outcome<std::optional<MilitarySiegeOccupationProjection>> read_military_siege_occupation(
 const CommonState&,const MilitarySiegeLifecycleState&,const SiegeRecord&,Integer tick,
 const StableId& actual_before_fact_id={});
// 与原个人康复候选同一严格单步，可重读真实旧credit边界；空边界只表示该候选当前to。
// 不接任意future上限，也不能以空边界把已含to变更的候选伪装为from。
Outcome<std::optional<MilitarySiegeOccupationProjection>> read_military_siege_occupation(
 const CommonState&,const MilitarySiegeLifecycleState&,const SiegeRecord&,Integer tick,
 const StableId& actual_before_fact_id,const DynamicStepInput&);
}
