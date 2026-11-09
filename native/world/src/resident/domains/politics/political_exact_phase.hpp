// 原组织/任命Calendar尾的真实组合；数据仍归各原owner，不保存第二期间游标。
#pragma once
#include "politics_organization_runtime.hpp"
#include "politics_time2.hpp"
#include "resident/domains/organization/organization_calendar.hpp"
#include "resident/domains/organization/organization_complete_validation.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
struct PoliticalTreatyExactReadBinding;
struct PoliticalTakeoverContext;
struct PoliticalFiscalTime2Context;
struct PoliticalTreatyExactWriteBinding;
struct PoliticalExactReadBinding {
  const OrganizationCompleteValidationContext &organization;
  const EconomicCalendarReadContext &opening;
  const CommonCatalog &common_catalog;
  const LocalEconomyCatalog &economy_catalog;
  const PoliticsState &politics;
  const PoliticsCatalog &politics_catalog;
  const ActivityRead &activities;
  const DynamicResultRead &results;
  const OrganizationResourceHistoryRead &resource_history;
  const OrganizationProspectiveGrantRead &prospective_grants;
  const PoliticalTerritoryRead *territories = nullptr;
  // 仅version2真实条约/退出组合；version1不靠存在此指针反向启用。
  const PoliticalTreatyExactReadBinding *treaty_exact = nullptr;
  // 同一原 Calendar 组的真实年龄/资格读者；完整任命预览不可用空来源替代。
  const PoliticalActorQualificationRead *qualifications = nullptr;
  // 原拥有 scope 的真实业务源尾，只借同 World 原对象，不新增存档协议。
  const PoliticalTakeoverContext *takeover = nullptr;
  const PoliticalFiscalTime2Context *fiscal = nullptr;
};
struct PoliticalExactWriteBinding {
  OrganizationCalendarWriteContext &organization;
  PoliticsState &politics;
  const PoliticsCatalog &politics_catalog;
  const LocalEconomyCatalog &economy_catalog;
  const PoliticalOrganizationRuntimeWriteContext &politics_runtime;
  PoliticalTreatyExactWriteBinding *treaty_exact = nullptr;
};
class PoliticalExactReadLease {
public:
  virtual ~PoliticalExactReadLease() = default;
  virtual PoliticalExactReadBinding bindings() const = 0;
};
class PoliticalExactWriteLease {
public:
  virtual ~PoliticalExactWriteLease() = default;
  virtual PoliticalExactWriteBinding bindings() = 0;
};
// 只捕两个编译函数指针；每次绑定传入World的真实状态/clock/stamp。
// read必须实际装齐完整OrganizationRuntime来源、S5 Opening和所选成员/任命依赖。
// 新physics选择为organization/6或7 + runtime/1 +
// politics/2；旧body不走此factory。
struct PoliticalExactWorldFactory {
  Outcome<std::shared_ptr<const PoliticalExactReadLease>> (*read)(
      const WorldBundle &, const WorldMoment &) = nullptr;
  Outcome<std::shared_ptr<PoliticalExactWriteLease>> (*write)(
      WorldBundle &, const WorldMoment &) = nullptr;
};
PoliticalExactWorldFactory native_political_exact_world_factory() noexcept;
// 仅同一未发布advance候选调用。失败由整World撤回；不能独立发布部分尾部。
Status apply_original_political_calendar_in_candidate(
    WorldBundle &, const ExactPeriodContext &, PoliticalExactWorldFactory);
// 具名借用口保原World/原clock绑定；专业后继不能另构未核的factory引用。
Outcome<std::shared_ptr<const PoliticalExactReadLease>>
bind_original_political_exact_read(const WorldBundle &, const WorldMoment &,
                                   PoliticalExactWorldFactory);
Outcome<std::shared_ptr<PoliticalExactWriteLease>>
bind_original_political_exact_write(WorldBundle &, const WorldMoment &,
                                    PoliticalExactWorldFactory);
Status
validate_original_political_exact_author_selection(const WorldBundle &,
                                                   PoliticalExactWorldFactory);
Status validate_original_political_exact_world_sources(
    const WorldBundle &, const WorldMoment &, PoliticalExactWorldFactory);
ExactPhaseRegistration
    make_political_exact_phase_registration(PoliticalExactWorldFactory);
} // namespace hanmo::resident
