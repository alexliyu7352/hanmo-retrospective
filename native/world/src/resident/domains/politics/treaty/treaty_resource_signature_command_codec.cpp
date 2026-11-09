// Finite original command input, not a caller-authored authority requirement or
// accepted resource payload. ID/M/Basis/activation are assigned by real writers.
#include "treaty_resource_signature_write.hpp"
#include "treaty_aid_resource_selection_fields.hpp"
namespace hanmo::resident {
Status validate_ratify_treaty_resource_command2_shape(const RatifyTreatyResourceTermsCommand2&r){
 const auto&c=r.ratification;const auto&a=c.authority;
 if(a.organization_id.empty()||a.office_id.empty()||a.actor_id.empty()||a.authority_epoch<0||c.treaty_id.empty()||c.terms_revision<1||
    !valid_source_ref(c.terms_source)||c.terms_source.domain!="politics"||
    (c.terms_source.kind!="treaty_terms_offered"&&c.terms_source.kind!="treaty_terms_counteroffered")||
    !valid_source_ref(c.actual_content_receipt.source_ref)||c.actual_content_receipt.source_schema!=1)
  return common_failure("InvalidRatifyTreatyResourceCommand",c.treaty_id);
 if(bool(r.actual_aid_resource_proposal)!=bool(r.resource_knowledge))return common_failure("TreatyResourceKnowledgeCoverageMismatch",c.treaty_id);
 if(r.actual_aid_resource_proposal){const auto&p=*r.actual_aid_resource_proposal;
  if(p.source_schema!=2||!valid_source_ref(p.source_ref)||p.source_ref.domain!="politics"||p.source_ref.kind!="treaty_aid_resources_proposed")return common_failure("InvalidTreatyAidResourceProposalRef",c.treaty_id);
 }
 if(r.resource_knowledge){if(r.resource_knowledge->index()>1)return common_failure("InvalidTreatyResourceKnowledgeKind",c.treaty_id);
  if(const auto*k=std::get_if<TreatyResourceDeliveredChoice>(&*r.resource_knowledge))if(!valid_source_ref(k->actual_receipt.source_ref)||k->actual_receipt.source_schema<1)return common_failure("InvalidTreatyResourceKnowledgeReceipt",c.treaty_id);
 }
 return common_success();
}
namespace {
template<class C>Status fields(C&c,RatifyTreatyResourceTermsCommand2&r){
 Integer schema=2;c.field("schema_version",schema);if(!c.status().ok)return c.status();if(schema!=2)return c.reject("UnsupportedRatifyTreatyResourceCommandSchema");
 auto&v=r.ratification;c.object("authority",[&](auto&w){return treaty_revision_codec_fields::acting(w,v.authority);});
 c.field("treaty_id",v.treaty_id);c.field("terms_revision",v.terms_revision);
 c.object("terms_source",[&](auto&w){return treaty_terms_codec_fields::source(w,v.terms_source);});
 c.object("actual_content_receipt",[&](auto&w){return treaty_signature_codec_fields::actual_source(w,v.actual_content_receipt);});
 c.optional("actual_aid_resource_proposal",r.actual_aid_resource_proposal,[](auto&w,auto&s){return treaty_signature_codec_fields::actual_source(w,s);});
 c.optional("resource_knowledge",r.resource_knowledge,[](auto&w,auto&knowledge){
  static constexpr const char*names[]={"own_original_choice","actual_disclosure"};std::size_t index=0;
  auto valid=treaty_terms_codec_fields::variant_tag(w,knowledge,names,index);if(!valid.ok)return valid;
  if(index==0){if(w.reading())knowledge=TreatyResourceOwnChoice{};return w.status();}
  if(w.reading())knowledge=TreatyResourceDeliveredChoice{};
  return w.object("actual_receipt",[&](auto&z){return treaty_signature_codec_fields::actual_source(z,std::get<TreatyResourceDeliveredChoice>(knowledge).actual_receipt);});
 });
 if(c.status().ok){auto valid=validate_ratify_treaty_resource_command2_shape(r);if(!valid.ok)return c.reject(valid.error.original.code);}
 return c.status();
}
}
Outcome<BoundaryValue> encode_ratify_treaty_resource_command2(const RatifyTreatyResourceTermsCommand2&r){
 BoundaryValue out;BoundaryProjectionWriter w(out);
 auto valid=emit_projection_record<ActorFoundationProjectionArchive>(w,"",r,[](auto&c,auto&v){return fields(c,v);});
 if(valid.ok)valid=w.finish();return {valid.ok,std::move(out),valid.error};
}
Outcome<RatifyTreatyResourceTermsCommand2> decode_ratify_treaty_resource_command2(const BoundaryValue&input){
 RatifyTreatyResourceTermsCommand2 out;ActorFoundationJsonVisitor r(input);auto valid=fields(r,out);if(valid.ok)valid=r.finish();return {valid.ok,std::move(out),valid.error};
}
}
