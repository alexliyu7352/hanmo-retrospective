#include "resident/kernel/catalog_section_observation.hpp"
#include "native_organization_runtime.hpp"
#include "world_bundle.hpp"
#include "native_economic_obligation_sources.hpp"
#include "catalog_copy_plan.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/organization/organization_runtime.hpp"
#include "resident/domains/organization/organization_authority_codec.hpp"
#include "resident/domains/organization/organization_service_effects_codec.hpp"
#include "resident/domains/organization/organization_lifecycle_catalog.hpp"
#include "resident/checkpoint/checkpoint_json_decode.hpp"
#include "resident/checkpoint/checkpoint_catalog_json_kind.hpp"
namespace hanmo::resident {namespace {
const BoundaryValue* runtime_block(const BoundaryValue& scenario){
 const BoundaryValue* found=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="organization_runtime"){
  if(found)boundary::fail("DuplicateOrganizationRuntimeSelection","organization_runtime");found=&row.second;
 }
 return found;
}
bool authored_service_effects_selected(const BoundaryValue& scenario){
 const BoundaryValue* found=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="organization_service_effects"){
  if(found)boundary::fail("DuplicateOrganizationServiceEffectsSelection","organization_service_effects");found=&row.second;
 }
 if(!found)return false;
 boundary::fields(*found,{"profile"},{},"organization_service_effects");
 if(boundary::object(*found,"organization_service_effects").size()!=1||
    boundary::get_string_ref(*found,"profile","organization_service_effects")!="original_organization_service_effects_v1")
  boundary::fail("UnsupportedOrganizationServiceEffectsSelection","organization_service_effects");
 return true;
}
}
Outcome<WorldMoment> NativeOrganizationAuthorityClock::read_organization_authority_now()const{
 return world_.actual_phase_moment();
}
Outcome<NativeOrganizationRuntimeSelection> read_native_organization_runtime_selection(const BoundaryValue& scenario,bool named){try{
 const bool effects=authored_service_effects_selected(scenario);
 const auto* block=runtime_block(scenario);if(!block){if(effects)return {false,{},common_failure("OrganizationServiceEffectsExactBodyRequired").error};return {true,{named?3:2,false},{}};}
 boundary::fields(*block,{"profile","organization_version","lifecycle_catalog"},{},"organization_runtime");
 if(boundary::object(*block,"organization_runtime").size()!=3||boundary::get_string_ref(*block,"profile","organization_runtime")!="original_organization_runtime_v1")
  return {false,{},common_failure("UnsupportedOrganizationRuntimeSelection",{},ErrorKind::unsupported_capability).error};
 const auto version=boundary::get_int(*block,"organization_version","organization_runtime");
 if(version<4||version>7||named!=(version==5||version==7))return {false,{},common_failure("OrganizationRuntimePhysicalBodyMismatch").error};
 if(effects&&version<6)return {false,{},common_failure("OrganizationServiceEffectsExactBodyRequired").error};
 return {true,{version,true},{}};
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Outcome<Integer> selected_native_organization_runtime(const CatalogIdentity& identity){
 bool selected=false;for(const auto& row:identity.module_versions)if(row.first=="organization_runtime"){
  if(selected||row.second!="1")return {false,0,common_failure("OrganizationRuntimeModuleMismatch").error};selected=true;
 }
 if(!selected)return {true,0,{}};
 auto original=select_organization_runtime_codec(identity);if(!original.ok)return {false,0,std::move(original.error)};
 return {true,1,{}};
}
Outcome<Integer> selected_native_organization_service_effects(const CatalogIdentity& identity){
 for(const auto& module:identity.module_versions)if(module.first=="organization_service_effects"){
  // The original domain selector owns duplicate/version/body pairing. Row
  // presence never enables this physical adjunct.
  auto selected=select_organization_runtime_codec(identity);if(!selected.ok)return {false,0,std::move(selected.error)};
  return {true,selected.value.service_effects_schema,{}};
 }
 return {true,0,{}};
}
Status append_native_organization_runtime_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
 for(const auto& row:identity.module_versions){if(row.first=="organization_runtime")return common_failure("DuplicateOrganizationRuntimeModule");if(row.first=="organization_service_effects")return common_failure("DuplicateOrganizationServiceEffectsModule");}
 StableId* original=nullptr;for(auto& row:identity.module_versions)if(row.first=="organization"){
  if(original)return common_failure("DuplicateOrganizationPhysicalModule");original=&row.second;
 }
 const bool effects=authored_service_effects_selected(scenario);
 const auto* block=runtime_block(scenario);if(!block)return effects?common_failure("OrganizationServiceEffectsExactBodyRequired"):common_success();
 if(!original||(*original!="2"&&*original!="3"))return common_failure("OrganizationRuntimeOriginalBodyRequired");
 auto selected=read_native_organization_runtime_selection(scenario,*original=="3");if(!selected.ok)return {false,{},std::move(selected.error)};
 *original=std::to_string(selected.value.body_version);identity.module_versions.emplace_back("organization_runtime","1");
 if(effects)identity.module_versions.emplace_back("organization_service_effects","1");
 return common_success();
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Outcome<OrganizationCatalog> decode_native_organization_catalog(const BoundaryValue& scenario,const BoundaryValue& body,const CatalogIdentity& identity,bool named){
 auto selected=read_native_organization_runtime_selection(scenario,named);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!selected.value.selected)return decode_organization_catalog(body,named);
 ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::organization_authority);
 auto decoded=selected.value.body_version>=6?decode_organization_catalog_v6(body,identity,named):decode_organization_catalog_v4(body,identity,named);
 if(decoded.ok)decoded.value.service_effects_enabled=authored_service_effects_selected(scenario);
 return decoded;
}
Status validate_native_organization_runtime_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 147, "PreparedCatalog", "validate_native_organization_runtime_catalog", &catalog);

 auto selected=selected_native_organization_runtime(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 auto effects=selected_native_organization_service_effects(catalog.identity);if(!effects.ok)return {false,{},std::move(effects.error)};
 if((catalog.organization&&catalog.organization->service_effects_enabled)!=(effects.value==1))return common_failure("OrganizationServiceEffectsSelectedCatalogMismatch");
 if(bool(selected.value)!=bool(catalog.organization_lifecycle))return common_failure("OrganizationRuntimeSelectedCatalogMismatch");
 if(!selected.value)return common_success();
 auto obligations=selected_native_economic_obligation_versions(catalog.identity);if(!obligations.ok)return {false,{},std::move(obligations.error)};if(obligations.value.sources!=1)return common_failure("OrganizationRuntimeActualObligationRootRequired");
 if(catalog.dynamic_profile!=DynamicProfile::local_society_v1||!catalog.organization||!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))return common_failure("OrganizationRuntimeCompleteOriginalCatalogsRequired",{},ErrorKind::unsupported_capability);
 auto valid=validate_organization_runtime_codec_selection(catalog.identity,*catalog.organization);if(!valid.ok)return valid;
 return validate_organization_lifecycle_catalog(*catalog.organization_lifecycle,*catalog.organization);
}
Status prepare_native_organization_runtime_catalog(const BoundaryValue& scenario,PreparedCatalog& catalog){try{
 auto selected=selected_native_organization_runtime(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 auto effects=selected_native_organization_service_effects(catalog.identity);if(!effects.ok)return {false,{},std::move(effects.error)};
 if(authored_service_effects_selected(scenario)!=(effects.value==1))return common_failure("OrganizationServiceEffectsSelectedCatalogMismatch");
 const auto* block=runtime_block(scenario);if(bool(block)!=bool(selected.value)||catalog.organization_lifecycle)return common_failure("OrganizationRuntimeSelectedCatalogMismatch");
 if(!block)return common_success();
 if(!catalog.organization)return common_failure("OrganizationRuntimeOriginalBodyRequired");
 auto authored=read_native_organization_runtime_selection(scenario,catalog.named_membership);if(!authored.ok)return {false,{},std::move(authored.error)};
 auto physical=select_organization_runtime_codec(catalog.identity);if(!physical.ok)return {false,{},std::move(physical.error)};
 if(authored.value.body_version!=physical.value.organization_version)return common_failure("OrganizationRuntimePhysicalBodyMismatch");
 // One immutable lock through the original copied-catalog lifetime owner. The
 // provisional decoder identity is not a separate authority root.
 catalog.organization=copy_locked_native_catalog(*catalog.organization,catalog.identity);
 ScopedCheckpointJsonDecode original(CheckpointCatalogJsonKind::organization_lifecycle);
 auto lifecycle=decode_authored_organization_lifecycle_catalog(boundary::field(*block,"lifecycle_catalog","organization_runtime"),catalog.identity,*catalog.organization);
 if(!lifecycle.ok)return {false,{},std::move(lifecycle.error)};
 catalog.organization_lifecycle=checkpoint_make_shared<const OrganizationLifecycleCatalog>(std::move(lifecycle.value));
 return validate_native_organization_runtime_catalog(catalog);
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Status validate_native_organization_runtime_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("OrganizationRuntimeCatalogRequired",{},ErrorKind::internal);
 auto valid=validate_native_organization_runtime_catalog(*world.catalog);if(!valid.ok)return valid;
 auto selected=selected_native_organization_runtime(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(bool(selected.value)!=bool(world.organization_runtime))return common_failure("OrganizationRuntimeSelectedRootMismatch");
 auto effects=selected_native_organization_service_effects(world.catalog->identity);if(!effects.ok)return {false,{},std::move(effects.error)};
 return validate_organization_service_effects(world.organization,effects.value);
}
}
