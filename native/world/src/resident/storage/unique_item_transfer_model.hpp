// Original applied Common transfer evidence, owned only by its actual source row.
#pragma once
#include "resident/model/common_model.hpp"
#include <string_view>
namespace hanmo::resident {
struct OriginalUniqueItemTransferEffect {
 StableId source_inventory_id,destination_inventory_id,lot_id,instance_id,item_definition_id;
 Owner legal_owner;ReservationRef source_reservation;
 Integer source_inventory_revision_before=0,destination_inventory_revision_before=0;
};
// Borrowed producer inputs for the same nine original fields. No source key,
// authority, capture token, additional wire value, or owned temporary is added.
struct OriginalUniqueItemTransferEffectBuildView {
 std::string_view source_inventory_id,destination_inventory_id,lot_id,instance_id,item_definition_id;
 const Owner& legal_owner;const ReservationRef& source_reservation;
 Integer source_inventory_revision_before=0,destination_inventory_revision_before=0;
};
Status validate_original_unique_item_transfer_effect_shape(const OriginalUniqueItemTransferEffect&);
}
namespace hanmo::resident::unique_item_transfer_fields {
template<class C,class V>void original_effect_fields(C&c,V&v){
 c("source_inventory_id",v.source_inventory_id);c("destination_inventory_id",v.destination_inventory_id);
 c("lot_id",v.lot_id);c("instance_id",v.instance_id);c("item_definition_id",v.item_definition_id);
 c("legal_owner",v.legal_owner);c("source_reservation",v.source_reservation);
 c("source_inventory_revision_before",v.source_inventory_revision_before);
 c("destination_inventory_revision_before",v.destination_inventory_revision_before);
}
template<class C>void fields(C&c,OriginalUniqueItemTransferEffect&v){original_effect_fields(c,v);}
template<class C>void fields(C&c,const OriginalUniqueItemTransferEffectBuildView&v){original_effect_fields(c,v);}
}
