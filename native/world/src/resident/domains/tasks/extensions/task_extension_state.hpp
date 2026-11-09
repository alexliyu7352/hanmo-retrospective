// 一次私人延期保存原委托人的真实同意；旧Task visitor与现金/实物账不追加字段。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
inline constexpr const char* task_extension_module="task_extensions";
inline constexpr const char* task_extension_fact_prefix="task_extension";
inline constexpr const char* task_extension_fact_kind="task_deadline_extended";
inline constexpr const char* task_extension_profile="private_actor_deadline_v1";
inline constexpr const char* task_extension_capability="task_extensions_v1";
// 作者只能声明原私人委托、原NPC的说话节点与固定增量，玩家不能提交自行选择的新截止。
struct TaskExtensionPolicy {
    StableId id,offer_id,dialogue_id,node_id;
    Integer additional_ticks=0;
};
struct TaskExtensionCatalog {Integer version=1;std::vector<TaskExtensionPolicy> policies;};
// 仅留本次实际会面的历史身份，不持久化临时报价epoch或读今天岗位反造旧同意。
struct TaskExtensionFact {
    StableId fact_id,policy_id,task_id,offer_id,actor_id,issuer_actor_id;
    StableId session_id,place_id,role_id,dialogue_id,node_id;
    Integer accepted_tick=0,granted_tick=0,original_deadline_tick=0,deadline_tick=0;
};
struct TaskExtensionState {
    Integer version=1;
    OrderedTable<TaskExtensionFact> facts;
    // 原Task只有一次延期，索引由事实顺序重建，不单列另一份有效截止表。
    CheckpointMap<StableId,StableId> by_task;
};
void rebuild_task_extension_index(TaskExtensionState&);
Outcome<TaskExtensionState> decode_task_extensions(const BoundaryValue&);
BoundaryValue encode_task_extensions(const TaskExtensionState&);
Status visit_task_extensions_checkpoint(CodecVisitor&,TaskExtensionState&);
}
