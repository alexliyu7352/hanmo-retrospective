// 原应用所消费的实际完成作业；本口不授容量、使用权或 holder 处置许可。
#pragma once
#include "facility_activation_model.hpp"
#include "facility_construction_completion_read.hpp"
namespace hanmo::resident {
using FacilityActivationCompletedWork = FacilityConstructionCompletedRead;
// 首个真实本人有界工程只接 EconomicWork.CompletedStage。S3 必须另借原
// Work application cut，不能将 fee-only reader 或未完成期间当成成果。
// 返回全部借自同 job 原根，不保存新的 application key、工量或费用行。
Outcome<FacilityActivationCompletedWork>
read_facility_activation_completed_work(
    const FacilityConstructionServiceContext &, const WorkApplicationSource &);
// 八字段 receipt 的工/料/目标部分必须逐实际源一一绑定；发生时刻、当前
// 原权限、实际空间与 holder 处置仍由真正应用 writer / full reader 另核。
Status validate_facility_activation_completed_work_binding(
    const FacilityConstructionServiceContext &,
    const FacilityActivationReceipt &);
} // namespace hanmo::resident
