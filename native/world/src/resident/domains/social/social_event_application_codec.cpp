#include "social_event_application_codec.hpp"
#include "social_event_application_codec_fields.hpp"
#include "resident/checkpoint/projection_record_archive.hpp"
#include "resident/domains/actor_foundation/actor_foundation_json_visitor.hpp"
namespace hanmo::resident {
Status visit_social_event_original_application(CodecVisitor&c,SocialEventOriginalApplication&r){return social_event_application_codec::fields(c,r);}
Status emit_social_event_original_application_fields(ProjectionWriter&w,const SocialEventOriginalApplication&r){ActorFoundationProjectionArchive a(w);return social_event_application_codec::fields(a,const_cast<SocialEventOriginalApplication&>(r));}
Outcome<BoundaryValue> encode_social_event_original_application(const SocialEventOriginalApplication&r){BoundaryValue out;BoundaryProjectionWriter w(out);auto s=w.object("",[&](auto&x){return emit_social_event_original_application_fields(x,r);});if(s.ok)s=w.finish();return {s.ok,std::move(out),s.error};}
Outcome<SocialEventOriginalApplication> decode_social_event_original_application(const BoundaryValue&input){SocialEventOriginalApplication r;ActorFoundationJsonVisitor c(input);auto s=social_event_application_codec::fields(c,r);if(s.ok)s=c.finish();return {s.ok,std::move(r),s.error};}
}
