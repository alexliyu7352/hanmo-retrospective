// S6日历腿复用S5真实Opening和原活动writer；市场活动先完成真实S5处置才写终态。
#pragma once
#include "organization_calendar_admission.hpp"
#include "organization_original_runtime.hpp"
#include "public_market_organization.hpp"
#include "resident/domains/local_economy/economic_calendar.hpp"
namespace hanmo::resident {
struct OrganizationCalendarWriteContext {
  PublicMarketOrganizationWriteContext &organization;
  const EconomicCalendarReadContext &opening;
  const PublicMarketStaffingContext *staffing;
  PublicMarketStaffingEffectsWrite *staffing_effects;
  const OrganizationOriginalRuntimeWriteContext &original_runtime;
  const OrganizationTrainingCoverageRead *training = nullptr;
  // 暂定内部接缝：null 只借同 World 原目录证明公市未选，不代表公市能力。
  const HouseholdPurchaseCatalog *household_catalog = nullptr;
  const HouseholdRetailCatalog *retail_catalog = nullptr;
};
// 只供同candidate原political_due_close阶段；失败由整世界候选撤回，不能单独提交半期。
Outcome<OrganizationEffects>
step_organization_calendar_in_candidate(OrganizationCalendarWriteContext &,
                                        const ExactPeriodContext &);
// 精确图逐原Opening活动调用；每次返回后由组合层失效热读票并重建本候选lease。
// 不接收调用者自造的活动集合，id须属于该期真实S5 Opening。
Outcome<OrganizationEffects> step_organization_calendar_activity_in_candidate(
    OrganizationCalendarWriteContext &, const ExactPeriodContext &,
    const StableId &activity_id);
// 源时间按具名原Calendar分支反读；新的市场源交真实S6
// reader，不开放任意fact→tick适配。
class NativeOrganizationCalendarSourceMomentRead final
    : public OrganizationActivitySourceMomentRead {
public:
  NativeOrganizationCalendarSourceMomentRead(
      const CommonStore &c, const OrganizationState &s,
      const OrganizationCatalog &catalog,
      const OrganizationActivitySourceMomentRead &market)
      : common_(c), state_(s), catalog_(catalog), market_(market) {}
  Outcome<WorldMoment> read_organization_activity_source_moment(
      const OrganizationFact &) const override;

private:
  const CommonStore &common_;
  const OrganizationState &state_;
  const OrganizationCatalog &catalog_;
  const OrganizationActivitySourceMomentRead &market_;
};
} // namespace hanmo::resident
