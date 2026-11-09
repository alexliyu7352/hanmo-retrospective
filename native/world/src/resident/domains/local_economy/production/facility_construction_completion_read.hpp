// 原实际完工的无环只读出口；不消费应用、费用后继或持久保存借值。
#pragma once
#include "facility_construction_result_read.hpp"

namespace hanmo::resident {
struct FacilityConstructionCompletedRead {
  const FacilityConstruction *construction = nullptr;
  const ConstructionStageResult *stage = nullptr;
  const ConstructionTerms *terms = nullptr;
  const ConstructionStartCost *cost = nullptr;
};
// 先沿唯一阶段/期间/实际成本正文，再核同原结果的 S0 真终止、工量及释放。
// 不授予设施应用或新的建设/支出许可；返回值只活在同 job 原根借用期内。
Outcome<FacilityConstructionCompletedRead>
read_completed_facility_construction(const FacilityConstructionServiceContext &,
                                     const MomentOwnerSource &actual_stage);
struct ProductionAssetLifecycleReadContext;
// 完整专业完成来源另核原本人同意/成功命令与每个真实期间的原用途覆盖。
// S3 成果消费者用本口；不回读后继设施应用或 Work 费用终局，避免应用环。
Outcome<FacilityConstructionCompletedRead>
read_authorized_completed_facility_construction(
    const FacilityConstructionServiceContext &,
    const ProductionAssetLifecycleReadContext &,
    const MomentOwnerSource &actual_stage);
} // namespace hanmo::resident
