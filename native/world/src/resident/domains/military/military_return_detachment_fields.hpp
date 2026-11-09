#pragma once
#include "military_return_detachment_model.hpp"
namespace hanmo::resident::military_return_detachment_fields {
template<class C>Status release(C& c,MilitaryReturnDetachmentRelease1& v){
 c.field("estate_id",v.estate_id);c.field("actor_id",v.actor_id);c.field("source_id",v.source_id);
 c.optional("detached_before",v.detached_before,[](auto& child,auto& value){return child.field("value",value);});
 c.optional("detached_after",v.detached_after,[](auto& child,auto& value){return child.field("value",value);});
 return c.status();
}
template<class C>Status value(C& c,MilitaryReturnDetachmentReleases1& v){
 return c.sequence("releases",v.releases,[](auto& child,auto& row){return release(child,row);});
}
}
