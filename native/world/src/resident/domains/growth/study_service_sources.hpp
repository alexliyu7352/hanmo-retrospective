// 原成功start同候选接受绑定；没有独立订单、课程ID或第二余额。
#pragma once
#include "study_lesson_capability.hpp"
namespace hanmo::resident {
class GrowthStartedParticipationWrite;
struct GrowthExactStartContext {const ActorFoundationCatalog& actor_foundation;WorldMoment actual_moment;GrowthStartedParticipationWrite* participation=nullptr;};
Status start_growth_at_moment(CommonStore&,GrowthState&,const GrowthCatalog&,const CommonCatalog&,const SocialState&,const GrowthAccess&,const ActionCoreView&,const GrowthQuote&,const GrowthTeachingStartAdmission*,const GrowthExactStartContext&);
// 由start_growth真实重验/预约/写原Fact之后在同一栈调用。
Status bind_study_service_after_actual_start(CommonStore&,GrowthState&,const GrowthCatalog&,const ActionCoreView&,const GrowthExactStartContext&);
Outcome<const StudyServiceBindingV2*> read_study_accepted_execution(const CommonStore&,const GrowthState&,const GrowthCatalog&,const ActorFoundationCatalog&,const StableId& action_id,const WorldMoment& actual_now,const GrowthAccess&);
// 原FSL唯一Started已真实写入后建立纯时间映射；不另推进S0或授课。
Status bind_study_time_after_actual_start(CommonStore&,GrowthState&,const GrowthCatalog&,const ActorFoundationCatalog&,const StableId& started_boundary_id,const WorldMoment& actual_now);
Status bind_study_time_after_actual_boundary(CommonStore&,GrowthState&,const StableId& actual_boundary_id);
// Borrow the original terminal time/boundary only. This does not waive the
// strict release-commit reader or issue a private capture proof.
Outcome<const StudyTerminalTime*> read_study_original_terminal_time(const GrowthState&,const StudyTerminal&,const WorldMoment& actual_now);
Status validate_study_time_bindings(const CommonStore&,const GrowthState&,const GrowthCatalog&,const ActorFoundationCatalog&,const ActionElapsedStore&,const TimeScaleRef&,const WorldMoment& actual_now,const GrowthAccess&);
}
