// Explicit original Item/Craft ownership. Repair remains an embedded Item child.
#pragma once
#include "hanmo/resident/model_contract.hpp"
namespace hanmo::resident {
struct BoundaryValue;struct WorldBundle;class CommonStore;class ProjectionWriter;
struct ItemInstancesState;struct CraftJobsState;struct FactRecord;
struct NativeItemCraftVersions {
    Integer item_instances=0,lot_properties=0,craft_jobs=0;
    Integer selected_resource_operations=0;
    bool domain_activity=false;
};
Outcome<NativeItemCraftVersions> selected_native_item_craft_versions(const CatalogIdentity&);
Status append_native_item_craft_modules(const BoundaryValue&,CatalogIdentity&);
Status prepare_native_item_craft_catalogs(const BoundaryValue&,PreparedCatalog&);
Status validate_native_item_craft_catalogs(const PreparedCatalog&);
Status initialize_native_item_craft_roots(const CommonStore&,const PreparedCatalog&,InitialWorldSeed&);
Status validate_native_item_craft_layout(const WorldBundle&);
// Bootstrap only: original typed owner/header identity and reverse coverage.
// Zero means this fact belongs to another original domain, never an Item
// child promoted to a Common fact. Business authority remains the owner gate.
Outcome<Integer> read_native_item_craft_original_fact_schema(
    const CommonStore&,const PreparedCatalog&,const ItemInstancesState*,
    const CraftJobsState*,const FactRecord&);
Status validate_native_item_craft_original_headers(
    const CommonStore&,const PreparedCatalog&,const ItemInstancesState*,const CraftJobsState*);
enum class NativeItemCraftFactProjection { all,craft,repair };
// Common carries no second Craft/Repair params DTO. The selected original live
// owner must first prove the actual Action identity before its empty params.
Status validate_native_item_craft_original_actions(
    const CommonStore&,const PreparedCatalog&,const ItemInstancesState*,const CraftJobsState*);
Status emit_native_item_craft_action_payloads_fields(
    ProjectionWriter&,const CommonStore&,const PreparedCatalog&,
    const ItemInstancesState*,const CraftJobsState*,NativeItemCraftFactProjection);

Status emit_native_item_craft_fact_payloads_fields(
    ProjectionWriter&,const CommonStore&,const PreparedCatalog&,
    const ItemInstancesState*,const CraftJobsState*,NativeItemCraftFactProjection);
}
