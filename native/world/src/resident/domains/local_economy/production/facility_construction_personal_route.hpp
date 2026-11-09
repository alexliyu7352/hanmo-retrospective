// 原 PlanFacilityCapacity 的只读预览与本人完整开工分派；不独立发布成本。
#pragma once
#include "facility_construction_commands.hpp"
#include "facility_construction_start.hpp"
namespace hanmo::resident {
struct FacilityConstructionCommandReadContext {
  const FacilityConstructionServiceContext &service;
  const ProductionAssetLifecycleReadContext &assets;
  const ActivityRead &activity;
};
// 首个已审分支只接受产权人本人一名worker，明拒后购/代理/多人而非假成功。
// actor 必须来自 S0 真实 envelope/静态准入；本口不更改控制或给任意 actor 授权。
Status
preview_plan_facility_capacity(const FacilityConstructionCommandReadContext &,
                               const StableId &actual_actor,
                               const PlanFacilityCapacity &);
// 同一候选再次实核，才沿原 consent→Consume/钱腿→Action/Fixed 实际采用。
// pending 只供原成功命令晚绑；失败必须弃整候选，不发布已扣成本或半个开工。
Outcome<PendingFacilityConstructionStart>
apply_plan_facility_capacity_in_candidate(
    FacilityConstructionStartWriteContext &, const StableId &actual_actor,
    const PlanFacilityCapacity &);
} // namespace hanmo::resident
