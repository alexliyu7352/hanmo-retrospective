#include "resident/kernel/catalog_section_observation.hpp"
#include "native_item_craft_state.hpp"
#include "world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
#include "resident/checkpoint/common_codec_selection.hpp"
#include "resident/domains/craft/item_service_codec.hpp"
#include "resident/domains/craft/item_service_initial.hpp"
#include "resident/domains/craft/craft_job.hpp"
#include "resident/domains/craft/craft_job_codec.hpp"
#include "resident/domains/craft/craft_recipe.hpp"
#include "resident/domains/craft/repair_policy.hpp"
#include "resident/domains/craft/repair_codec.hpp"
#include "resident/domains/commerce/repair_service/repair_service_rules.hpp"
#include "resident/domains/commerce/repair_service/repair_service_codec.hpp"
namespace hanmo::resident {namespace {
const BoundaryValue* block(const BoundaryValue& scenario,const char* name){
    const BoundaryValue* found=nullptr;
    for(const auto& row:boundary::object(scenario,"scenario"))if(row.first==name){
        if(found)boundary::fail("DuplicateItemCraftSelection",name);
        found=&row.second;
    }
    return found;
}
Integer item_selection(const BoundaryValue& value){
    boundary::fields(value,{"profile","item_service_catalog"},{"repair_policies","repair_service_rules"},"item_service");
    const auto profile=boundary::get_string(value,"profile","item_service");
    const Integer version=profile=="original_item_service_v1"?1:
        profile=="original_item_service_v2"?2:profile=="original_item_service_v3"?3:0;
    if(!version)boundary::unsupported("UnsupportedItemServiceSelection","item_service/profile");
    const bool policies=value.find("repair_policies"),rules=value.find("repair_service_rules");
    if(policies!=(version==3)||rules!=(version==3)||
       boundary::object(value,"item_service").size()!=(version==3?4u:2u))
        boundary::fail("ItemRepairSelectedCatalogPairRequired","item_service");
    return version;
}
void selected_resources_selection(const BoundaryValue& value){
    boundary::fields(value,{"profile"},{},"selected_resource_operations");
    if(boundary::object(value,"selected_resource_operations").size()!=1||
       boundary::get_string_ref(value,"profile","selected_resource_operations")!="original_selected_resource_operations_v1")
        boundary::unsupported("UnsupportedSelectedResourceOperationSelection","selected_resource_operations/profile");
}
void craft_selection(const BoundaryValue& value){
    boundary::fields(value,{"profile","craft_recipe_catalog"},{},"craft_jobs");
    if(boundary::object(value,"craft_jobs").size()!=2||
       boundary::get_string(value,"profile","craft_jobs")!="original_craft_jobs_v1")
        boundary::unsupported("UnsupportedCraftJobsSelection","craft_jobs/profile");
}
}
Outcome<NativeItemCraftVersions> selected_native_item_craft_versions(const CatalogIdentity& identity){
    NativeItemCraftVersions out;
    for(const auto& row:identity.module_versions){
        Integer* target=row.first=="item_instances"?&out.item_instances:
            row.first=="lot_properties"?&out.lot_properties:row.first=="craft_jobs"?&out.craft_jobs:nullptr;
        if(target){
            if(*target||(row.second!="1"&&
               (target!=&out.item_instances||(row.second!="2"&&row.second!="3"))))
                return {false,{},common_failure("ItemCraftModuleMismatch",row.first).error};
            *target=row.second=="3"?3:row.second=="2"?2:1;
        }else if(row.first=="selected_resource_operations"){
            if(out.selected_resource_operations||row.second!="1")return {false,{},common_failure("SelectedResourceModuleMismatch",row.first).error};
            out.selected_resource_operations=1;
        }else if(row.first=="domain_activity_reservations"){
            if(out.domain_activity||row.second!="1")
                return {false,{},common_failure("ItemCraftModuleMismatch",row.first).error};
            out.domain_activity=true;
        }
    }
    if(bool(out.item_instances)!=bool(out.lot_properties)||
       (out.craft_jobs&&!out.item_instances)||
       ((out.item_instances==3||out.craft_jobs)&&!out.domain_activity))
        return {false,{},common_failure("ItemCraftOriginalModulePairRequired").error};
    return {true,out,{}};
}
Status append_native_item_craft_modules(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    for(const auto& row:identity.module_versions)
        if(row.first=="item_instances"||row.first=="lot_properties"||row.first=="craft_jobs"||row.first=="selected_resource_operations")
            return common_failure("DuplicateItemCraftModule",row.first);
    const auto* item=block(scenario,"item_service");const auto* craft=block(scenario,"craft_jobs");
    if(const auto* selected=block(scenario,"selected_resource_operations")){selected_resources_selection(*selected);identity.module_versions.emplace_back("selected_resource_operations","1");}
    const Integer version=item?item_selection(*item):0;
    if(craft){craft_selection(*craft);if(!item)return common_failure("CraftOriginalItemCatalogRequired");}
    if(item){identity.module_versions.emplace_back("item_instances",std::to_string(version));identity.module_versions.emplace_back("lot_properties","1");}
    if(craft)identity.module_versions.emplace_back("craft_jobs","1");
    if(version==3||craft){
        bool present=false;
        for(const auto& row:identity.module_versions)if(row.first=="domain_activity_reservations"){
            if(present||row.second!="1")return common_failure("ItemCraftModuleMismatch",row.first);
            present=true;
        }
        if(!present)identity.module_versions.emplace_back("domain_activity_reservations","1");
    }
    auto selected=selected_native_item_craft_versions(identity);
    return selected.ok?common_success():Status{false,{},std::move(selected.error)};
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_native_item_craft_catalogs(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 141, "PreparedCatalog", "validate_native_item_craft_catalogs", &catalog);

    auto selected=selected_native_item_craft_versions(catalog.identity);
    if(!selected.ok)return {false,{},std::move(selected.error)};
    const auto& v=selected.value;
    if(bool(v.item_instances)!=bool(catalog.item_services)||
       (v.item_instances==3)!=bool(catalog.repair_policies)||
       (v.item_instances==3)!=bool(catalog.repair_service_rules)||
       bool(v.craft_jobs)!=bool(catalog.craft_recipes))
        return common_failure("ItemCraftSelectedCatalogMismatch");
    if((catalog.common&&catalog.common->selected_resource_operations_enabled!=bool(v.selected_resource_operations))||
       (!catalog.common&&v.selected_resource_operations))
        return common_failure("SelectedResourceCommonCatalogMismatch");
    if(v.selected_resource_operations&&(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)))
        return common_failure("SelectedResourceCompleteOriginalSourcesRequired",{},ErrorKind::unsupported_capability);
    if(!v.item_instances)return common_success();
    if(!catalog.common||!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))
        return common_failure("ItemCraftCompleteOriginalCatalogsRequired",{},ErrorKind::unsupported_capability);
    // The original F contract stores Repair placement events only in P20.
    // This complete-profile dependency does not change the general selector.
    if(v.item_instances==3&&v.craft_jobs!=1)
        return common_failure("CompleteRepairCraftFacilityOwnerRequired","craft_jobs",ErrorKind::unsupported_capability);
    auto physical=select_common_codec(5,catalog.identity);if(!physical.ok)return {false,{},std::move(physical.error)};
    if(!physical.value.features.item_instances||!physical.value.features.lot_properties||
       catalog.common->item_instances_enabled!=physical.value.features.item_instances||
       catalog.common->lot_properties_enabled!=physical.value.features.lot_properties||
       catalog.common->domain_activity_reservations_enabled!=physical.value.features.domain_activity)
        return common_failure("ItemCraftCommonSelectedFeaturesMismatch");
    auto valid=validate_item_service_catalog(*catalog.item_services,*catalog.common);if(!valid.ok)return valid;
    if(catalog.repair_policies){
        valid=validate_repair_policy_catalog(*catalog.repair_policies,*catalog.common);if(!valid.ok)return valid;
        valid=validate_repair_service_rule_catalog(*catalog.repair_service_rules,*catalog.repair_policies);if(!valid.ok)return valid;
    }
    if(catalog.craft_recipes)return validate_craft_recipe_catalog(*catalog.craft_recipes,*catalog.common,*catalog.item_services);
    return common_success();
}
Status prepare_native_item_craft_catalogs(const BoundaryValue& scenario,PreparedCatalog& catalog){try{
    if(catalog.item_services||catalog.repair_policies||catalog.repair_service_rules||catalog.craft_recipes)
        return common_failure("ItemCraftCatalogAlreadyPrepared",{},ErrorKind::internal);
    auto selected=selected_native_item_craft_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    const auto* item=block(scenario,"item_service");const auto* craft=block(scenario,"craft_jobs");
    const auto* resources=block(scenario,"selected_resource_operations");
    if(bool(resources)!=bool(selected.value.selected_resource_operations))return common_failure("SelectedResourceCatalogSelectionMismatch");
    if(resources)selected_resources_selection(*resources);
    if(bool(item)!=bool(selected.value.item_instances)||bool(craft)!=bool(selected.value.craft_jobs))
        return common_failure("ItemCraftSelectedCatalogMismatch");
    if(item){
        if(!catalog.common||item_selection(*item)!=selected.value.item_instances)return common_failure("ItemCraftSelectedCatalogMismatch");
        {
            ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::item_services);
            auto decoded=decode_item_service_catalog(boundary::field(*item,"item_service_catalog","item_service"),*catalog.common);
            if(!decoded.ok)return {false,{},std::move(decoded.error)};
            catalog.item_services=checkpoint_make_shared<const ItemServiceCatalog>(std::move(decoded.value));
        }
        if(selected.value.item_instances==3){
            {
                ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::repair_policies);
                auto decoded=decode_repair_policy_catalog(boundary::field(*item,"repair_policies","item_service"),*catalog.common);
                if(!decoded.ok)return {false,{},std::move(decoded.error)};
                catalog.repair_policies=checkpoint_make_shared<const RepairPolicyCatalog>(std::move(decoded.value));
            }
            {
                ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::repair_service_rules);
                auto decoded=decode_repair_service_rule_catalog(boundary::field(*item,"repair_service_rules","item_service"),*catalog.repair_policies);
                if(!decoded.ok)return {false,{},std::move(decoded.error)};
                catalog.repair_service_rules=checkpoint_make_shared<const RepairServiceRuleCatalog>(std::move(decoded.value));
            }
        }
    }
    if(craft){
        craft_selection(*craft);
        ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::craft_recipes);
        auto decoded=decode_craft_recipe_catalog(boundary::field(*craft,"craft_recipe_catalog","craft_jobs"),*catalog.common,*catalog.item_services);
        if(!decoded.ok)return {false,{},std::move(decoded.error)};
        catalog.craft_recipes=checkpoint_make_shared<const CraftRecipeCatalog>(std::move(decoded.value));
    }
    return validate_native_item_craft_catalogs(catalog);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status initialize_native_item_craft_roots(const CommonStore& common,const PreparedCatalog& catalog,InitialWorldSeed& seed){
    auto valid=validate_native_item_craft_catalogs(catalog);if(!valid.ok)return valid;
    if(seed.item_instances||seed.lot_properties||seed.craft_jobs)return common_failure("ItemCraftRootsAlreadyInitialized",{},ErrorKind::internal);
    auto selected=selected_native_item_craft_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(selected.value.item_instances){
        auto fresh=make_initial_item_service(common,*catalog.common,*catalog.item_services,selected.value.item_instances,
            catalog.repair_policies.get(),catalog.repair_service_rules.get());
        if(!fresh.ok)return {false,{},std::move(fresh.error)};
        seed.item_instances=checkpoint_make_shared<const ItemInstancesState>(std::move(fresh.value.instances));
        seed.lot_properties=checkpoint_make_shared<const LotPropertiesState>(std::move(fresh.value.properties));
    }
    if(selected.value.craft_jobs){
        auto fresh=make_initial_craft_jobs(common,*catalog.common,*catalog.item_services,*catalog.craft_recipes,selected.value.craft_jobs);
        if(!fresh.ok)return {false,{},std::move(fresh.error)};
        seed.craft_jobs=checkpoint_make_shared<const CraftJobsState>(std::move(fresh.value));
    }
    return common_success();
}
Status validate_native_item_craft_layout(const WorldBundle& world){
    if(!world.catalog)return common_failure("ItemCraftCatalogRequired",{},ErrorKind::internal);
    auto valid=validate_native_item_craft_catalogs(*world.catalog);if(!valid.ok)return valid;
    auto selected=selected_native_item_craft_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value.item_instances)!=bool(world.item_instances)||
       bool(selected.value.lot_properties)!=bool(world.lot_properties)||
       bool(selected.value.craft_jobs)!=bool(world.craft_jobs)||
       (world.craft_jobs&&world.craft_jobs->schema_version!=1))
        return common_failure("ItemCraftSelectedRootMismatch");
    // This is only physical ownership. Original materialization, title, custody,
    // fee, skill and independent Repair inspection proofs remain mandatory in
    // the complete owning world admission, not replaced by this layout gate.
    return common_success();
}
namespace {
enum class OriginalItemCraftFact { other,craft,repair,service };
OriginalItemCraftFact original_fact_family(const FactRecord& head){
    if(is_craft_original_fact_kind(head.kind))return OriginalItemCraftFact::craft;
    if(is_repair_original_fact_kind(head.kind))return OriginalItemCraftFact::repair;
    if(is_repair_service_original_fact_kind(head.kind))return OriginalItemCraftFact::service;
    return OriginalItemCraftFact::other;
}
class CheckedOriginalHeaders final:public CraftOriginalFactHeaderSink,
    public RepairServiceOriginalFactHeaderSink {
public:
    Status append(const FactRecord&,Integer schema)override{
        return schema==1?common_success():common_failure("ItemCraftOriginalSchemaMismatch");
    }
};
}
Outcome<Integer> read_native_item_craft_original_fact_schema(
    const CommonStore& common,const PreparedCatalog& catalog,const ItemInstancesState* items,
    const CraftJobsState* jobs,const FactRecord& header){
    auto selected=selected_native_item_craft_versions(catalog.identity);
    if(!selected.ok)return {false,0,std::move(selected.error)};
    switch(original_fact_family(header)){
    case OriginalItemCraftFact::craft:
        if(!selected.value.craft_jobs||!jobs)
            return {false,0,common_failure("CraftOriginalSelectedOwnerRequired",header.id).error};
        return read_craft_original_fact_payload_schema(common,*jobs,header);
    case OriginalItemCraftFact::repair:
        if(selected.value.item_instances!=3||!items)
            return {false,0,common_failure("RepairOriginalSelectedOwnerRequired",header.id).error};
        return read_repair_original_fact_payload_schema(common,*items,header);
    case OriginalItemCraftFact::service:
        if(selected.value.item_instances!=3||!items)
            return {false,0,common_failure("RepairServiceOriginalSelectedOwnerRequired",header.id).error};
        return read_repair_service_original_payload_schema(common,*items,header);
    case OriginalItemCraftFact::other:
        // These addresses belong to a retained physical child, not Common.
        if(header.domain=="commerce"&&(header.kind=="repair_custody_item_transfer"||
           header.kind=="repair_return_item_transfer"))
            return {false,0,common_failure("RepairOwnedTransferCannotBeCommonHeader",header.id).error};
        return {true,0,{}};
    }
    return {false,0,common_failure("ItemCraftOriginalSourceKindMismatch",header.id).error};
}
Status validate_native_item_craft_original_headers(const CommonStore& common,
    const PreparedCatalog& catalog,const ItemInstancesState* items,const CraftJobsState* jobs){
    auto selected=selected_native_item_craft_versions(catalog.identity);
    if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value.item_instances)!=bool(items)||bool(selected.value.craft_jobs)!=bool(jobs))
        return common_failure("ItemCraftSelectedRootMismatch");
    // Check selected gates even when the malformed head uses a wrong domain.
    // Each domain owner performs the actual unique row/header proof.
    for(const auto& row:common.state().facts.rows()){
        auto schema=read_native_item_craft_original_fact_schema(common,catalog,items,jobs,row.second);
        if(!schema.ok)return {false,{},std::move(schema.error)};
    }
    CheckedOriginalHeaders sink;
    if(jobs){auto checked=visit_craft_original_fact_headers(common,*jobs,sink);if(!checked.ok)return checked;}
    if(selected.value.item_instances==3){
        auto checked=visit_repair_original_fact_headers(common,*items,sink);if(!checked.ok)return checked;
        checked=visit_repair_service_original_fact_headers(common,*items,sink);if(!checked.ok)return checked;
    }
    return common_success();
}
Status emit_native_item_craft_fact_payloads_fields(ProjectionWriter& writer,
    const CommonStore& common,const PreparedCatalog& catalog,const ItemInstancesState* items,
    const CraftJobsState* jobs,NativeItemCraftFactProjection part){
    if(part!=NativeItemCraftFactProjection::all&&part!=NativeItemCraftFactProjection::craft&&
       part!=NativeItemCraftFactProjection::repair)return common_failure("UnknownItemCraftFactProjection");
    auto checked=validate_native_item_craft_original_headers(common,catalog,items,jobs);
    if(!checked.ok)return checked;
    for(const auto& row:common.state().facts.rows()){
        const auto family=original_fact_family(row.second);
        if(family==OriginalItemCraftFact::other)continue;
        if(part==NativeItemCraftFactProjection::craft&&family!=OriginalItemCraftFact::craft)continue;
        if(part==NativeItemCraftFactProjection::repair&&family==OriginalItemCraftFact::craft)continue;
        const auto& header=row.second;
        auto emitted=writer.object(row.first,header.field_order,[&](auto& fields)->Status{
            Status payload=family==OriginalItemCraftFact::craft?
                emit_craft_original_fact_payload_fields(fields,common,*jobs,header):
                family==OriginalItemCraftFact::repair?
                emit_repair_original_fact_payload_fields(fields,common,*items,header):
                emit_repair_service_original_payload_fields(fields,common,*items,header);
            if(!payload.ok)return payload;
            fields.field("kind",header.kind);return fields.field("domain",header.domain);
        });
        if(!emitted.ok)return emitted;
    }
    return writer.status();
}
Status validate_native_item_craft_original_actions(const CommonStore& common,
    const PreparedCatalog& catalog,const ItemInstancesState* items,const CraftJobsState* jobs){
    auto selected=selected_native_item_craft_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value.item_instances)!=bool(items)||bool(selected.value.craft_jobs)!=bool(jobs))return common_failure("ItemCraftSelectedRootMismatch");
    for(const auto& row:common.state().actions.rows()){
        if(row.second.kind=="craft_job"){
            if(!selected.value.craft_jobs||!jobs)return common_failure("CraftOriginalSelectedOwnerRequired",row.first);
            auto original=read_craft_original_action_owner_bootstrap(common,*jobs,row.first);if(!original.ok)return {false,{},std::move(original.error)};
        }else if(row.second.kind=="repair"){
            if(selected.value.item_instances!=3||!items)return common_failure("RepairOriginalSelectedOwnerRequired",row.first);
            auto original=read_repair_original_action_owner_bootstrap(common,*items,row.first);if(!original.ok)return {false,{},std::move(original.error)};
        }
    }
    if(jobs){auto checked=validate_craft_original_action_coverage_bootstrap(common,*jobs);if(!checked.ok)return checked;}
    if(selected.value.item_instances==3){auto checked=validate_repair_original_action_coverage_bootstrap(common,*items);if(!checked.ok)return checked;}
    return common_success();
}
Status emit_native_item_craft_action_payloads_fields(ProjectionWriter& out,const CommonStore& common,
    const PreparedCatalog& catalog,const ItemInstancesState* items,const CraftJobsState* jobs,
    NativeItemCraftFactProjection part){
    auto checked=validate_native_item_craft_original_actions(common,catalog,items,jobs);if(!checked.ok)return checked;
    for(const auto& row:common.state().actions.rows()){
        const bool craft=row.second.kind=="craft_job",repair=row.second.kind=="repair";
        if((craft&&part!=NativeItemCraftFactProjection::repair)||(repair&&part!=NativeItemCraftFactProjection::craft)){
            auto emitted=out.object(row.first,[](auto& empty){return empty.status();});if(!emitted.ok)return emitted;
        }
    }
    return out.status();
}

}
