// 只保新显式围城的参与/节点演变来源，旧SiegeRecord不被当成过去加入事件。
#pragma once
#include "siege_model.hpp"
#include "resident/storage/common_store.hpp"
#include "resident/model/dynamic_contract.hpp"
namespace hanmo::resident {
enum class MilitarySiegeLifecycleKind { created,joined,advanced,battle_finished,strategic,terminated,closed,integrated,departed };
struct MilitarySiegeParticipationSnapshot {
 StableId phase;
 Integer started_tick=0,last_tick=0,round=0;
 std::vector<std::pair<StableId,Integer>> armies;
 std::vector<std::pair<StableId,StableId>> unit_nodes;
 std::vector<StableId> withdrawn_army_ids;
 FieldOrder field_order;
};
// cause_id须由对应领域绑定原到达/会战/准备/命令来源，结构门不冒授权门。
struct MilitarySiegeLifecycleFact {
 StableId id,siege_id,topology_id,actor_id,cause_id,previous_fact_id;
 MilitarySiegeLifecycleKind kind=MilitarySiegeLifecycleKind::created;
 Integer tick=0;
 std::optional<MilitarySiegeParticipationSnapshot> before;
 MilitarySiegeParticipationSnapshot after;
 std::optional<SiegeStrategicCommand> strategic_command;
 std::optional<StableId> termination_reason;
 FieldOrder field_order;
};
struct MilitarySiegeLifecycleState {std::vector<MilitarySiegeLifecycleFact> facts;FieldOrder field_order;};
// 只复制原已经完成的围城状态，不推断今天缺席者的历史退出。
MilitarySiegeParticipationSnapshot capture_military_siege_participation(const SiegeRecord&);
StableId military_siege_lifecycle_kind_name(MilitarySiegeLifecycleKind);
Status validate_military_siege_lifecycle_shape(const MilitarySiegeLifecycleFact&,const SiegeTopology&);
// 限完整已验本域来源后使用；按真实Common顺序，不能按当前siege.armies或数字后缀代替。
Outcome<MilitarySiegeParticipationSnapshot> military_siege_participation_before(
 const CommonStore&,const MilitarySiegeLifecycleState&,const StableId& siege_id,const StableId& actual_before_fact_id);
// 只有原未提交单半日候选允许真实to头；旧稳定口不观察未来。
Outcome<MilitarySiegeParticipationSnapshot> military_siege_participation_before(
 const CommonStore&,const MilitarySiegeLifecycleState&,const StableId& siege_id,const StableId& actual_before_fact_id,const DynamicStepInput&);
}
