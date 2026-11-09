// 普通技艺授课的学生请求与实际课程报价；不改旧StudyFact或共同人物布局。
#pragma once
#include "resident/domains/growth/growth_model.hpp"
#include "growth_study_teaching_model.hpp"
namespace hanmo::resident {
struct GrowthStudyTeachingQuote {
    AuthoredNodeSourceProof source;
    GrowthStudyTeachingAgreement agreement;
    GrowthQuote student_course;
};
// 只借同一候选的现有读接口；不保存在世界或存档中。
struct GrowthStudyTeachingRead {
    const CommonStore& common;
    const GrowthState& growth;
    const GrowthCatalog& catalog;
    const CommonCatalog& shared;
    const SocialState& social;
    const GrowthAccess& growth_access;
    const InteractionState& interaction;
    const InteractionCatalog& interaction_catalog;
    const InteractionAccess& interaction_access;
};
Status validate_growth_study_teaching_requests(const GrowthStudyTeachingRequestCatalog&,const CommonCatalog&,const GrowthCatalog&,const InteractionCatalog&);
Outcome<GrowthStudyTeachingRequestSource> preview_growth_study_teaching_source(const GrowthStudyTeachingRead&,const GrowthStudyTeachingRequestCatalog&,const StableId& teacher,const StableId& student,const StableId& skill,Integer duration);
Status revalidate_growth_study_teaching_source(const GrowthStudyTeachingRead&,const GrowthStudyTeachingRequestCatalog&,const GrowthStudyTeachingRequestSource&);
Outcome<GrowthStudyTeachingQuote> preview_growth_study_teaching_request(const GrowthStudyTeachingRead&,const GrowthStudyTeachingRequestCatalog&,const StableId& teacher,const StableId& student,const StableId& skill,Integer duration);
Status revalidate_growth_study_teaching_request(const GrowthStudyTeachingRead&,const GrowthStudyTeachingRequestCatalog&,const GrowthStudyTeachingQuote&);
Status validate_growth_study_teaching_agreement(const CommonStore&,const CommonCatalog&,const GrowthCatalog&,const InteractionCatalog&,const GrowthStudyTeachingRequestCatalog&,const GrowthStudyTeachingAgreement&,const StudyTerms&);
}
