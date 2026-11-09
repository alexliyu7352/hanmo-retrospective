#include "treaty_aid_recipient_acceptance_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_aid_recipient_acceptance(CodecVisitor&c,TreatyAidRecipientAcceptance&r){return treaty_aid_recipient_acceptance_fields::fields(c,r);}
Status emit_treaty_aid_recipient_acceptance_projection(ProjectionWriter&w,std::string_view name,const TreatyAidRecipientAcceptance&r){return emit_projection_record<ActorFoundationProjectionArchive>(w,name,r,[](auto&c,auto&v){return treaty_aid_recipient_acceptance_fields::fields(c,v);});}
Outcome<BoundaryValue> encode_treaty_aid_recipient_acceptance(const TreatyAidRecipientAcceptance&r){BoundaryValue out;BoundaryProjectionWriter w(out);auto valid=emit_treaty_aid_recipient_acceptance_projection(w,"",r);if(valid.ok)valid=w.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyAidRecipientAcceptance> decode_treaty_aid_recipient_acceptance(const BoundaryValue&input){TreatyAidRecipientAcceptance out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_aid_recipient_acceptance_fields::fields(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
