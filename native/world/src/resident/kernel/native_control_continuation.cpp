#include "resident/kernel/catalog_section_observation.hpp"
#include "native_control_continuation.hpp"
#include "world_bundle.hpp"
#include "exact_complete_world.hpp"
#include "resident/domains/family/native_continuation_sources.hpp"
#include "hanmo/resident/checkpoint_memory.hpp"
namespace hanmo::resident {
Outcome<Integer> selected_native_control_continuation_version(const CatalogIdentity& identity){
    Integer selected=0;
    for(const auto& module:identity.module_versions)if(module.first=="control_continuation"){
        if(selected||(module.second!="2"&&module.second!="3"))
            return {false,0,common_failure("ControlContinuationModuleMismatch",module.first).error};
        selected=module.second=="2"?2:3;
    }
    return {true,selected,{}};
}
namespace {
const BoundaryValue* authored(const BoundaryValue& scenario){
    const BoundaryValue* result=nullptr;
    for(const auto& field:boundary::object(scenario,"scenario"))if(field.first=="control_continuation"){
        if(result)boundary::fail("DuplicateControlContinuationSelection","control_continuation");
        result=&field.second;
    }
    return result;
}
}
Status append_native_control_continuation_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
    auto selected=selected_native_control_continuation_version(identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(selected.value)return common_failure("DuplicateControlContinuationModule");
    const auto* value=authored(scenario);if(!value)return common_success();
    auto original=decode_control_continuation_catalog(*value);if(!original.ok)return {false,{},std::move(original.error)};
    identity.module_versions.emplace_back("control_continuation",std::to_string(original.value.selected_version));
    return validate_control_continuation_catalog(original.value,identity);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_native_control_continuation_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 129, "PreparedCatalog", "validate_native_control_continuation_catalog", &catalog);

    auto selected=selected_native_control_continuation_version(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
    if(bool(selected.value)!=bool(catalog.control_continuation))
        return common_failure("ControlContinuationSelectedCatalogMismatch");
    if(!selected.value)return common_success();
    auto original=validate_control_continuation_catalog(*catalog.control_continuation,catalog.identity);if(!original.ok)return original;
    if(!catalog.exact_time||!exact_complete_selected(*catalog.exact_time)||!catalog.actor_foundation||
       !catalog.family_life||catalog.family_life->selected_version!=4||!catalog.family||
       !catalog.common||!catalog.interaction||!catalog.family_legacy)
        return common_failure("ControlContinuationOriginalCatalogsRequired",{},ErrorKind::unsupported_capability);
    return common_success();
}
Status prepare_native_control_continuation_catalog(const BoundaryValue& scenario,PreparedCatalog& catalog){try{
    if(catalog.control_continuation)return common_failure("ControlContinuationCatalogAlreadyPrepared",{},ErrorKind::internal);
    if(const auto* value=authored(scenario)){
        auto original=decode_control_continuation_catalog(*value);if(!original.ok)return {false,{},std::move(original.error)};
        catalog.control_continuation=checkpoint_make_shared<const ControlContinuationCatalog>(std::move(original.value));
    }
    return validate_native_control_continuation_catalog(catalog);
}catch(const boundary::DecodeFailure& error){return {false,{},error.error};}}
Status validate_native_control_continuation_layout(const WorldBundle& world){
    if(!world.catalog)return common_failure("ControlContinuationCatalogRequired",{},ErrorKind::internal);
    auto valid=validate_native_control_continuation_catalog(*world.catalog);if(!valid.ok)return valid;
    if(bool(world.catalog->control_continuation)!=bool(world.control_continuation))
        return common_failure("ControlContinuationSelectedStatePairRequired");
    return common_success();
}
Status validate_native_control_continuation_world(const WorldBundle& world){
    auto valid=validate_native_control_continuation_layout(world);if(!valid.ok||!world.control_continuation)return valid;
    auto complete=world.complete_exact_time_bindings();if(!complete.ok)return {false,{},std::move(complete.error)};
    const auto* original=complete.value?complete.value->reads().continuation_sources:nullptr;
    if(!original||!original->bound_to(world.common,world.family,*world.control_continuation))
        return common_failure("ControlContinuationSameOriginalOwnerRequired",{},ErrorKind::internal);
    const auto& catalog=*world.catalog->control_continuation;
    return catalog.selected_version==3?
        validate_control_continuation_v3(world.common,*world.control_continuation,catalog,world.catalog->identity,*original):
        validate_control_continuation_v2(world.common,*world.control_continuation,catalog,world.catalog->identity,*original);
}
}
