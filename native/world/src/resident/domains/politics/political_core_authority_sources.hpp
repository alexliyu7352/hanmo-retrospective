// 已有 World 原根的七个权源叶组合；不是尚缺预算/公市产权/条约根的完整工厂。
#pragma once
#include "resident/domains/organization/authority_basis.hpp"
#include <memory>

namespace hanmo::resident {
struct WorldBundle;
struct PublicMarketOrganizationReadContext;
struct PoliticalFiscalTime2Context;
struct PoliticalTakeoverContext;
struct GovernanceProgramReadContext;
class PoliticalBudgetRead;
// 可选项只能随真实 World/目录所选根存在；已选根漏传不降级为空能力。
// 原 context 中跨域 reader 由外层 retaining scope/core
// 保活，不能独立缓存本对象。
struct PoliticalCoreAuthorityInputs {
  const PublicMarketOrganizationReadContext *market = nullptr;
  const PoliticalFiscalTime2Context *fiscal = nullptr;
  const PoliticalTakeoverContext *takeover = nullptr;
  const GovernanceProgramReadContext *governance = nullptr;
  const PoliticalBudgetRead *budgets = nullptr;
};
class NativePoliticalCoreAuthorityRead final : public AuthorityIssueSourceRead {
  struct Sources;
  std::unique_ptr<Sources> sources_;

public:
  // 实际持有原 context 副本和 concrete reader，借值仍指唯一 World 原根。
  NativePoliticalCoreAuthorityRead(const WorldBundle &, const WorldMoment &,
                                   const PoliticalCoreAuthorityInputs &);
  ~NativePoliticalCoreAuthorityRead();
  NativePoliticalCoreAuthorityRead(const NativePoliticalCoreAuthorityRead &) =
      delete;
  NativePoliticalCoreAuthorityRead &
  operator=(const NativePoliticalCoreAuthorityRead &) = delete;
  NativePoliticalCoreAuthorityRead(NativePoliticalCoreAuthorityRead &&) =
      delete;
  NativePoliticalCoreAuthorityRead &
  operator=(NativePoliticalCoreAuthorityRead &&) = delete;
  // 缺失的后三组物理根若被选中，明确拒绝本有限组合；绝不注册完整 ready。
  Status validate_bindings() const;
  Outcome<WorldMoment> read_organization_authority_now() const override;
  Outcome<AuthorityIssueSourceView>
  read_authority_issue_source(const SourceRef &) const override;
};
} // namespace hanmo::resident
