#include "treaty_revision_codec_fields.hpp"
namespace hanmo::resident {
Status visit_treaty_offer_source(CodecVisitor& c,TreatyOfferSource& r){return treaty_revision_codec_fields::offer(c,r);}
Status visit_treaty_counteroffer_source(CodecVisitor& c,TreatyCounterofferSource& r){return treaty_revision_codec_fields::counteroffer(c,r);}
Status emit_treaty_offer_source_projection(ProjectionWriter& sink,std::string_view name,const TreatyOfferSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_revision_codec_fields::offer(c,v);});}
Status emit_treaty_counteroffer_source_projection(ProjectionWriter& sink,std::string_view name,const TreatyCounterofferSource& r){return emit_projection_record<ActorFoundationProjectionArchive>(sink,name,r,[](auto& c,auto& v){return treaty_revision_codec_fields::counteroffer(c,v);});}
Outcome<BoundaryValue> encode_treaty_offer_source(const TreatyOfferSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_offer_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyOfferSource> decode_treaty_offer_source(const BoundaryValue& input){TreatyOfferSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_revision_codec_fields::offer(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<BoundaryValue> encode_treaty_counteroffer_source(const TreatyCounterofferSource& r){BoundaryValue out;BoundaryProjectionWriter writer(out);auto valid=emit_treaty_counteroffer_source_projection(writer,"",r);if(valid.ok)valid=writer.finish();return {valid.ok,std::move(out),valid.error};}
Outcome<TreatyCounterofferSource> decode_treaty_counteroffer_source(const BoundaryValue& input){TreatyCounterofferSource out;ActorFoundationJsonVisitor reader(input);auto valid=treaty_revision_codec_fields::counteroffer(reader,out);if(valid.ok)valid=reader.finish();return {valid.ok,std::move(out),valid.error};}
}
