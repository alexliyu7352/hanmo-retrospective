#include "resident/checkpoint/checkpoint_decode_lifecycle.hpp"
#include "communication_ref_codec.hpp"
#include "resident/domains/tasks/task_dynamic_projection.hpp"
#include "communication_links.hpp"
#include "resident/model/commitment_ref_codec.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
namespace hanmo::resident {
namespace {
using namespace communication_ref_codec;
template<class A>Status fields(A&a,CommunicationLinksState&s){
 dynamic_codec::table(a,"message_business_links",s.links,[](auto&c,auto&v){c.field("id",v.id);c.field("original_public_message_ref",v.original_public_message_ref);c.object("original_business_fact_ref",[&](auto&x){return source(x,v.original_business_fact_ref);});c.field("original_business_source_schema",v.original_business_source_schema);c.object("addressed_to",[&](auto&x){return address(x,v.addressed_to);});refs(c,"route_source_refs",v.route_source_refs);refs(c,"dossier_source_refs",v.dossier_source_refs);c.optional("official_content_ref",v.official_content_ref,[](auto&x,auto&value){return official(x,value);});return c.optional("cancellation_source",v.cancellation_source,[](auto&x,auto&value){x.object("source_ref",[&](auto&z){return source(z,value.source_ref);});return x.field("source_schema",value.source_schema);});});
 dynamic_codec::table(a,"delivery_sources",s.deliveries,[](auto&c,auto&v){c.field("id",v.id);c.field("business_link_id",v.business_link_id);c.object("arrival_ref",[&](auto&x){return source(x,v.arrival_ref);});c.field("arrival_schema",v.arrival_schema);c.object("actual_receiver",[&](auto&x){return dynamic_codec::subject(x,v.actual_receiver);});c.object("authority_source",[&](auto&x){return source(x,v.authority_source);});c.field("office_epoch",v.office_epoch);c.object("delivered_at",[&](auto&x){return dynamic_codec::exact_moment(x,v.delivered_at);});c.field("contents_disclosed",v.contents_disclosed);return c.optional("custody_receiver",v.custody_receiver,[](auto&x,auto&value){return custody(x,value);});});
 dynamic_codec::table(a,"dossier_access_sources",s.dossier_accesses,[](auto&c,auto&v){c.field("id",v.id);c.field("business_link_id",v.business_link_id);c.object("original_access_ref",[&](auto&x){return source(x,v.original_access_ref);});c.field("source_schema",v.source_schema);c.object("reader",[&](auto&x){return dynamic_codec::subject(x,v.reader);});c.object("authority_source",[&](auto&x){return source(x,v.authority_source);});return c.object("accessed_at",[&](auto&x){return dynamic_codec::exact_moment(x,v.accessed_at);});});return a.status();
}
}
Outcome<CommunicationLinksState> decode_communication_links(const BoundaryValue&value){CommunicationLinksState s;dynamic_codec::JsonArchive a(value);auto r=fields(a,s);if(r.ok)r=a.finish();return r.ok?Outcome<CommunicationLinksState>{true,std::move(s),{}}:Outcome<CommunicationLinksState>{false,{},r.error};}
Status emit_communication_links_projection(ProjectionWriter&sink,std::string_view key,const CommunicationLinksState&value){return dynamic_projection::emit(sink,key,value,[](auto&a,auto&v){return fields(a,v);});}
Outcome<BoundaryValue> encode_communication_links(const CommunicationLinksState&value){return dynamic_projection::encode(value,[](auto&a,auto&v){return fields(a,v);});}
Status visit_communication_links_checkpoint(CodecVisitor&a,CommunicationLinksState&s){return fields(a,s);}
}
