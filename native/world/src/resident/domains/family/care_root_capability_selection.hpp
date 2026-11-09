// Care 的情景能力选择只归正式 PreparedCatalog，不是第二持久 selector。
// 原完整身份与情景在最终 prepare 时锁定；本值不安装教育来源或运行 provider。
#pragma once
#include "hanmo/resident/model_contract.hpp"
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include <optional>
#include <utility>

namespace hanmo::resident {
// 只准唯一正式 prepare helper 形成；缺 owner 不能默认成“教育未选”。
class CareRootCapabilitySelection {
public:
  CareRootCapabilitySelection(const CareRootCapabilitySelection &) = delete;
  CareRootCapabilitySelection &
  operator=(const CareRootCapabilitySelection &) = delete;
  CareRootCapabilitySelection(CareRootCapabilitySelection &&) noexcept =
      default;
  CareRootCapabilitySelection &
  operator=(CareRootCapabilitySelection &&) = delete;
  // 原完整身份的只读借用，不输出可重新选择版本的 mutable 对象。
  const CatalogIdentity &catalog_identity() const noexcept {
    return identity_;
  }
  // 此情景键必须与实际 World/Common metadata 或原 prepare 输入相同。
  const StableId &scenario_id() const noexcept {
    return scenario_id_;
  }
  // 只返回原私有 enum 的观察结果；业务入口不接受外传 bool。
  bool family_education_selected() const noexcept {
    return education_ == Education::family_education_v1;
  }

private:
  enum class Education { unselected, family_education_v1 };
  friend Status prepare_native_care_root_capability_selection(
      const BoundaryValue &, const StableId &, PreparedCatalog &);
  // 所有 ordinary 字段的实际预留先于深拷贝，持有顺序保证先销毁字段再退账。
  CareRootCapabilitySelection(CheckpointCopyLease memory,
                              CatalogIdentity identity, StableId scenario,
                              Education education)
      : ordinary_memory_(std::move(memory)), identity_(std::move(identity)),
        scenario_id_(std::move(scenario)), education_(education) {
  }
  CheckpointCopyLease ordinary_memory_;
  CatalogIdentity identity_;
  StableId scenario_id_;
  Education education_;
};

// 短寿命观察值而非可安装 token；所有共同门都自行从同 PreparedCatalog 读取。
struct CareRootSelectedCapabilities {
  std::optional<Integer> physical_version;
  bool family_education = false;
};

// 唯一正式 prepare 接头：从原 data.scenarios[scenario_id] 读精确配置。
// 调用须在最终 module/identity 形成之后；不创建 module marker、不开放安装。
Status prepare_native_care_root_capability_selection(
    const BoundaryValue &original_data, const StableId &scenario_id,
    PreparedCatalog &catalog);
// 核完整八项 identity 与实际情景；有 Care 却无 owner、错配或未知版本均拒。
Outcome<CareRootSelectedCapabilities>
read_native_care_root_capability_selection(const PreparedCatalog &,
                                           const StableId &actual_scenario_id);
} // namespace hanmo::resident
