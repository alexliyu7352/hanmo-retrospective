#include "social_relation_result_codec.hpp"
#include "social_relation_result_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
Status visit_social_relation_result(CodecVisitor&c,SocialRelationResult&r){return social_relation_codec::result(c,r);}
Status emit_social_relation_result_fields(ProjectionWriter&w,const SocialRelationResult&r){ActorFoundationProjectionArchive a(w);return social_relation_codec::result(a,const_cast<SocialRelationResult&>(r));}
Outcome<BoundaryValue> encode_social_relation_result(const SocialRelationResult&r){BoundaryValue out;BoundaryProjectionWriter w(out);auto s=w.object("",[&](auto&x){return emit_social_relation_result_fields(x,r);});if(s.ok)s=w.finish();return {s.ok,std::move(out),s.error};}
Outcome<SocialRelationResult> decode_social_relation_result(const BoundaryValue&input){SocialRelationResult r;ActorFoundationJsonVisitor c(input);auto s=social_relation_codec::result(c,r);if(s.ok)s=c.finish();return {s.ok,std::move(r),s.error};}
}
