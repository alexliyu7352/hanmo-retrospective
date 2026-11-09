// 路线页只借本人已受领条款的公开场所和原截止；到城不等于交付或回报完成。
#pragma once
#include "task_model.hpp"
namespace hanmo::resident {
struct WorldObservationIndex;
struct GovernanceTaskState;struct EnvoyTaskState;class DiplomaticMissionRead;class DiplomacyPresentationRead;class DiplomaticEnvoyReportRead;
// target_kind是所选城里的办事目标；deadline_scope明确原截止约束最终交付还是回报。
struct TaskRouteObligation {
    StableId task_id,display_name,template_id,task_status,place_id,place_name,target_kind,deadline_scope;
    Integer deadline_tick=0,minimum_handover_ticks=0;
    bool deadline_inclusive=true,requires_prior_objective=false;
};
// 续页必须保留同一世界、本人和目的城，不能把一个目的地的扫描位置套到另一地。
struct TaskRouteObligationsCursor { WorldIdentity identity;StableId observer_actor_id,destination_settlement_id;Integer offset=0; };
struct TaskRouteObligationsPage {
    WorldIdentity identity;StableId observer_actor_id,destination_settlement_id;
    Integer cursor=0,scanned=0;std::vector<TaskRouteObligation> rows;
    std::optional<TaskRouteObligationsCursor> next_cursor;
};
inline constexpr Integer task_route_page_limit=64,task_route_scan_limit=256;
// 消费提交尾构建的本人索引。治理只在显式登记的真实sidecar通过后跳过；不扫描未知委托或NPC位置。
Outcome<TaskRouteObligationsPage> query_task_route_obligations(const CommonStore&,const TaskState&,const TaskCatalog&,
    const CommonCatalog&,const WorldObservationIndex&,const StableId& observer,const StableId& destination_settlement,
    const std::optional<TaskRouteObligationsCursor>& cursor={},const GovernanceTaskState* registered_governance=nullptr);
// 新模块只消费本人真实使者受领；原回报未配置截止时不生成推算行。
Outcome<TaskRouteObligationsPage> query_task_route_obligations_with_envoy(const CommonStore&,const TaskState&,const TaskCatalog&,const CommonCatalog&,const WorldObservationIndex&,const StableId& observer,const StableId& destination_settlement,const std::optional<TaskRouteObligationsCursor>& cursor,const GovernanceTaskState* registered_governance,const EnvoyTaskState&,const DiplomaticMissionRead&,const DiplomacyPresentationRead&,const DiplomaticEnvoyReportRead* reports=nullptr);
BoundaryValue encode_task_route_obligations(const TaskRouteObligationsPage&);
Outcome<TaskRouteObligationsCursor> decode_task_route_obligations_cursor(const BoundaryValue&);
}
