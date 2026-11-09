// Social精确后继与期间原源的成对冷边界；不推进世界或重新形成知识。
#pragma once
#include "social_model.hpp"
#include "social_period_sources.hpp"
#include "resident/checkpoint/codec_visitor.hpp"
namespace hanmo::resident {
class ProjectionWriter;
// Physical 3/4 keep exact old bytes; 5/6 add the actual Knowledge causal cut.
// All keep the original ten roots. No inferred migration of prior knowledge.
Outcome<DynamicSocialState> decode_dynamic_social_time2(const BoundaryValue&);
Outcome<BoundaryValue> encode_dynamic_social_time2(const DynamicSocialState&);
Status visit_dynamic_social_time2_checkpoint(CodecVisitor&,DynamicSocialState&);
Status emit_dynamic_social_time2_fields(ProjectionWriter&,const DynamicSocialState&);
// Explicit catalog-selected exports. Payload selection must equal the real catalog.
Outcome<DynamicSocialState> decode_dynamic_social_time2(const BoundaryValue&,Integer selected_version);
Outcome<BoundaryValue> encode_dynamic_social_time2(const DynamicSocialState&,Integer selected_version);
Status emit_dynamic_social_time2_fields(ProjectionWriter&,const DynamicSocialState&,Integer selected_version);
Status visit_dynamic_social_time2_checkpoint(CodecVisitor&,DynamicSocialState&,Integer selected_version);
// 物理social_period_sources/1由外层选择；载荷只含openings、closures两根。
Outcome<SocialPeriodSources> decode_social_period_sources(const BoundaryValue&);
Outcome<BoundaryValue> encode_social_period_sources(const SocialPeriodSources&);
Status visit_social_period_sources_checkpoint(CodecVisitor&,SocialPeriodSources&);
} // codec成功仅证结构；真实source、期间闭包与profile由世界validator核验。
