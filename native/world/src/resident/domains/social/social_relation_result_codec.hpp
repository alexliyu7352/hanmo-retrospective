#pragma once
#include "social_relation_result.hpp"
#include "resident/storage/common_codec.hpp"
namespace hanmo::resident {
class ProjectionWriter;
Status visit_social_relation_result(CodecVisitor&,SocialRelationResult&);
Status emit_social_relation_result_fields(ProjectionWriter&,const SocialRelationResult&);
Outcome<BoundaryValue> encode_social_relation_result(const SocialRelationResult&);
Outcome<SocialRelationResult> decode_social_relation_result(const BoundaryValue&);
}
