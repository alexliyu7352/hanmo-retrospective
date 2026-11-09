// 在已验证军事/舟战历史上提供人物占用来源；不读Family，也不重新验证整个World。
#pragma once
#include "resident/domains/military/military_refs.hpp"
#include "resident/model/dynamic_contract.hpp"
#include "resident/storage/common_store.hpp"
namespace hanmo::resident {
struct NavalState;
// 三集合为空才证明本域没有占用；未知旧历史必须失败，不能用当前位置或available补历史。
struct MilitaryActorOccupation {
 StableId actor_id,boundary_fact_id;Integer observed_tick=0;
 std::vector<StableId> custody_source_ids,battle_source_ids,siege_source_ids;
 // 当前激活必须预先可重放；旧无序关闭/围城进退会列出不足来源，避免提交后冷读才拒。
 bool history_complete=true;std::vector<StableId> incomplete_source_ids;
};
// 当前入口只观察稳定Common.tick；普通个人Action不是军事占用。
Outcome<MilitaryActorOccupation> query_military_actor_occupation(
 const CommonStore&,const MilitaryState&,const NavalState*,const StableId& actor_id);
// before_fact_id必须非空且真实存在。调用域先核其payload的tick；本域核来源tick及实际Common顺序。
// 旧陆战手动关闭、围城进退没有永久边界；相关历史无法确定时明确返回HistoryIncomplete。
Outcome<MilitaryActorOccupation> query_military_actor_occupation_before(
 const CommonStore&,const MilitaryState&,const NavalState*,const StableId& actor_id,
 Integer tick,const StableId& before_fact_id);
// 个人康复原Action阶段的精确credit头；严格from→to候选可重核0..to旧credit，稳定口仍拒未来。
Outcome<MilitaryActorOccupation> query_military_actor_occupation_before(
 const CommonStore&,const MilitaryState&,const NavalState*,const StableId& actor_id,
 Integer tick,const StableId& before_fact_id,const DynamicStepInput&);
// 只在真实单步候选使用；当前集合含本次已发生的to军史，observed_tick固定to。
// stable签名不变；消费者必须显式核同一phase，不能仍把它当Common.from的状态。
Outcome<MilitaryActorOccupation> query_military_actor_occupation(
 const CommonStore&,const MilitaryState&,const NavalState*,const StableId& actor_id,
 const DynamicStepInput&);

}
