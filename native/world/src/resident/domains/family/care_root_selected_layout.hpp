// 同一真实目录驱动 Care 根的共同形状门；不让 caller bool/版本代替能力选择。
// 这里只核拥有形状，完整来源、PIN、clone/rebind 仍由原真实根负责。
#pragma once
#include "care_root_capability_selection.hpp"
#include "care_root_layout.hpp"
#include <utility>

namespace hanmo::resident {
// encode/copy/clone/rebind/安装/冷验共用真实 catalog 门，不另持状态开关。
// v1c 的物理3教育条件与物理键集合分别核，不据4/5数字推断教育能力。
template <class CareOwner>
Status validate_care_root_capability_shape(const CareOwner &value,
                                           const PreparedCatalog &catalog,
                                           const StableId &actual_scenario_id) {
  auto selected =
      read_native_care_root_capability_selection(catalog, actual_scenario_id);
  if (!selected.ok)
    return {false, {}, selected.error};
  auto valid = validate_care_root_selection_shape(
      value, selected.value.physical_version);
  if (!valid.ok)
    return valid;
  if (selected.value.physical_version == 3 &&
      !selected.value.family_education &&
      (!value.education_plans.empty() || !value.maturity_results.empty()))
    return common_failure("Care3EducationSelectionRequired");
  return common_success();
}

// 每次从同目录重新取得只读选择，先共同核验再访问实际表，禁止跳尾丢值。
// 不暴露接受 caller 自填能力快照的另一门；同一 immutable 目录重读不改选择。
template <class CareOwner, class Visitor>
Status visit_selected_care_root_tables(CareOwner &value,
                                       const PreparedCatalog &catalog,
                                       const StableId &actual_scenario_id,
                                       Visitor &&visitor) {
  auto valid =
      validate_care_root_capability_shape(value, catalog, actual_scenario_id);
  if (!valid.ok)
    return valid;
  auto selected =
      read_native_care_root_capability_selection(catalog, actual_scenario_id);
  if (!selected.ok)
    return {false, {}, selected.error};
  return visit_care_root_tables(value, selected.value.physical_version,
                                std::forward<Visitor>(visitor));
}
} // namespace hanmo::resident
