// B1.1 已选作者起点的真实读取组合；不声明动态成立/迁移/终结能力已经实现。
#pragma once
#include "organization_authored_origin.hpp"
#include <memory>

namespace hanmo::resident {
struct WorldBundle;
struct EconomicFacilityInstallationReadContext;
class MilitaryFormationSourceRead;
class MilitaryReturnSourceRead;
class NativeMilitaryTrainingPeriodRead;

// 原 context 会复制到稳定成员，所有根仍借唯一 World；外层 scope/core 保活。
// installation 只在实际 active 固定作者起点使用，不能提供另一个世界的空根。
struct OrganizationAuthoredLifecycleInputs {
  OrganizationAuthoredOriginContext original;
  const EconomicFacilityInstallationReadContext *installation = nullptr;
  const MilitaryFormationSourceRead *formations = nullptr;
  const MilitaryReturnSourceRead *returns = nullptr;
  const NativeMilitaryTrainingPeriodRead *periods = nullptr;
};

// 三个正向方法反读真实作者起点/ID 集合/Office 身份。动态分支缺原 producer
// 时具名拒绝，不从 runtime 自证，不借此对象登记 complete profile ready。
class NativeOrganizationAuthoredLifecycleRead final
    : public OrganizationLifecycleSourceRead,
      public OrganizationRelationSourceRead {
  struct Sources;
  std::unique_ptr<Sources> sources_;
  Status validate_stamp(const ReadStamp &) const;
  Status validate_authored_parent_body(const OrganizationRecord &) const;

public:
  NativeOrganizationAuthoredLifecycleRead(
      const WorldBundle &, const WorldMoment &,
      const OrganizationAuthoredLifecycleInputs &);
  ~NativeOrganizationAuthoredLifecycleRead();
  NativeOrganizationAuthoredLifecycleRead(
      const NativeOrganizationAuthoredLifecycleRead &) = delete;
  NativeOrganizationAuthoredLifecycleRead &
  operator=(const NativeOrganizationAuthoredLifecycleRead &) = delete;
  NativeOrganizationAuthoredLifecycleRead(
      NativeOrganizationAuthoredLifecycleRead &&) = delete;
  NativeOrganizationAuthoredLifecycleRead &
  operator=(NativeOrganizationAuthoredLifecycleRead &&) = delete;

  // 只核同根/真实钟/原选择；不是完整 Lifecycle 或完整政治来源能力证明。
  Status validate_bindings() const;
  // 逐实际作者 seed 核真源，并拒本组合无法解释的动态 origin/历史/处置。
  // 不能以“表目前为空”跳过所选 profile 的完整 required 发布门。
  Status validate_selected_authored_sources(const ReadStamp &) const;
  // 暂定内部接缝：直接反验原父字段及既存引用索引，不新建关系账或动态关系。
  Status validate_selected_authored_relations(const ReadStamp &) const;
  Status validate_organization_relation(const OrganizationRelationBinding &,
                                        const ReadStamp &) const override;
  Status validate_authored_origin(const OrganizationRecord &,
                                  const LifecycleSeedPolicy &,
                                  const CatalogIdentity &) const override;
  Outcome<OrganizationOriginalIds>
  enumerate_original_organization_ids(const ReadStamp &) const override;
  Status validate_organization_office_origin(const OrganizationRecord &,
                                             const OfficeState &,
                                             const ReadStamp &) const override;
  Outcome<VerifiedFoundedOrganization>
  read_founded_organization(const SourceRef &,
                            const ReadStamp &) const override;
  Status validate_migration(const OrganizationLifecycleMigrationDecision &,
                            const OrganizationRecord &) const override;
  Outcome<AppliedOrganizationLifecycleCause>
  read_applied_lifecycle_cause(const SourceRef &,
                               const ReadStamp &) const override;
  Status validate_lifecycle_outcome(const OrganizationLifecycleOutcome &,
                                    const ReadStamp &) const override;
  Status validate_terminal_disposition(const OrganizationTerminalDisposition &,
                                       const OrganizationLifecycleOutcome &,
                                       const ReadStamp &) const override;
};
} // namespace hanmo::resident
