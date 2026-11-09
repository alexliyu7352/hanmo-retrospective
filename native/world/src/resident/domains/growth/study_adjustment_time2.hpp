// 原迟答调整真实写腿/提交尾绑定；候选物理后继未登记前不得安装到任何profile。
#pragma once
#include "resident/domains/growth/growth_refs.hpp"
#include "resident/kernel/command_types.hpp"
namespace hanmo::resident {
Outcome<GrowthEffect> apply_growth_instant_at_moment(CommonStore&,GrowthState&,const GrowthCatalog&,const CommonCatalog&,const SocialState&,const GrowthAccess&,const StableId&,const GrowthQuote&,const StudyAdjustmentWriteContext&);
Status bind_study_adjustment_result_in_candidate(const GrowthState& before,CommonStore&,GrowthState& after,const GrowthCatalog&,const StableId& actor_id,Integer request_seq,const CommitResult&,const WorldMoment& actual_from,const WorldMoment& actual_to);
Status validate_study_adjustment_time2_sources(const CommonStore&,const GrowthState&,const GrowthCatalog&,const WorldMoment& actual_now);
struct StudyAdjustmentSourceView {const StudyFact* fact=nullptr;const StudyLesson* lesson=nullptr;const StudyPerformanceAdjustment* adjustment=nullptr;const StudyAdjustmentExactBinding* exact=nullptr;};
Outcome<StudyAdjustmentSourceView> read_study_adjustment_time2_source(const CommonStore&,const GrowthState&,const GrowthCatalog&,const StudyPerformanceAdjustmentRef&,const WorldMoment& actual_now);
}
