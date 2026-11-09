// One finite field table for JSON, binary and shape. Catalog refs are not facts.
#pragma once
#include "mobile_inventory_adoption.hpp"
#include "resident/kernel/exact_struct_codec.hpp"
namespace hanmo::resident::mobile_inventory_codec {
template<class C>Status source(C& c,SourceRef& v){c.field("domain",v.domain);c.field("kind",v.kind);return c.field("id",v.id);}
template<class C>Status source(C& c,MomentOwnerSource& v){c.object("source_ref",[&](auto& x){return source(x,v.source_ref);});return c.field("source_schema",v.source_schema);}
template<class C>Status adoption(C& c,MobileInventoryAdoptionView& v){c.field("inventory_id",v.inventory_id);c.optional("vehicle_ref",v.vehicle_ref,[](auto& x,auto& id){return x.field("value",id);});c.object("original_container_source",[&](auto& x){return source(x,v.original_container_source);});c.object("spatial_source",[&](auto& x){return source(x,v.spatial_source);});c.object("capacity_source",[&](auto& x){return source(x,v.capacity_source);});return c.object("adopted_at",[&](auto& x){return exact_fields::moment(x,v.adopted_at);});}
template<class C>Status definition(C& c,MobileInventoryDefinition& v){c.field("id",v.id);c.field("inventory_id",v.inventory_id);c.field("initial_site_id",v.initial_site_id);c.field("version",v.version);c.field("container_maximum_weight",v.container_maximum_weight);return c.object("initialized_at",[&](auto& x){return exact_fields::moment(x,v.initialized_at);});}
}
