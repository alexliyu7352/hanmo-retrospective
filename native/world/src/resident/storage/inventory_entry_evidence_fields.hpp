// 原Entry最小库存证据的唯一字段序。A只借真实typed值，I/L由同选Common决定。
#pragma once
#include "inventory_entry_evidence.hpp"
namespace hanmo::resident::inventory_entry_fields {
template<class A>void fields(A&a,SourceRef&v,const CommonResourceCodecContext&){a("domain",v.domain);a("kind",v.kind);a("id",v.id);}
template<class A>void fields(A&a,LotPropertiesRef&v,const CommonResourceCodecContext&){a("properties_id",v.properties_id);}
template<class A>void fields(A&a,InventoryEntryLotEvidence&v,const CommonResourceCodecContext&flags){a("lot_id",v.lot_id);a("item_id",v.item_id);a("quantity",v.quantity);a("item_definition_weight",v.item_definition_weight);if(flags.item_instance_refs_enabled)a("item_instance_id",v.item_instance_id);else if(v.item_instance_id)a.reject("UnselectedEntryItemInstanceRef",v.lot_id);if(flags.lot_property_refs_enabled)a("lot_properties_ref",v.lot_properties_ref);else if(v.lot_properties_ref)a.reject("UnselectedEntryLotPropertiesRef",v.lot_id);}
template<class A>void fields(A&a,InventoryEntryEvidence&v,const CommonResourceCodecContext&flags){a("source_schema",v.source_schema);if(v.source_schema!=1){a.reject("UnsupportedInventoryEntryEvidenceSchema",v.inventory_id);return;}a("entry_source",v.entry_source);a("inventory_id",v.inventory_id);a("inventory_revision",v.inventory_revision);a("occurred_at",v.occurred_at);a("world_id",v.world_id);a("item_catalog_digest",v.item_catalog_digest);a("lots",v.lots);}
}
