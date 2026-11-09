// P1 原任职与作者组织的有限拥有前置；不替代所选扩展的完整 producer。
#pragma once
#include "political_core_authority_sources.hpp"
#include "resident/domains/organization/organization_authored_lifecycle.hpp"
namespace hanmo::resident {
struct PoliticalP1PrerequisiteInputs {
  OrganizationAuthoredLifecycleInputs authored;
  PoliticalCoreAuthorityInputs authority;
  const ReadStamp &stamp;
};
// 本对象不可移动，内部 reader 只借稳定成员与同 World 原根。
// 外层 retaining scope 仍须保活 authored/authority 中跨域来源及原读票。
class NativePoliticalP1Prerequisites final : public CheckpointAllocatedObject {
  struct Sources;
  std::unique_ptr<Sources> sources_;
  NativePoliticalP1Prerequisites(const WorldBundle &, const WorldMoment &,
                                 const PoliticalP1PrerequisiteInputs &);

public:
  static Outcome<std::unique_ptr<NativePoliticalP1Prerequisites>>
  create(const WorldBundle &, const WorldMoment &,
         const PoliticalP1PrerequisiteInputs &);
  ~NativePoliticalP1Prerequisites();
  NativePoliticalP1Prerequisites(const NativePoliticalP1Prerequisites &) =
      delete;
  NativePoliticalP1Prerequisites &
  operator=(const NativePoliticalP1Prerequisites &) = delete;
  NativePoliticalP1Prerequisites(NativePoliticalP1Prerequisites &&) = delete;
  NativePoliticalP1Prerequisites &
  operator=(NativePoliticalP1Prerequisites &&) = delete;
  // 构图门不要求尚未安装的作者 origin；不据此宣称完整 source ready。
  Status validate_bindings() const;
  // 原 body 安装后必须调用，直接覆盖原父字段和全部实际 Office 引用。
  Status validate_selected_sources() const;
  const OrganizationLifecycleSourceRead &lifecycle_sources() const;
  const OrganizationRelationSourceRead &relation_sources() const;
  const OfficeTransitionSourceRead &office_transitions() const;
  const AuthorityIssueSourceRead &authority_sources() const;
  // Selected /2 returns the sole same-world concrete reader; unselected is null.
  const AuthorityBudgetRead *budgets() const noexcept;
};
} // namespace hanmo::resident
