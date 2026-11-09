// The original relation writer, real application order and immutable initial
// author basis. Effective evaluations are a separate consumer of this base.
#pragma once
#include "social_model.hpp"
#include "social_relation_cut.hpp"
namespace hanmo::resident {
enum class OriginalSocialRelationRule {theft_victim,theft_actor,refusal,aid_receiver,aid_helper,heard_theft,heard_aid,successful_task_report,completed_work_service};
struct OriginalSocialRelationDelta {Integer trust=0,affinity=0;};
Outcome<OriginalSocialRelationDelta> original_social_relation_delta(OriginalSocialRelationRule);
Status change_original_social_relation_legacy(CommonWrite&,SocialState&,const StableId& from,const StableId& to,OriginalSocialRelationRule);
struct OriginalSocialRelationApplication {
 const StableId* from_actor=nullptr;const StableId* to_actor=nullptr;
 const WorldMoment* occurred_at=nullptr;
 OriginalSocialRelationRule rule=OriginalSocialRelationRule::theft_victim;
};
class OriginalSocialRelationSourceRead {
public:virtual ~OriginalSocialRelationSourceRead()=default;
 virtual bool bound_to(const CommonStore&,const SocialState&)const noexcept=0;
 // Source-local only: never calls effective Social or this result fold.
 virtual Outcome<OriginalSocialRelationApplication> read_original_application(
  const OriginalSocialRelationCause&,const WorldMoment&)const=0;
 virtual Status validate_original_application_before_result(
  const OriginalSocialRelationCause&,const FactRecord&,const WorldMoment&)const=0;
 // Every selected original effect must have exactly one matching result.
 virtual Status validate_original_application_coverage(const WorldMoment&)const=0;
};
struct OriginalSocialRelationContext {
 const CommonStore& common;const SocialState& social;
 const OriginalSocialRelationCatalog& catalog;
 const OriginalSocialRelationSourceRead& sources;const WorldMoment& actual_now;
};
struct OriginalSocialRelationView {
 const StableId* relation_record_key=nullptr;
 Integer trust=0,affinity=0;
 const SocialRelationResult* last_application=nullptr;
};
Outcome<OriginalSocialRelationCatalog> decode_original_social_relation_catalog(
 const BoundaryValue& locked_data,const CatalogIdentity& actual_identity);
Status validate_original_social_relations(const OriginalSocialRelationContext&);
Outcome<const SocialRelationResult*> read_original_social_relation_result(
 const OriginalSocialRelationContext&,const SourceRef&,Integer schema);
Outcome<OriginalSocialRelationView> read_original_social_relation_at_cut(
 const OriginalSocialRelationContext&,const StableId& from,const StableId& to,
 const OriginalSocialRelationCut&);
Outcome<const SocialRelationResult*> apply_original_social_relation_in_candidate(
 CommonStore&,SocialState&,const OriginalSocialRelationContext&,
 const OriginalSocialRelationCause&,const WorldMoment& actual_application);
struct TaskState;struct GrowthState;struct ExactPeriodContext;
class OriginalSocialCalendarRelationWrite {
public:virtual ~OriginalSocialCalendarRelationWrite()=default;
 virtual bool bound_to(const CommonStore&,const SocialState&)const noexcept=0;
 // Before any resource, memory, event, or relation write in this Calendar call.
 virtual Status validate_period_candidate(const ExactPeriodContext&,const WorldMoment&)const=0;
 // Immediately after the original event insertion, before any relation result.
 virtual Status apply_original_calendar_event(const StableId& event_id,
  const ExactPeriodContext&,const WorldMoment&)=0;
};
class OriginalGrowthServiceRelationWrite {
public:virtual ~OriginalGrowthServiceRelationWrite()=default;
 virtual bool bound_to(const CommonStore&,const SocialState&,const GrowthState&)const noexcept=0;
 virtual Status validate_completion_candidate(const StableId& action,const WorldMoment& actual_now)const=0;
 // Called after the unique original terminal publication has been appended.
 virtual Status apply_completed_work_service(const SourceRef& actual_boundary,Integer schema,
  const WorldMoment& actual_application)=0;
};
class OriginalTaskReportRelationWrite {
public:virtual ~OriginalTaskReportRelationWrite()=default;
 virtual bool bound_to(const CommonStore&,const SocialState&,const TaskState&)const noexcept=0;
 virtual Status apply_original_task_report(const SourceRef& terminal,Integer schema,
  const WorldMoment& actual_application)=0;
};
}
