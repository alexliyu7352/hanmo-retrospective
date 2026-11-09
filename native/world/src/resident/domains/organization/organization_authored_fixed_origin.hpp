// B1.1 已锁作者固定起点的真实只读组合；不包含动态成立、迁移或 mobile 实现。
#pragma once
#include "organization_authored_origin.hpp"
namespace hanmo::resident {
struct EconomicFacilityInstallationReadContext;
// 仅借同一实际目录/Common/设施安装根；拥有者必须让这些原根覆盖整个读取寿命。
struct OrganizationAuthoredFixedOriginContext {
  const OrganizationAuthoredOriginContext &original;
  const PreparedCatalog &prepared;
  const EconomicFacilityInstallationReadContext &installation;
};
// 不是 OrganizationLifecycleSourceRead 的空 full 实现：只提供本真实 authored
// 分支。 其他生命周期状态/动态业务仍由其原 owner，不能把此对象注册成完整政治
// source graph。
class NativeOrganizationAuthoredFixedOriginRead final
    : public OrganizationAuthoredOriginSourceRead {
  const OrganizationAuthoredFixedOriginContext &context_;

public:
  explicit NativeOrganizationAuthoredFixedOriginRead(
      const OrganizationAuthoredFixedOriginContext &context)
      : context_(context) {}
  Status validate_authored_origin(const OrganizationRecord &,
                                  const LifecycleSeedPolicy &,
                                  const CatalogIdentity &) const override;
};
} // namespace hanmo::resident
