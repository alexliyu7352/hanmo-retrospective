// Original S1 carrier union, shared without translating its nested wire shape.
#pragma once
#include "spatial_carrier_model.hpp"
namespace hanmo::resident::spatial_carrier_ref_codec_fields {
template<class Archive,class T>Status scalar(Archive& c,const char* name,T& v){return c.object(name,[&](auto& x){return x.field("value",v);});}
template<class Archive>Status subject_fields(Archive& c,SubjectRef& v){
 c.object("kind",[&](auto& x){Integer tag=Integer(v.kind);x.field("enum",tag);if(x.status().ok&&(tag<0||tag>2))return x.reject("InvalidMovementEnum");if(x.reading()&&x.status().ok)v.kind=SubjectKind(tag);return x.status();});
 scalar(c,"id",v.id);return c.status();
}
template<class Archive>Status transport_unit_fields(Archive& c,TransportUnitRef& v){c.object("subject",[&](auto& x){return subject_fields(x,v.subject);});scalar(c,"transport_unit_id",v.transport_unit_id);return c.status();}
template<class Archive>Status branch_fields(Archive& c,ActorCarrierRef& v){return scalar(c,"actor_id",v.actor_id);}
template<class Archive>Status branch_fields(Archive& c,PartyCarrierRef& v){return scalar(c,"party_id",v.party_id);}
template<class Archive>Status branch_fields(Archive& c,ArmyCarrierRef& v){return scalar(c,"army_id",v.army_id);}
template<class Archive>Status branch_fields(Archive& c,FleetCarrierRef& v){return scalar(c,"fleet_id",v.fleet_id);}
template<class Archive>Status branch_fields(Archive& c,TransportUnitCarrierRef& v){c.object("transport_unit_ref",[&](auto& x){return transport_unit_fields(x,v.transport_unit_ref);});scalar(c,"composition_revision",v.composition_revision);return c.status();}
template<class Archive>Status fields(Archive& c,SpatialCarrierRef& v){
 Integer tag=Integer(v.index());c.field("variant",tag);
 if(c.status().ok&&(tag<0||tag>4))return c.reject("InvalidMovementVariant");
 if(c.reading()&&c.status().ok){switch(tag){case 0:v=ActorCarrierRef{};break;case 1:v=PartyCarrierRef{};break;case 2:v=ArmyCarrierRef{};break;case 3:v=FleetCarrierRef{};break;case 4:v=TransportUnitCarrierRef{};break;}}
 if(c.status().ok)return std::visit([&](auto& branch){return branch_fields(c,branch);},v);
 return c.status();
}
}
