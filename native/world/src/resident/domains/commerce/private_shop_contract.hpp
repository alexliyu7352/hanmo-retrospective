// 首间商铺保私人产权和原合同；客户/技能/司法许可各由其权威域给真实来源，不能凭余额冒资格。
#pragma once
#include "resident/checkpoint/checkpoint_copy_shape.hpp"
#include "resident/model/common_model.hpp"
#include <optional>
#include <utility>
namespace hanmo::resident {
inline constexpr const char *private_shop_module = "private_shops";
inline constexpr const char *private_shop_open_prefix = "private_shop";
inline constexpr const char *private_shop_open_kind = "private_shop_opened";
// 作者明确列出可租铺位及办理本人，市场仅供原中间报价，不复制其库存或现金。
struct PrivateShopPremises {
  StableId id, place_id, lessor_actor_id, lessor_wallet_id, lessor_role_id;
  StableId agreement_dialogue_id, agreement_node_id, market_shop_id;
  Integer opening_fee = 900, initial_capital = 300, inventory_capacity = 200,
          daily_rent = 2;
  StableId rent_rate_profile;
};
struct PrivateShopOpen {
  StableId premises_id;
};
struct PrivateShopFund {
  StableId shop_id;
  Integer amount = 0;
};
struct PrivateShopSetMarkup {
  StableId shop_id;
  Integer markup_basis_points = 0;
};
struct PrivateShopOperate {
  StableId shop_id, item_id;
};
struct PrivateShopClose {
  StableId shop_id;
};
// 动态钱包/库存仍属原本人；独立容器不是第二资产Owner或可花账。
struct PrivateShopOpeningTerms {
  StableId premises_id, place_id, proprietor_actor_id, lessor_actor_id;
  StableId payer_wallet_id, lessor_wallet_id, market_shop_id;
  Integer opening_fee = 0, initial_capital = 0, inventory_capacity = 0,
          daily_rent = 0;
  StableId rent_rate_profile;
};
// 当面许可冻结原说话人和节点，cold不拿当前角色任职/地点回填过去签约。
struct PrivateShopMeetingSource {
  StableId actor_id, provider_actor_id, session_id, dialogue_id, node_id,
      place_id, role_id;
};
// 法律来源只由具名提供者验证；当前无司法口时必须显式不可用，不能默认无通缉。
struct PrivateShopPermissionRequest {
  StableId actor_id, premises_id, place_id;
};
struct PrivateShopPermissionSource {
  WorldIdentity source_identity;
  PrivateShopPermissionRequest request;
  StableId permit_fact_id, clearance_fact_id;
  Integer issued_tick = 0, observed_tick = 0, valid_through_tick = 0;
};
// 原许可值的同步拥有返回：租约先于实际五文本字段，复制前逐字段预留。
// 不是新的权限事实；只有原 full provider 能判许可，构造只解决所有权。
class OwnedPrivateShopPermissionSource {
  CheckpointMemoryCharge memory_;
  PrivateShopPermissionSource source_;
  friend OwnedPrivateShopPermissionSource own_private_shop_permission_source(
      const WorldIdentity &, const PrivateShopPermissionRequest &,
      const StableId &, const StableId &, Integer, Integer, Integer);

public:
  OwnedPrivateShopPermissionSource() = default;
  OwnedPrivateShopPermissionSource(const OwnedPrivateShopPermissionSource &) =
      delete;
  OwnedPrivateShopPermissionSource &
  operator=(const OwnedPrivateShopPermissionSource &) = delete;
  OwnedPrivateShopPermissionSource(
      OwnedPrivateShopPermissionSource &&) noexcept = default;
  OwnedPrivateShopPermissionSource &
  operator=(OwnedPrivateShopPermissionSource &&) = delete;
  const PrivateShopPermissionSource &source() const noexcept { return source_; }
};
OwnedPrivateShopPermissionSource own_private_shop_permission_source(
    const WorldIdentity &, const PrivateShopPermissionRequest &,
    const StableId &permit_fact_id, const StableId &clearance_fact_id,
    Integer issued_tick, Integer observed_tick, Integer valid_through_tick);

// caller 必须先用上方唯一 planner 预留目标租约，并使其覆盖整个返回值生命周期。
PrivateShopPermissionSource copy_private_shop_permission_source_preleased(
    const PrivateShopPermissionSource &);
// Eligibility / quote
// 的瞬态值：复制必须重新租五字段；不共享租约或复制第二许可事实。
class PrivateShopPermissionValue {
  std::optional<OwnedPrivateShopPermissionSource> source_{std::in_place};

public:
  PrivateShopPermissionValue() = default;
  explicit PrivateShopPermissionValue(
      OwnedPrivateShopPermissionSource &&source) noexcept
      : source_(std::in_place, std::move(source)) {}
  PrivateShopPermissionValue(const PrivateShopPermissionValue &);
  PrivateShopPermissionValue &operator=(const PrivateShopPermissionValue &);
  PrivateShopPermissionValue(PrivateShopPermissionValue &&) noexcept = default;
  PrivateShopPermissionValue &operator=(PrivateShopPermissionValue &&) noexcept;
  const PrivateShopPermissionSource &source() const noexcept {
    return source_->source();
  }
};
class PrivateShopOpeningProvenanceSink;
class PrivateShopPermissionRead {
public:
  virtual ~PrivateShopPermissionRead() = default;
  virtual Outcome<OwnedPrivateShopPermissionSource>
  private_shop_opening_permission_owned(
      const PrivateShopPermissionRequest &) const {
    return {false,
            {},
            common_failure("PrivateShopPermissionOwnedSourceRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  }
  virtual Outcome<OwnedPrivateShopPermissionSource>
  private_shop_opening_permission_before_fact_owned(
      const PrivateShopPermissionRequest &, const StableId &, Integer) const {
    return {false,
            {},
            common_failure("PrivateShopPermissionOwnedSourceRequired", {},
                           ErrorKind::unsupported_capability)
                .error};
  }
  // 保留只在原完整历史许可成功后借实际根；旧 provider 未接时显式拒绝。
  virtual Status visit_opening_permission_provenance(
      const PrivateShopPermissionRequest &, const StableId &, Integer,
      PrivateShopOpeningProvenanceSink &) const {
    return common_failure("PrivateShopPermissionProvenanceUnavailable", {},
                          ErrorKind::unsupported_capability);
  }
  virtual Outcome<PrivateShopPermissionSource> private_shop_opening_permission(
      const PrivateShopPermissionRequest &) const = 0;
  virtual Outcome<PrivateShopPermissionSource>
  private_shop_opening_permission_before_fact(
      const PrivateShopPermissionRequest &,
      const StableId &actual_opening_fact_id, Integer opening_tick) const = 0;
};
// 页与私人继产仅借明确来源；资格未通、欠款未清、成本不可证不能变为默认空责任。
struct PrivateShopSource {
  WorldIdentity source_identity;
  StableId observer_actor_id, shop_id, premises_id, proprietor_actor_id,
      place_id, status;
  StableId wallet_id, inventory_id, opening_fact_id, rent_obligation_id,
      employee_contract_id, last_operating_fact_id;
  Integer opened_tick = 0, markup_basis_points = 0, operating_days = 0,
          completed_orders = 0, net_income = 0;
  // accounts_proven 仅表示实际同铺位的当前观察，所有权不自动授远方知情。
  // 为假时只给原开设身份和时刻，边界层省略当前动态字段。
  bool accounts_proven = false, milestone_reached = false;
  StableId unavailable_code;
  // 已付但所属营业Action尚未结日的原费用；当前净收益已包含，不能再由前端扣减。
  Integer unclosed_rent_paid = 0, unclosed_wage_paid = 0;
};
class PrivateShopRead {
public:
  virtual ~PrivateShopRead() = default;
  virtual Outcome<std::vector<StableId>>
  private_shop_ids(const StableId &observer, Integer offset,
                   Integer limit) const = 0;
  virtual Outcome<std::optional<PrivateShopSource>>
  private_shop_source(const StableId &observer,
                      const StableId &shop_id) const = 0;
};
} // namespace hanmo::resident
