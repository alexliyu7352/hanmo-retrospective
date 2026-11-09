// 共同观察页是带运行身份的有限只读值；行类型闭合，不能承载任意世界字段或修改请求。
#pragma once
#include "hanmo/resident/checkpoint_memory.hpp"
#include "resident/domains/director/director_model.hpp"
#include <map>
#include <variant>
namespace hanmo::resident {
// 人物摘要只来自当前同地观察；不包含远地位置、私人属性、钱包或自主计划。
struct ObservedActorSummary {
  StableId actor_id, place_id;
  Integer observed_tick = 0;
  bool available = false, occupied = false;
};
// 已知报告保留原观察和获知时刻，stale不触发读取真实库存补值。
struct ObservedSupplySummary {
  KnownSupply source;
  bool stale = false;
};
struct ObservedArrearSummary {
  KnownArrear source;
};
struct ObservedCommitmentSummary {
  KnownCommitment source;
};
using WorldObservationRow =
    std::variant<ObservedActorSummary, ObservedSupplySummary,
                 ObservedArrearSummary, ObservedCommitmentSummary>;
struct WorldObservationCursor {
  WorldIdentity identity;
  StableId observer_actor_id, place_id;
  Integer offset = 0;
};
struct WorldObservationPage {
  WorldIdentity identity;
  StableId observer_actor_id, place_id;
  Integer cursor = 0, scanned = 0;
  std::optional<WorldObservationCursor> next_cursor;
  std::vector<WorldObservationRow> rows;
};
// 同候选只读派生索引不进存档；只按resolved_location、已发布公开报告和任务所属者保存引用。
struct WorldObservationIndex {
  Integer revision = -1, tick = -1;
  CheckpointMap<StableId, std::vector<StableId>> actors_by_place,
      public_reports_by_site, tasks_by_actor;
};
WorldObservationIndex build_world_observation_index(const CommonStore &,
                                                    const SocialState &,
                                                    const TaskState &);
inline constexpr Integer world_observation_page_limit = 64,
                         world_observation_scan_limit = 256;
// 非本人或远地查询拒绝；续页必须使用同一world/epoch/revision/observer/place的原游标。
Outcome<WorldObservationPage> query_world_observation(
    const CommonStore &, const SocialState &, const TaskState &,
    const InteractionState &, const WorldObservationIndex &,
    const StableId &observer, const StableId &place,
    const std::optional<WorldObservationCursor> &cursor = {});
// 唯一边界编码器只输出上述typed页，前端不能取得CommonStore或WorldBundle。
BoundaryValue encode_world_observation(const WorldObservationPage &);
Outcome<WorldObservationCursor>
decode_world_observation_cursor(const BoundaryValue &);
} // namespace hanmo::resident
