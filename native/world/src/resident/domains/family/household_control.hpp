// 家庭事务受托权的唯一来源在Family事实账；私粮、余额与周期仍归LocalEconomy。
#pragma once
#include "resident/storage/common_codec.hpp"
#include "resident/kernel/source_ref.hpp"
namespace hanmo::resident {
struct FamilyState;
struct LifeAgeSources;
struct FamilyCatalog;
struct LocalEconomyState;
inline constexpr const char *household_provision_operation =
    "configure_household_provisioning";
// 采购接受是独立本人受托用途；旧供养委托不得自动扩权。
inline constexpr const char *household_purchase_operation =
    "accept_household_purchase";
// 原公共市场劳动的两个本人同意用途；不从采购或供养控制推导。
inline constexpr const char *household_market_staffing_operation =
    "consent_public_market_staffing";
inline constexpr const char *household_market_staffing_release_operation =
    "consent_public_market_staffing_release";
// 只接作者明确给定的既有、本人已接受的初态委托。不是运行时自授权命令。
struct HouseholdControlDefinition {
  StableId id, actor_id, household_group_id, source_role_id;
  StableId operation = household_provision_operation;
  Integer effective_from_tick = 0;
  std::optional<Integer> through_tick;
  bool actor_accepted = false;
};
struct HouseholdControlCatalog {
  OrderedTable<HouseholdControlDefinition> initial_mandates;
  bool purchase_control_enabled = false;
  bool market_staffing_control_enabled = false;
};
// 此payload追加到FamilyFact；不再维护第二张当前控制者表。
struct HouseholdControlFact {
  StableId kind = "family_household_control", definition_id, actor_id,
           household_group_id, source_role_id;
  StableId operation = household_provision_operation, scenario_id;
  Integer recorded_tick = 0, effective_from_tick = 0;
  std::optional<Integer> through_tick;
  bool actor_accepted = false;
  FieldOrder field_order;
};
struct HouseholdProvisionControlProof {
  StableId actor_id, household_group_id, fact_id, source_role_id, operation;
  Integer source_revision = 0, effective_from_tick = 0;
  std::optional<Integer> through_tick;
};
class HouseholdProvisionControlRead {
public:
  virtual ~HouseholdProvisionControlRead() = default;
  virtual Outcome<std::optional<HouseholdProvisionControlProof>>
  current(const StableId &actor_id, const StableId &household_group_id,
          const StableId &operation, Integer at_tick) const = 0;
  // 只验已接受配置的永久来源，不能用它代替current给新配置授权。
  virtual Status validate_accepted(const HouseholdProvisionControlProof &,
                                   Integer accepted_tick) const = 0;
};
class FamilyHouseholdControlAccess final
    : public HouseholdProvisionControlRead {
public:
  FamilyHouseholdControlAccess(const CommonStore &, const FamilyState &,
                               const FamilyCatalog &, const CommonCatalog &,
                               const LocalEconomyState &,
                               const LifeAgeSources *initial_age = nullptr);
  Outcome<std::optional<HouseholdProvisionControlProof>>
  current(const StableId &, const StableId &, const StableId &,
          Integer) const override;
  Status validate_accepted(const HouseholdProvisionControlProof &,
                           Integer) const override;

private:
  const CommonStore &common_;
  const FamilyState &family_;
  const FamilyCatalog &catalog_;
  const CommonCatalog &shared_;
  const LocalEconomyState &economy_;
  const LifeAgeSources *initial_age_;
};
Outcome<HouseholdControlCatalog>
decode_household_control_catalog(const BoundaryValue &);
Status
validate_household_control_catalog(const CommonStore &, const CommonCatalog &,
                                   const LocalEconomyState &,
                                   const HouseholdControlCatalog &,
                                   const LifeAgeSources *initial_age = nullptr);
// 仅新局tick0登记真实Common头和同ID的Family事实；失败不留下半份来源。
Status
initialize_household_control(CommonStore &, FamilyState &,
                             const CommonCatalog &, const LocalEconomyState &,
                             const HouseholdControlCatalog &,
                             const LifeAgeSources *initial_age = nullptr);
Status validate_household_control(const CommonStore &, const FamilyState &,
                                  const CommonCatalog &,
                                  const LocalEconomyState &,
                                  const HouseholdControlCatalog *,
                                  const LifeAgeSources *initial_age = nullptr);
// 当前授权投影：known无权为空，未知/坏源/未启用报错；从不据血缘、现Owner或同址给权。
Outcome<std::optional<HouseholdProvisionControlProof>>
household_provision_control_for_actor(
    const CommonStore &, const FamilyState &, const FamilyCatalog &,
    const CommonCatalog &, const LocalEconomyState &, const StableId &actor_id,
    const StableId &household_group_id, const StableId &operation,
    Integer at_tick, const LifeAgeSources *initial_age = nullptr);
Status validate_household_provision_control_source(
    const CommonStore &, const FamilyState &, const FamilyCatalog &,
    const CommonCatalog &, const LocalEconomyState &,
    const HouseholdProvisionControlProof &, Integer accepted_tick,
    const LifeAgeSources *initial_age = nullptr);
HouseholdControlFact decode_household_control_fact(const BoundaryValue &);
BoundaryValue encode_household_control_fact(const HouseholdControlFact &);
Status visit_household_control_fact(CodecVisitor &, HouseholdControlFact &);
// 只借原已接受 Steward 事实；不授当前权、不从今日资产所有人倒填历史。
// 原 Aid 读取者另验真实容器产权及岗位历史，返回行只活于同根未变更期间。
Outcome<const HouseholdControlFact *>
read_original_household_steward_source(const CommonStore &, const FamilyState &,
                                       const FamilyCatalog &,
                                       const CommonCatalog &, const SourceRef &,
                                       const LifeAgeSources *initial_age);
} // namespace hanmo::resident
