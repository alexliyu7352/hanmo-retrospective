#pragma once
#include "social_model.hpp"
#include "resident/kernel/exact_phase_graph.hpp"
namespace hanmo::resident {
class OriginalSocialCalendarRelationWrite;
struct SocialCalendarDispatch {bool exchange=true,share_news=true;};
// Selection only: an actually installed original Director route may suppress its
// same-effect calendar leg. It cannot authorize a gift, disclosure, or knowledge.
class SocialCalendarDispatchRead {public:virtual ~SocialCalendarDispatchRead()=default;virtual Outcome<SocialCalendarDispatch> dispatch(const StableId& actor)const=0;};
using SocialCalendarDispatchFunction=Outcome<SocialCalendarDispatch>(*)(const WorldBundle&,const StableId& actor);
Status validate_social_at_moment(const CommonStore&,const SocialState&,const CommonCatalog&,const SocialFactRead*,const WorldMoment&);
// 原 Calendar 实际关系写者由同候选 owning lease 借入；选中 Social2 时缺失须在任何写入前拒绝。
Outcome<SocialEffects> step_social_at_period(CommonStore&,SocialState&,const SocialCatalog&,const CommonCatalog&,const ActivityRead&,const SocialFactRead*,const ExactPeriodContext&,const WorldMoment&,const SocialCalendarDispatchRead* selected_dispatch=nullptr,OriginalSocialCalendarRelationWrite* original_relations=nullptr);
}
