// 先验证实际托管原件，再逐真实依赖发出保留边；不建立另一份产权或同意账。
#pragma once
#include "private_shop_household_custody.hpp"
#include <string_view>
namespace hanmo::resident {
// 由中央实现真正跨域保留；具型初始化与目录引用不得降成伪 Common 头。
class PrivateShopHouseholdCustodyPinSink {
public:
  virtual ~PrivateShopHouseholdCustodyPinSink() = default;
  virtual Status original_source(std::string_view slot, std::size_t ordinal,
                                 const SourceRef &,
                                 std::optional<Integer> actual_schema) = 0;
  virtual Status policy(const ActorFoundationRuleRef &) = 0;
  virtual Status handling_rule(const StableId &, Integer) = 0;
  virtual Status actor(const StableId &) = 0;
  virtual Status inventory(const StableId &) = 0;
  // 保留原初始化的具型身份，不能把作者目录或初始化结果编号伪装成事实头。
  virtual Status
  inventory_initial_origin(const PrivateInventoryInitialOrigin &) = 0;
};
// 先全验托管，再发出其直接依赖，失败不宣称保留闭包完成。
Status visit_private_shop_household_custody_pins(
    const PrivateShopHouseholdCustodyContext &,
    const MomentOwnerSource &actual_source,
    PrivateShopHouseholdCustodyPinSink &);
} // namespace hanmo::resident
