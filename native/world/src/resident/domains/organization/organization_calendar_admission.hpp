// 原Calendar活动的首合法窗口；批准不等于供工，返回值不是工作量或完成凭据。
#pragma once
#include "resident/domains/local_economy/economic_calendar.hpp"
namespace hanmo::resident {
struct OrganizationCalendarWindow {
 Integer from_tick=0,until_tick=0;
 std::optional<SourceRef> excluded_current_opening;
};
struct OrganizationActualCalendarWindow {
 OrganizationCalendarWindow approved_window;
 // 原S5既存集合已实际冻结后才有值，不在批准时预分配Opening来源。
 const EconomicPeriodOpening* actual_first_opening=nullptr;
};
class OrganizationCalendarAdmissionRead {
public:virtual ~OrganizationCalendarAdmissionRead()=default;
 virtual Outcome<OrganizationCalendarWindow> read_new_calendar_window(Integer duration_ticks)const=0;
 virtual Outcome<OrganizationActualCalendarWindow> read_original_calendar_window(const SourceRef& approval,Integer duration_ticks)const=0;
 virtual Status validate_completed_calendar_window(const SourceRef& approval,Integer duration_ticks)const=0;
};
// 仅同World owning lease构造；timeline和原Opening源不得跨candidate借用。
class NativeOrganizationCalendarAdmissionRead final:public OrganizationCalendarAdmissionRead {
 const WorldTimelineState& timeline_;const EconomicCalendarReadContext& original_;
public:
 NativeOrganizationCalendarAdmissionRead(const WorldTimelineState& t,const EconomicCalendarReadContext& x):timeline_(t),original_(x){}
 Outcome<OrganizationCalendarWindow> read_new_calendar_window(Integer)const override;
 Outcome<OrganizationActualCalendarWindow> read_original_calendar_window(const SourceRef&,Integer)const override;
 Status validate_completed_calendar_window(const SourceRef&,Integer)const override;
};
// S5原人数、assignment、场所、逐期供养必须覆盖这个真实服务窗。
// 不能将今日allocation或已到due当作实际训练；结果与人数仍由原S6 writer采用。
class OrganizationTrainingCoverageRead {
public:virtual ~OrganizationTrainingCoverageRead()=default;
 virtual Status validate_original_training_window(const OrgActivity&,const OrgActivityTerms&,
  const SourceRef& actual_approval,Integer from_tick,Integer until_tick)const=0;
};
Status validate_organization_calendar_windows(const OrganizationState&,const OrganizationCatalog&,
 const OrganizationCalendarAdmissionRead&,const OrganizationTrainingCoverageRead*);
}
