#pragma once
#include "social_relation_execution.hpp"
#include "social_event_original_source.hpp"
#include "resident/domains/tasks/task_exact_execution.hpp"
#include "resident/domains/growth/growth_work_service_source.hpp"
namespace hanmo::resident {
// Source-local readers for the actual original Calendar, Task, and Growth effects.
class NativeOriginalSocialRelationSourceRead final:public OriginalSocialRelationSourceRead {
 const CommonStore&common_;const SocialState&social_;const TaskState&tasks_;
 const TaskCatalog&task_catalog_;const GrowthState&growth_;const CatalogIdentity&identity_;
 const GrowthWorkServiceSources* work_services_;const OriginalSocialEventSources* events_;
public:
 NativeOriginalSocialRelationSourceRead(const CommonStore&c,const SocialState&s,
  const TaskState&t,const TaskCatalog&tc,const GrowthState&g,const CatalogIdentity&i,const GrowthWorkServiceSources* work_services=nullptr,const OriginalSocialEventSources* events=nullptr)
  :common_(c),social_(s),tasks_(t),task_catalog_(tc),growth_(g),identity_(i),work_services_(work_services),events_(events){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept override{return &common_==&c&&&social_==&s;}
 Outcome<OriginalSocialRelationApplication> read_original_application(const OriginalSocialRelationCause&,const WorldMoment&)const override;
 Status validate_original_application_before_result(const OriginalSocialRelationCause&,const FactRecord&,const WorldMoment&)const override;
 Status validate_original_application_coverage(const WorldMoment&)const override;
 bool bound_to_events(const OriginalSocialEventSources&c)const noexcept{return events_==&c&&&c.common==&common_&&&c.social==&social_&&&c.identity==&identity_;}
 bool bound_to_tasks(const TaskState&t)const noexcept{return &tasks_==&t;}
 bool bound_to_work_services(const GrowthWorkServiceSources&c)const noexcept{return work_services_==&c&&&c.common==&common_&&&c.growth==&growth_;}
};
class NativeOriginalSocialCalendarRelationWrite final:public OriginalSocialCalendarRelationWrite {
 CommonStore&common_;SocialState&social_;const OriginalSocialEventSources&events_;
 const OriginalSocialRelationCatalog&catalog_;const NativeOriginalSocialRelationSourceRead&sources_;
public:
 NativeOriginalSocialCalendarRelationWrite(CommonStore&c,SocialState&s,const OriginalSocialEventSources&events,
  const OriginalSocialRelationCatalog&catalog,const NativeOriginalSocialRelationSourceRead&sources)
  :common_(c),social_(s),events_(events),catalog_(catalog),sources_(sources){}
 bool bound_to(const CommonStore&c,const SocialState&s)const noexcept override{return &common_==&c&&&social_==&s&&sources_.bound_to(c,s)&&sources_.bound_to_events(events_);}
 Status validate_period_candidate(const ExactPeriodContext&,const WorldMoment&)const override;
 Status apply_original_calendar_event(const StableId&,const ExactPeriodContext&,const WorldMoment&)override;
};
class NativeOriginalTaskReportRelationWrite final:public OriginalTaskReportRelationWrite {
 CommonStore&common_;SocialState&social_;const TaskState&tasks_;const TaskCatalog&tasks_catalog_;
 const OriginalSocialRelationCatalog&catalog_;const NativeOriginalSocialRelationSourceRead&sources_;
public:
 NativeOriginalTaskReportRelationWrite(CommonStore&c,SocialState&s,const TaskState&t,
  const TaskCatalog&tc,const OriginalSocialRelationCatalog&catalog,
  const NativeOriginalSocialRelationSourceRead&sources)
  :common_(c),social_(s),tasks_(t),tasks_catalog_(tc),catalog_(catalog),sources_(sources){}
 bool bound_to(const CommonStore&c,const SocialState&s,const TaskState&t)const noexcept override{return &common_==&c&&&social_==&s&&&tasks_==&t&&sources_.bound_to(c,s)&&sources_.bound_to_tasks(t);}
 Status apply_original_task_report(const SourceRef&,Integer,const WorldMoment&)override;
};
class NativeOriginalGrowthServiceRelationWrite final:public OriginalGrowthServiceRelationWrite {
 CommonStore&common_;SocialState&social_;const GrowthWorkServiceSources&work_services_;
 const OriginalSocialRelationCatalog&catalog_;const NativeOriginalSocialRelationSourceRead&sources_;
public:
 NativeOriginalGrowthServiceRelationWrite(CommonStore&c,SocialState&s,const GrowthWorkServiceSources&work,
  const OriginalSocialRelationCatalog&catalog,const NativeOriginalSocialRelationSourceRead&sources)
  :common_(c),social_(s),work_services_(work),catalog_(catalog),sources_(sources){}
 bool bound_to(const CommonStore&c,const SocialState&s,const GrowthState&g)const noexcept override{return &common_==&c&&&social_==&s&&&work_services_.common==&c&&&work_services_.growth==&g&&sources_.bound_to(c,s)&&sources_.bound_to_work_services(work_services_);}
 Status validate_completion_candidate(const StableId&,const WorldMoment&)const override;
 Status apply_completed_work_service(const SourceRef&,Integer,const WorldMoment&)override;
};
}
