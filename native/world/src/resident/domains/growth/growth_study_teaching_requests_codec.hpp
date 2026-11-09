// 普通授课的显式作者目录与永久来源；只有新外层模块启用才可读写。
#pragma once
#include "growth_study_teaching_requests.hpp"
namespace hanmo::resident {
Outcome<GrowthStudyTeachingRequestCatalog> decode_growth_study_teaching_catalog(const BoundaryValue&,const BoundaryValue&,const CommonCatalog&,const GrowthCatalog&,const InteractionCatalog&);
Outcome<GrowthStudyTeachingAgreement> decode_growth_study_teaching_agreement(const BoundaryValue&);
BoundaryValue encode_growth_study_teaching_agreement(const GrowthStudyTeachingAgreement&);
Status visit_growth_study_teaching_agreement(CodecVisitor&,GrowthStudyTeachingAgreement&);
}
