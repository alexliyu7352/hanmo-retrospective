// E§12.5I/A07 的首次真实应用根候选；不并入已选 economic_facility_sources/1。
#pragma once
#include "facility_construction_model.hpp"
#include "resident/domains/tasks/work_execution/work_result_source.hpp"

namespace hanmo::resident {
// 逐字采用原 WorkResultSource；设施消费者只接受真实 construction
// SupervisedResult。
struct ProductionApplicationS3Work {
  WorkResultSource source;
};
struct ProductionApplicationCompletedPeriod {
  SourceRef receipt_ref;
};
struct ProductionApplicationCompletedStage {
  SourceRef stage_result_ref;
};
using ProductionApplicationEconomicResult =
    std::variant<ProductionApplicationCompletedPeriod,
                 ProductionApplicationCompletedStage>;
struct ProductionApplicationEconomicWork {
  WorkRef work_ref;
  ProductionApplicationEconomicResult result;
};
// 原闭合两分支；不得用一个裸 Fact 或政治批准冒真实完成作业。
using WorkApplicationSource = std::variant<ProductionApplicationS3Work,
                                           ProductionApplicationEconomicWork>;
// 原八字段、原次序。不另持 current_capacity 或另计 worker 数。
struct FacilityActivationReceipt {
  StableId receipt_id;
  WorkApplicationSource source_work_fact;
  StableId asset_ref;
  std::vector<SourceRef> material_receipt_refs;
  std::vector<SourceRef> worker_result_refs;
  Integer capacity_units = 0;
  StableId capacity_unit_id;
  Integer active_from_tick = 0;
};
// 09 要求的实际原应用时点与实际采用定义，属于同 asset owner 的真实应用行。
// applied_at 只由同候选 clock 捕获，definition 只取已核 ConstructionTerms，
// 不是自由命令参数，也不根据今天设施定义回填旧 receipt。
struct AppliedFacilityActivation {
  FacilityActivationReceipt receipt;
  WorldMoment applied_at;
  EconomicRuleRef adopted_definition_ref;
};
// 候选首次 production_work_applications/1，尚未登记/选型/接 World。
// 只保真实应用结果；统一 application key 从完成工程原源派生，不再存索引账。
struct ProductionWorkApplicationsState {
  Integer schema_version = 1;
  CheckpointVector<AppliedFacilityActivation> facility_activations;
};
} // namespace hanmo::resident
