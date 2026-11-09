#include "resident/kernel/catalog_section_observation.hpp"
#include "native_economic_obligation_sources.hpp"
#include "world_bundle.hpp"
#include "resident/storage/common_codec.hpp"
#include "resident/domains/local_economy/obligations/obligation_sources_codec.hpp"
namespace hanmo::resident {namespace {
const BoundaryValue* selected_block(const BoundaryValue& scenario){
 const BoundaryValue* result=nullptr;
 for(const auto& row:boundary::object(scenario,"scenario"))if(row.first=="economic_obligation_sources"){
  if(result)boundary::fail("DuplicateEconomicObligationSelection","economic_obligation_sources");result=&row.second;
 }
 return result;
}
Integer finite_version(const StableId& value){
 if(value=="1")return 1;if(value=="2")return 2;if(value=="3")return 3;if(value=="4")return 4;if(value=="5")return 5;
 if(value=="6")return 6;if(value=="7")return 7;if(value=="8")return 8;if(value=="9")return 9;if(value=="10")return 10;if(value=="11")return 11;if(value=="12")return 12;if(value=="13")return 13;if(value=="14")return 14;if(value=="15")return 15;if(value=="16")return 16;if(value=="17")return 17;if(value=="18")return 18;if(value=="19")return 19;if(value=="20")return 20;return 0;
}
}
Outcome<NativeEconomicObligationVersions> selected_native_economic_obligation_versions(const CatalogIdentity& identity){
 NativeEconomicObligationVersions result;
 for(const auto& row:identity.module_versions){
  if(row.first=="local_economy"){
   if(result.local_economy)return {false,{},common_failure("DuplicateLocalEconomyModule").error};
   result.local_economy=finite_version(row.second);if(!result.local_economy)return {false,{},common_failure("UnsupportedLocalEconomyCodecVersion").error};
  }else if(row.first=="named_birth_population_sources"){
   if(result.named_birth_population||row.second!="1")return {false,{},common_failure("NamedBirthPopulationModuleMismatch").error};result.named_birth_population=1;
  }else if(row.first=="economic_obligation_sources"){
   if(result.sources||row.second!="1")return {false,{},common_failure("EconomicObligationModuleMismatch").error};result.sources=1;
  }
 }
 if(bool(result.named_birth_population)!=(result.local_economy>=11))return {false,{},common_failure("NamedBirthPopulationLocalEconomyVersionPairRequired").error};
 const auto original=result.local_economy>=11?result.local_economy-10:result.local_economy;
 if(bool(result.sources)!=(original>=6))return {false,{},common_failure("EconomicObligationLocalEconomyVersionPairRequired").error};
 return {true,result,{}};
}
Outcome<Integer> read_native_economic_obligation_selection(const BoundaryValue& scenario,Integer base){try{
 if(base<1||base>5)return {false,0,common_failure("EconomicObligationOriginalBaseRequired").error};
 const auto* block=selected_block(scenario);if(!block)return {true,base,{}};
 boundary::fields(*block,{"profile"},{},"economic_obligation_sources");
 if(boundary::object(*block,"economic_obligation_sources").size()!=1||boundary::get_string_ref(*block,"profile","economic_obligation_sources")!="original_economic_obligation_sources_v1")return {false,0,common_failure("UnsupportedEconomicObligationSelection",{},ErrorKind::unsupported_capability).error};
 return {true,base+5,{}};
}catch(const boundary::DecodeFailure& failure){return {false,0,failure.error};}}
Status append_native_economic_obligation_module(const BoundaryValue& scenario,CatalogIdentity& identity){try{
 const auto* block=selected_block(scenario);if(!block)return common_success();
 StableId* base=nullptr;
 for(auto& row:identity.module_versions){if(row.first=="economic_obligation_sources")return common_failure("DuplicateEconomicObligationModule");if(row.first=="local_economy"){if(base)return common_failure("DuplicateLocalEconomyModule");base=&row.second;}}
 if(!base)return common_failure("EconomicObligationOriginalEconomyRequired");const auto version=finite_version(*base);
 auto selected=read_native_economic_obligation_selection(scenario,version);if(!selected.ok)return {false,{},std::move(selected.error)};
 *base=std::to_string(selected.value);identity.module_versions.emplace_back("economic_obligation_sources","1");return common_success();
}catch(const boundary::DecodeFailure& failure){return {false,{},failure.error};}}
Status validate_native_economic_obligation_catalog(const PreparedCatalog& catalog){
  // 只旁观本次原入口；返回值与首错仍由原函数独占。
  ::hanmo::resident::catalog_section_observation::Scope catalog_cost_observation(
      ::hanmo::resident::catalog_section_observation::Kind::catalog_validation, 132, "PreparedCatalog", "validate_native_economic_obligation_catalog", &catalog);

 auto selected=selected_native_economic_obligation_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(!selected.value.sources)return common_success();
 if(catalog.dynamic_profile!=DynamicProfile::local_society_v1||!catalog.local_economy||!catalog.exact_time||!exact_complete_selected(*catalog.exact_time))return common_failure("EconomicObligationCompleteOriginalCatalogsRequired",{},ErrorKind::unsupported_capability);
 const bool military=catalog.population_military&&catalog.population_military->enabled;
 const bool management=catalog.local_economy->production_management&&catalog.local_economy->production_management->version==1;
 const bool household=catalog.local_economy->household_provisioning&&catalog.local_economy->household_provisioning->version==1;
 return validate_local_economy_codec_selection(selected.value.local_economy,military,catalog.named_membership,management,household);
}
Status initialize_native_economic_obligation_root(const PreparedCatalog& catalog,InitialWorldSeed& seed){
 auto valid=validate_native_economic_obligation_catalog(catalog);if(!valid.ok)return valid;
 auto selected=selected_native_economic_obligation_versions(catalog.identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(seed.economic_obligation_sources)return common_failure("EconomicObligationFreshRootRequired");if(!selected.value.sources)return common_success();
 auto state=checkpoint_make_shared<EconomicObligationSourcesState>();valid=initialize_economic_obligation_sources(*state,selected.value.sources);if(!valid.ok)return valid;
 seed.economic_obligation_sources=std::move(state);return common_success();
}
Status validate_native_economic_obligation_layout(const WorldBundle& world){
 if(!world.catalog)return common_failure("EconomicObligationCatalogRequired",{},ErrorKind::internal);
 auto valid=validate_native_economic_obligation_catalog(*world.catalog);if(!valid.ok)return valid;
 auto selected=selected_native_economic_obligation_versions(world.catalog->identity);if(!selected.ok)return {false,{},std::move(selected.error)};
 if(bool(selected.value.sources)!=bool(world.economic_obligation_sources))return common_failure("EconomicObligationSelectedRootMismatch");
 return common_success();
}
}
