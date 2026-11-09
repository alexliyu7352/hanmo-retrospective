// local-move/1 独立保存原局部移动的真实写入，不改 travel/4、travel/5 段。
#pragma once
#include "resident/model/common_model.hpp"
#include "resident/kernel/common_fact_prefix.hpp"
#include "resident/kernel/world_timeline.hpp"
namespace hanmo::resident {
// 前驱实际 Common cut 与空间源一起冻结；from/to 是本次原 writer 的真实前后值。
struct LocalMoveSource {
  StableId id, actor_id;
  WorldMoment occurred_at;
  CommonActualFactPrefix before_prefix;
  std::vector<MomentOwnerSource> before_head;
  SettlementLocation from, to;
};
} // namespace hanmo::resident
