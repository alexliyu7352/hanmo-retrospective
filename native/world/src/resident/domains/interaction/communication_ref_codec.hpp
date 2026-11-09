#pragma once
#include "communication_delivery_read.hpp"
#include "resident/model/commitment_ref_codec.hpp"
#include "resident/domains/tasks/task_dynamic_codec.hpp"
namespace hanmo::resident::communication_ref_codec {
template<class A>Status source(A&a,SourceRef&v){a.field("domain",v.domain);a.field("kind",v.kind);return a.field("id",v.id);}
template<class A>Status refs(A&a,const StableId&key,std::vector<SourceRef>&v){return a.sequence(key,v,[](auto&c,auto&x){return source(c,x);});}
template<class A>Status custodian(A&a,CommunicationCustodian&v){Integer tag=v.index();a.field("kind",tag);if(tag<0||tag>1)return a.reject("UnknownCommunicationCustodian");if(a.reading()){if(tag==0)v=CustodianActor{};else v=DeliverableProp{};}if(auto*p=std::get_if<CustodianActor>(&v))return a.field("actor_id",p->actor_id);return a.field("prop_id",std::get<DeliverableProp>(v).prop_id);}
template<class A>Status custody(A&a,CustodyReceiver&v){a.field("organization_id",v.organization_id);a.field("office_id",v.office_id);a.field("place_id",v.place_id);return a.object("custodian",[&](auto&c){return custodian(c,v.custodian);});}
template<class A>Status official(A&a,OfficialContentRef&v){dynamic_codec::enumeration(a,"kind",v.kind,20);a.field("id",v.id);a.object("terms_revision",[&](auto&c){return commitment_codec::terms(c,v.terms_revision);});
 if(!a.status().ok)return a.status();
 // Existing tags retain their original field order/validation. This new
 // finite source cannot use a key/fact masquerading as its treaty revision.
 if(v.kind==OfficialContentKind::treaty_aid_resource_proposal){auto valid=validate_official_content_ref(v);if(!valid.ok)return a.reject(valid.error.original.code);}
 return a.status();}
template<class A>Status address(A&a,RegisteredAddress&v){Integer kind=v.index();a.field("kind",kind);if(kind<0||kind>2)return a.reject("UnknownCommunicationAddress");if(a.reading()){if(kind==0)v=ActorAddress{};else if(kind==1)v=CustodyAddress{};else v=RoleAddress{};}if(auto*x=std::get_if<ActorAddress>(&v))return a.field("actor_id",x->actor_id);if(auto*x=std::get_if<CustodyAddress>(&v)){a.field("receiver_actor_id",x->receiver_actor_id);a.field("place_id",x->place_id);return a.object("custodian",[&](auto&c){return custodian(c,x->custodian);});}auto&x=std::get<RoleAddress>(v);a.field("organization_id",x.organization_id);a.field("office_id",x.office_id);a.object("scope",[&](auto&c){return dynamic_codec::scope(c,x.scope);});a.field("site_id",x.site_id);dynamic_codec::enumeration(a,"delivery_policy",x.delivery_policy,1);return a.field("required_service_kind",x.required_service_kind);}
}
