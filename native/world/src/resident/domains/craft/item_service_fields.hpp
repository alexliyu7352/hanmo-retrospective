// Original owning field callbacks shared by codec and raw retention.
#pragma once
#include "item_service.hpp"
namespace hanmo::resident::item_service_fields {
template<class A>void fields(A& a,ItemInstance& v){a("id",v.id);a("item_definition_id",v.item_definition_id);a("creation_fact_id",v.creation_fact_id);a("maker_ref",v.maker_ref);a("quality_score",v.quality_score);a("durability",v.durability);a("max_durability",v.max_durability);a("repair_policy_id",v.repair_policy_id);a("state",v.state);a("instance_version",v.instance_version);a("repair_fact_refs",v.repair_fact_refs);}
template<class A>void fields(A& a,LotProperties& v){a("properties_id",v.properties_id);a("definition_id",v.definition_id);a("schema_version",v.schema_version);a("quality_score",v.quality_score);a("fit_tag_ids",v.fit_tag_ids);a("produced_tick",v.produced_tick);a("expires_at_tick",v.expires_at_tick);a("origin_receipt_ref",v.origin_receipt_ref);a("maker_ref",v.maker_ref);a("property_policy_id",v.property_policy_id);a("property_policy_version",v.property_policy_version);}
template<class A>void fields(A& a,ItemPropertyPolicy& v){a("id",v.id);a("definition_id",v.definition_id);a("version",v.version);a("nonperishable",v.nonperishable);a("splittable",v.splittable);a("shelf_life_ticks",v.shelf_life_ticks);a("allowed_fit_tag_ids",v.allowed_fit_tag_ids);}
template<class A>void fields(A& a,ItemOutputKey& v){a("source_ref",v.source_ref);a("output_ordinal",v.output_ordinal);a("source_schema",v.source_schema);}
template<class A>void fields(A& a,ItemOutputAdoption& v){a("key",v.key);a("source_kind",v.source_kind);a("definition_id",v.definition_id);a("original_quantity",v.original_quantity);a("instance_id",v.instance_id);a("properties_ref",v.properties_ref);a("original_lot_id",v.original_lot_id);}
template<class A>void fields(A& a,ItemTerminationResult& v){a("id",v.id);a("instance_id",v.instance_id);a("cause_ref",v.cause_ref);a("cause_schema",v.cause_schema);a("usage_ordinal",v.usage_ordinal);a("effective_tick",v.effective_tick);a("instance_version_before",v.instance_version_before);a("instance_version_after",v.instance_version_after);a("source_inventory_id",v.source_inventory_id);a("source_lot_id",v.source_lot_id);}
// Full owning walk selects all genuine tails; finite wire selection is passed
// explicitly by the original codec, never inferred from row presence.
template<class A>void fields(A& a,ItemInstancesState& v,Integer version){a("instances",v.instances);a("output_adoptions",v.output_adoptions);a("terminations",v.terminations);if(version==2||version==3)a("condition_results",v.condition_results);if(version==3){a("repair_jobs",v.repair_jobs);a("repair_service_contracts",v.repair_service_contracts);}}
template<class A>void fields(A& a,ItemInstancesState& v){fields(a,v,3);}
template<class A>void fields(A& a,LotPropertiesState& v){a("properties",v.properties);a("output_adoptions",v.output_adoptions);}
template<class A>void fields(A& a,ItemServiceCatalog& v){a("property_policies",v.property_policies);}
}
