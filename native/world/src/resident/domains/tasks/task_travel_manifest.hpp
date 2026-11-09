// 本人实际携行摘要只引用原任务和真实货批/同行者；地图锚点仍由唯一行旅位置提供。
#pragma once
#include "task_model.hpp"
#include "hanmo/resident/presentation_contract.hpp"
namespace hanmo::resident {
struct WorldObservationIndex;
struct TaskTravelCargo { StableId task_id,display_name,lot_id,item_id,item_name,cargo_kind,task_status;Integer quantity=0,observed_tick=0;bool reserved=false; };
struct TaskTravelEscort { StableId task_id,display_name,actor_id,actor_name,task_status,objective_state;Integer observed_tick=0; };
using TaskTravelRow=std::variant<TaskTravelCargo,TaskTravelEscort>;
struct TaskTravelManifestCursor { WorldIdentity identity;StableId observer_actor_id;Integer offset=0; };
struct TaskTravelManifestPage { WorldIdentity identity;StableId observer_actor_id;Integer cursor=0,scanned=0;std::vector<TaskTravelRow> rows;std::optional<TaskTravelManifestCursor> next_cursor; };
inline constexpr Integer task_travel_page_limit=64,task_travel_scan_limit=256;
// 托管物只看本人实际行囊，护送只用本人任务索引与真party；不枚举全局任务/运输表。
Outcome<TaskTravelManifestPage> query_task_travel_manifest(const CommonStore&,const TaskState&,const TaskCatalog&,
    const WorldObservationIndex&,const PresentationRead&,const StableId& observer,
    const std::optional<TaskTravelManifestCursor>& cursor={});
BoundaryValue encode_task_travel_manifest(const TaskTravelManifestPage&);
Outcome<TaskTravelManifestCursor> decode_task_travel_manifest_cursor(const BoundaryValue&);
}
