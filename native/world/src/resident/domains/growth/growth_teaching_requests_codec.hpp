// 教学来源独立typed叶；只有外层显式新profile/版本才可调用，旧银行不追加存在位。
#pragma once
#include "growth_teaching_requests.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
Outcome<GrowthTeachingRequestCatalog> decode_growth_teaching_request_catalog(const BoundaryValue& definitions,const BoundaryValue& config,const CommonCatalog&,const InteractionCatalog&);
Outcome<GrowthTeachingAgreement> decode_growth_teaching_agreement(const BoundaryValue&);
BoundaryValue encode_growth_teaching_agreement(const GrowthTeachingAgreement&);
Status visit_growth_teaching_agreement(CodecVisitor&,GrowthTeachingAgreement&);
}
