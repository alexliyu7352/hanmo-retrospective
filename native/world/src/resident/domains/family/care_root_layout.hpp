// Care 首次完整拥有根的唯一工程排序与选型形状门。
// 只借实际 typed owner，不保存第二 selector，也不代替任一来源或运行安装校验。
#pragma once
#include "resident/model/common_model.hpp"
#include <array>
#include <optional>
#include <string_view>

namespace hanmo::resident {
// 七张基础表是首次 C++ 工程根名；后续追加严格沿原 Care2、3、4、5。
// JSON 的实际键不包含顺序元数据；binary 在全部表之后保存同一 field_order。
inline constexpr std::array<std::string_view, 19> care_root_table_names{
    "requirements",
    "requirement_transitions",
    "accepted_caregivers",
    "care_bindings",
    "coverage_satisfied",
    "coverage_lost",
    "care_periods",
    "education_plans",
    "maturity_results",
    "care_handover_executions",
    "care_temporal_events",
    "care_period_sources",
    "education_choices",
    "education_activations",
    "facility_use_bindings",
    "facility_occupancy_events",
    "facility_disposition_results",
    "facility_clearance_executions",
    "facility_transport_acceptances"};

// 这里只选择已实现的精确时刻值布局；历史整数版与未准入 Care6 不隐式升级。
inline Outcome<std::size_t> care_root_table_count(Integer selected_version) {
  switch (selected_version) {
  case 3:
    return {true, 12, {}};
  case 4:
    return {true, 14, {}};
  case 5:
    return {true, 19, {}};
  default:
    return {false,
            {},
            common_failure("CareRootExactVersion345Required", {},
                           ErrorKind::unsupported_capability)
                .error};
  }
}

// 新建值可尚未指定顺序，编码使用上方 canonical 序；已有顺序必须本版恰好一次。
// 顺序可保留原输入排列，但不可多键、漏键、重复键或夹入未来版本尾。
inline Status validate_care_root_field_order(const FieldOrder &order,
                                             Integer selected_version) {
  const auto count = care_root_table_count(selected_version);
  if (!count.ok)
    return {false, {}, count.error};
  if (order.empty())
    return common_success();
  if (order.size() != count.value)
    return common_failure("CareRootFieldOrderMismatch");
  for (std::size_t i = 0; i < order.size(); ++i) {
    bool known = false;
    for (std::size_t key = 0; key < count.value; ++key)
      if (order[i] == care_root_table_names[key])
        known = true;
    if (!known)
      return common_failure("CareRootFieldOrderMismatch", order[i]);
    for (std::size_t prior = 0; prior < i; ++prior)
      if (order[prior] == order[i])
        return common_failure("CareRootDuplicateField", order[i]);
  }
  return common_success();
}

// 直接检查完整 owner 的十九张表；不接受 caller 自报 counts 或另一根影子状态。
// encode、copy/clone/rebind 和安装校验在选择任何表之前共用此门，禁止跳表丢值。
template <class CareOwner>
Status
validate_care_root_selection_shape(const CareOwner &value,
                                   std::optional<Integer> selected_version) {
  const bool facility_nonempty = !value.facility_use_bindings.empty() ||
                                 !value.facility_occupancy_events.empty() ||
                                 !value.facility_disposition_results.empty() ||
                                 !value.facility_clearance_executions.empty() ||
                                 !value.facility_transport_acceptances.empty();
  const bool autonomy_nonempty =
      !value.education_choices.empty() || !value.education_activations.empty();
  if (!selected_version) {
    if (facility_nonempty || autonomy_nonempty || !value.requirements.empty() ||
        !value.requirement_transitions.empty() ||
        !value.accepted_caregivers.empty() || !value.care_bindings.empty() ||
        !value.coverage_satisfied.empty() || !value.coverage_lost.empty() ||
        !value.care_periods.empty() || !value.education_plans.empty() ||
        !value.maturity_results.empty() ||
        !value.care_handover_executions.empty() ||
        !value.care_temporal_events.empty() ||
        !value.care_period_sources.empty() || !value.field_order.empty())
      return common_failure("CareRootSelectionRequired");
    return common_success();
  }
  const auto count = care_root_table_count(*selected_version);
  if (!count.ok)
    return {false, {}, count.error};
  if ((*selected_version == 3 && autonomy_nonempty) ||
      (*selected_version != 5 && facility_nonempty))
    return common_failure("CareRootFutureTailNotSelected");
  if (*selected_version != 5)
    for (const auto &row : value.care_handover_executions.rows())
      if (!row.second.work_period_sources.empty() ||
          !row.second.handover_execution_boundaries.empty())
        return common_failure("CareHandoverFutureTailNotSelected", row.first);
  return validate_care_root_field_order(value.field_order, *selected_version);
}

// 唯一遍历顺序供真正根 codec/owned traversal 使用；每回调拿到实际强型表。
// 这里只枚举已选表，前置共同检查保证未选内存没有被静默遗失。
template <class CareOwner, class Visitor>
Status visit_care_root_tables(CareOwner &value,
                              std::optional<Integer> selected_version,
                              Visitor &&visitor) {
  auto status = validate_care_root_selection_shape(value, selected_version);
  if (!status.ok || !selected_version)
    return status;
  const auto visit = [&](std::size_t index, auto &table) {
    if (status.ok)
      status = visitor(care_root_table_names[index], table);
  };
  visit(0, value.requirements);
  visit(1, value.requirement_transitions);
  visit(2, value.accepted_caregivers);
  visit(3, value.care_bindings);
  visit(4, value.coverage_satisfied);
  visit(5, value.coverage_lost);
  visit(6, value.care_periods);
  visit(7, value.education_plans);
  visit(8, value.maturity_results);
  visit(9, value.care_handover_executions);
  visit(10, value.care_temporal_events);
  visit(11, value.care_period_sources);
  if (*selected_version == 4 || *selected_version == 5) {
    visit(12, value.education_choices);
    visit(13, value.education_activations);
  }
  if (*selected_version == 5) {
    visit(14, value.facility_use_bindings);
    visit(15, value.facility_occupancy_events);
    visit(16, value.facility_disposition_results);
    visit(17, value.facility_clearance_executions);
    visit(18, value.facility_transport_acceptances);
  }
  return status;
}
} // namespace hanmo::resident
