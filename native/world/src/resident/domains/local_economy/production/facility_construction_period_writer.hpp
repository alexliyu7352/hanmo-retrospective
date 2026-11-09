// 原施工实际期间与单阶段 producer；只在同一 World 私有候选中采用。
#pragma once
#include "facility_construction_action_time.hpp"
#include "production_asset_construction_authority.hpp"
#include "resident/kernel/fixed_service_runtime.hpp"

namespace hanmo::resident {
struct FacilityConstructionPeriodWriteContext {
  CommonStore &common;
  FacilityConstructionState &state;
  CheckpointMemoryCharge &owned_fields;
  ActionElapsedStore &elapsed;
  const FacilityConstructionServiceContext &service;
  const ProductionAssetLifecycleReadContext &assets;
  const ActionTimeOriginalSourceRead &action_sources;
};
struct PendingFacilityConstructionResult {
  std::size_t row_index = 0;
  bool replay = false;
};
// ordinal 只定位同一原已供工期；到真实边界才写，不能补走过去或预测未来。
// 本人原初始用途必须覆盖实际服务窗，旧/新产权不能靠当前 Owner 回填。
Outcome<PendingFacilityConstructionResult>
record_facility_construction_period_in_candidate(
    FacilityConstructionPeriodWriteContext &, const StableId &construction,
    Integer actual_period_ordinal);
// H 个真实期齐且原 S0 满工后才写单阶段并终止原 Action；不激活容量。
// 物理占位、产权变化处置与专业应用仍由完整原业务候选继续核；任何失败
// 必须丢整个 World，不能独立发布本叶或保留已经结束的 Action。
Outcome<PendingFacilityConstructionResult>
complete_facility_construction_stage_in_candidate(
    FacilityConstructionPeriodWriteContext &, const StableId &construction);
} // namespace hanmo::resident
