// Shared by the original S1 binary visitor and S6 paired JSON/projection writers.
#pragma once
#include "passage_model.hpp"
namespace hanmo::resident::passage_boundary_codec_fields {
// Keep the original movement leaf's nested scalar representation and field order.
// No second set of boundary enum tags is introduced by another domain.
template<class Archive>Status fields(Archive& c,PassageBoundaryRef& v){
 c.object("kind",[&](auto& x){Integer tag=Integer(v.kind);x.field("enum",tag);if(x.status().ok&&(tag<0||tag>3))return x.reject("InvalidMovementEnum");if(x.reading()&&x.status().ok)v.kind=PassageBoundaryKind(tag);return x.status();});
 c.object("id",[&](auto& x){return x.field("value",v.id);});
 c.object("map_id",[&](auto& x){return x.field("value",v.map_id);});
 c.object("from_site_id",[&](auto& x){return x.field("value",v.from_site_id);});
 c.object("to_site_id",[&](auto& x){return x.field("value",v.to_site_id);});
 return c.status();
}
}
